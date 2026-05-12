// Deterministic mock engine used by the API server's `--mock` mode.
//
// Real inference goes through OmniEngine, which needs CUDA / Vulkan /
// llama.cpp / a GGUF model file. The mock engine produces a canned
// response pattern that lets the API server be exercised end-to-end on
// any host, including the CPU-only build path. It's also what the
// curl-based round-trip tests in tests/api/ run against.
#pragma once

#include <string>

#include "api/chat_request.h"

namespace omni::api {

class MockEngine {
public:
    // Run the request synchronously. Always succeeds; ignores temperature /
    // top_p / etc. Echoes the request shape so tests can assert on it
    // (token counts, tool-call presence, etc.).
    static ChatResponse Run(const ChatRequest& request);

    // Stream the response chunk-by-chunk. Each text chunk corresponds to a
    // single token in the canned reply. Tool calls are emitted in the same
    // order the request lists them.
    static void Stream(const ChatRequest& request, const StreamCallback& cb);
};

}  // namespace omni::api
