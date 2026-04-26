#include "gui_app.h"
#include <iostream>

int main(int argc, char* argv[]) {
    GUIApp app;
    
    if (!app.Initialize(1280, 720)) {
        std::cerr << "Failed to initialize GUI application" << std::endl;
        return 1;
    }
    
    std::cout << "Starting OmniInference GUI..." << std::endl;
    app.Run();
    app.Shutdown();
    
    return 0;
}