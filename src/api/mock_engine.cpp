#include "api/mock_engine.h"

#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <string>

namespace omni::api {

namespace {

// Crude token estimate: ~4 chars per token. Matches the heuristic the rest of
// the codebase uses; good enough for round-trip tests that only assert shape.
int EstimateTokens(const std::string& s) {
    if (s.empty()) return 0;
    return static_cast<int>((s.size() + 3) / 4);
}

int CountPromptTokens(const ChatRequest& req) {
    int total = EstimateTokens(req.params.system_prompt);
    for (const auto& msg : req.messages) {
        for (const auto& part : msg.content) {
            if (part.kind == ContentPart::Kind::kText) {
                total += EstimateTokens(part.text);
            } else if (part.kind == ContentPart::Kind::kToolResult) {
                total += EstimateTokens(part.text);
            }
        }
    }
    return total;
}

// Build the canned response. If the caller supplied tools, we pretend the
// model wants to invoke the first one with the message as its argument; this
// lets the round-trip tests exercise the tool-call path.
struct PreparedReply {
    std::string text;
    std::vector<ToolCall> tool_calls;
    FinishReason finish_reason = FinishReason::kStop;
};

std::string LastUserUtterance(const ChatRequest& req) {
    for (auto it = req.messages.rbegin(); it != req.messages.rend(); ++it) {
        if (it->role == Role::kUser) {
            std::string acc;
            for (const auto& part : it->content) {
                if (part.kind == ContentPart::Kind::kText) {
                    acc.append(part.text);
                }
            }
            if (!acc.empty()) return acc;
        }
    }
    return "(no user message)";
}

std::string GenerateMockId(const char* prefix) {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::ostringstream os;
    os << prefix << "-" << ms << "-" << counter.fetch_add(1, std::memory_order_relaxed);
    return os.str();
}

PreparedReply Prepare(const ChatRequest& req) {
    PreparedReply out;
    if (!req.tools.empty()) {
        ToolCall call;
        call.id   = GenerateMockId("call");
        call.name = req.tools.front().name;
        // Echo the last user message as the argument under a stable key so
        // tests can assert the tool received the expected payload.
        call.arguments = nlohmann::json{{"echo", LastUserUtterance(req)}};
        out.tool_calls.push_back(std::move(call));
        out.text = "";
        out.finish_reason = FinishReason::kToolCalls;
    } else {
        out.text =
            "[mock] Hello from OmniInference. I received: " + LastUserUtterance(req);
        out.finish_reason = FinishReason::kStop;
    }
    return out;
}

std::vector<std::string> SplitWords(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        cur.push_back(c);
        if (c == ' ') {
            out.push_back(std::move(cur));
            cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}

}  // namespace

ChatResponse MockEngine::Run(const ChatRequest& request) {
    auto reply = Prepare(request);
    ChatResponse resp;
    resp.id                = GenerateMockId("chatcmpl");
    resp.model             = request.model.empty() ? "omni-mock" : request.model;
    resp.content           = reply.text;
    resp.tool_calls        = std::move(reply.tool_calls);
    resp.finish_reason     = reply.finish_reason;
    resp.prompt_tokens     = CountPromptTokens(request);
    resp.completion_tokens = EstimateTokens(resp.content);
    for (const auto& tc : resp.tool_calls) {
        resp.completion_tokens += EstimateTokens(tc.arguments.dump());
    }
    return resp;
}

void MockEngine::Stream(const ChatRequest& request, const StreamCallback& cb) {
    auto reply = Prepare(request);

    // Emit the start chunk so OpenAI clients see the assistant role early.
    {
        StreamChunk chunk;
        chunk.kind = StreamChunk::Kind::kStart;
        cb(chunk);
    }

    if (!reply.tool_calls.empty()) {
        // Emit one tool call: a start chunk, then a single arguments-delta
        // carrying the full JSON. Real engines would emit incremental
        // fragments; clients accept either shape.
        for (size_t i = 0; i < reply.tool_calls.size(); ++i) {
            const auto& call = reply.tool_calls[i];
            StreamChunk start;
            start.kind            = StreamChunk::Kind::kToolCallStart;
            start.tool_call_index = static_cast<int>(i);
            start.tool_call_id    = call.id;
            start.tool_call_name  = call.name;
            cb(start);

            StreamChunk args;
            args.kind                       = StreamChunk::Kind::kToolCallArgumentsDelta;
            args.tool_call_index            = static_cast<int>(i);
            args.tool_call_arguments_delta  = call.arguments.dump();
            cb(args);
        }
    } else {
        for (const auto& word : SplitWords(reply.text)) {
            StreamChunk chunk;
            chunk.kind       = StreamChunk::Kind::kTextDelta;
            chunk.text_delta = word;
            cb(chunk);
        }
    }

    // Final chunk with the finish reason + usage.
    StreamChunk fin;
    fin.kind          = StreamChunk::Kind::kFinish;
    fin.finish_reason = reply.finish_reason;
    fin.prompt_tokens = CountPromptTokens(request);
    fin.completion_tokens = EstimateTokens(reply.text);
    for (const auto& tc : reply.tool_calls) {
        fin.completion_tokens += EstimateTokens(tc.arguments.dump());
    }
    cb(fin);
}

}  // namespace omni::api
