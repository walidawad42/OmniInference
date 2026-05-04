#include "gui_imgui_compat.h"
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL2/SDL.h>
#if defined(_WIN32)
    #include <windows.h>
    #include <GL/gl.h>
#elif defined(__APPLE__)
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif
#include <iostream>
#include "gui_visual_control.h"
#include "gui_memory_buffer_panel.h"
#include "gui_integration_panel.h"
#include "gui_turboquant_panel.h"
#include "omni_engine.h"
#include "memory_manager.h"

int main(int argc, char* argv[]) {
    // Setup window
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "OmniInference v2.0.0 - Full Production GUI",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1920, 1080,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED
    );

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    // ImGui's OpenGL3 backend (with IMGUI_IMPL_OPENGL_LOADER_CUSTOM=0 by
    // default) loads its own GL function pointers via the system loader, so
    // we no longer need an explicit gl3wInit() call. Just verify we managed
    // to obtain a context.
    if (!gl_context) {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        return 1;
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 430");

    // Initialize OmniEngine
    OmniEngine engine;
    if (!engine.InitializeBackend()) {
        std::cerr << "Failed to initialize OmniEngine" << std::endl;
        return 1;
    }

    engine.PrintHardwareReport();

    // Initialize Memory Manager
    MemoryManager memory_manager;
    MemoryPolicy mem_policy;
    mem_policy.vram_buffer_size = 1024 * 1024 * 1024; // 1GB
    mem_policy.enable_disk_swap = true;
    memory_manager.Initialize(mem_policy);

    // Initialize GUI panels
    GUIVisualControl visual_control;
    visual_control.SetEngine(&engine);

    GUIMemoryBufferPanel memory_panel;
    memory_panel.SetMemoryManager(&memory_manager);

    GUIIntegrationPanel integration_panel;

    GUITurboQuantPanel turboquant_panel;
    // turboquant_panel.SetPipeline(...); // TODO: Add pipeline

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        // Render all panels
        visual_control.RenderFullControlPanel();
        memory_panel.Render();
        integration_panel.Render();
        turboquant_panel.Render();

        ImGui::Render();

        glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}