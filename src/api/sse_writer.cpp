#include "api/sse_writer.h"

#include <string>

namespace omni::api::sse {

namespace {

bool WriteAll(const SinkFn& sink, const std::string& s) {
    return sink(s.data(), s.size());
}

}  // namespace

bool EmitOpenAI(SinkFn sink, const nlohmann::json& chunk) {
    std::string buf;
    buf.reserve(64 + 32);
    buf.append("data: ");
    buf.append(chunk.dump());
    buf.append("\n\n");
    return WriteAll(sink, buf);
}

bool EmitOpenAIDone(SinkFn sink) {
    static const std::string kDone = "data: [DONE]\n\n";
    return WriteAll(sink, kDone);
}

bool EmitAnthropic(SinkFn sink, const std::string& event_name, const nlohmann::json& data) {
    std::string buf;
    buf.reserve(64 + event_name.size());
    buf.append("event: ");
    buf.append(event_name);
    buf.append("\n");
    buf.append("data: ");
    buf.append(data.dump());
    buf.append("\n\n");
    return WriteAll(sink, buf);
}

}  // namespace omni::api::sse
