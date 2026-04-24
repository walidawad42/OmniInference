#include "gui_main.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "=== OmniInference: LLM/MLLM Master Control ===" << std::endl;
    std::cout << "Initializing application..." << std::endl;

    GUIMainWindow app;

    if (!app.Initialize(1600, 900)) {
        std::cerr << "FATAL: Application initialization failed" << std::endl;
        return 1;
    }

    app.Run();
    app.Shutdown();

    return 0;
}