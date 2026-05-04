// OmniServer: headless OpenAI- + Anthropic-compatible API server entrypoint.
//
// Examples:
//   OmniServer                               # listen on :8080, real engine
//   OmniServer --mock                        # listen on :8080, deterministic mock
//   OmniServer --port 11434 --api-key sekret # custom port + bearer auth
//
// Run `OmniServer --help` for the full flag list.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include "omni_server.h"

namespace {

std::atomic<bool> g_should_stop{false};

extern "C" void HandleSignal(int) {
    g_should_stop.store(true);
}

void PrintUsage(const char* argv0) {
    std::cout <<
        "Usage: " << argv0 << " [options]\n"
        "\n"
        "Options:\n"
        "  --port <n>            TCP port to listen on (default: 8080)\n"
        "  --api-key <key>       Require Authorization: Bearer <key> on every request\n"
        "  --mock                Use the deterministic mock engine instead of OmniEngine\n"
        "  --cors-origin <orig>  Origin allowed in CORS headers (default: *)\n"
        "  --default-model <id>  Logical model id reported by /v1/models (default: omni-inference-mock)\n"
        "  -h, --help            Show this help and exit\n"
        "\n"
        "Endpoints:\n"
        "  GET  /health\n"
        "  GET  /v1/models\n"
        "  POST /v1/chat/completions    (streaming + non-streaming + tools)\n"
        "  POST /v1/completions\n"
        "  POST /v1/embeddings\n"
        "  POST /v1/messages            (Anthropic, streaming + non-streaming + tools)\n"
        "  POST /v1/models/load\n"
        "  POST /tool/call\n"
        "  GET  /quantization/status\n";
}

bool MatchFlag(const char* arg, const char* flag) {
    return std::strcmp(arg, flag) == 0;
}

// Pull the value for a `--flag <value>` pair. Bumps `i` past the value on
// success; on missing argument, prints an error and returns false.
bool TakeValue(int argc, char** argv, int& i, const char* flag, std::string& out) {
    if (i + 1 >= argc) {
        std::cerr << "error: " << flag << " requires a value\n";
        return false;
    }
    out = argv[++i];
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    OmniServerConfig cfg;
    cfg.port = 8080;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (MatchFlag(arg, "-h") || MatchFlag(arg, "--help")) {
            PrintUsage(argv[0]);
            return 0;
        } else if (MatchFlag(arg, "--mock")) {
            cfg.mock_mode = true;
        } else if (MatchFlag(arg, "--port")) {
            std::string v;
            if (!TakeValue(argc, argv, i, "--port", v)) return 2;
            try {
                cfg.port = std::stoi(v);
            } catch (...) {
                std::cerr << "error: --port expects an integer (got '" << v << "')\n";
                return 2;
            }
        } else if (MatchFlag(arg, "--api-key")) {
            if (!TakeValue(argc, argv, i, "--api-key", cfg.api_key)) return 2;
        } else if (MatchFlag(arg, "--cors-origin")) {
            if (!TakeValue(argc, argv, i, "--cors-origin", cfg.cors_origin)) return 2;
        } else if (MatchFlag(arg, "--default-model")) {
            if (!TakeValue(argc, argv, i, "--default-model", cfg.default_model_id)) return 2;
        } else {
            std::cerr << "error: unknown argument '" << arg << "' (try --help)\n";
            return 2;
        }
    }

    std::signal(SIGINT,  HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    OmniServer server(cfg);
    if (!server.Start()) {
        std::cerr << "OmniServer failed to start.\n";
        return 1;
    }

    std::cout << "OmniServer ready. Press Ctrl+C to stop." << std::endl;
    while (!g_should_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    server.Stop();
    return 0;
}
