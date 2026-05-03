cmake_minimum_required(VERSION 3.20)
project(OmniInference VERSION 2.0.0 LANGUAGES C CXX CUDA)

# ============================================================
# CRITICAL: CUDA 11.4 MINIMUM REQUIREMENT
# Supports: 11.4, 11.5, 11.6, 11.7, 11.8, 12.0, 12.1, 12.2, 12.3, 13.0+
# ============================================================

set(MINIMUM_CUDA_VERSION "11.4" CACHE STRING "Minimum CUDA version required")

# ============================================================
# PLATFORM & COMPILER SETUP
# ============================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cross-platform setup
if(WIN32)
    message(STATUS "[OmniInference] Building for Windows 10/11")
    add_compile_options(/W4 /WX /permissive-)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    message(STATUS "[OmniInference] Building for Linux (Ubuntu/Pop!_OS)")
    add_compile_options(-Wall -Wextra -Werror -fPIC)
endif()

# ============================================================
# CUDA CONFIGURATION (11.4 MINIMUM)
# ============================================================

message(STATUS "[CUDA] Minimum requirement: ${MINIMUM_CUDA_VERSION}")
message(STATUS "[CUDA] Supported architectures: sm_30 (Kepler) through sm_90 (Hopper)")

find_package(CUDAToolkit 11.4 REQUIRED)

message(STATUS "[CUDA] Found CUDA: ${CUDAToolkit_VERSION}")
message(STATUS "[CUDA] CUDA Runtime: ${CUDAToolkit_LIBRARY}")

# GPU Architectures: Kepler (30) to Hopper (90)
set(CMAKE_CUDA_ARCHITECTURES 30 50 60 70 75 80 86 89 90 
    CACHE STRING "CUDA GPU architectures to compile for" FORCE)

message(STATUS "[CUDA] Compiling for architectures: ${CMAKE_CUDA_ARCHITECTURES}")

# CUDA-specific flags
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Xptxas -O3")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -lineinfo")

# Kepler-specific optimizations
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_30,code=sm_30")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_50,code=sm_50")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_60,code=sm_60")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_70,code=sm_70")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_75,code=sm_75")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_80,code=sm_80")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_86,code=sm_86")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_89,code=sm_89")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -gencode arch=compute_90,code=sm_90")

# ============================================================
# DEPENDENCIES
# ============================================================

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)

include(FetchContent)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

FetchContent_Declare(ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
)

FetchContent_MakeAvailable(json ImGui)

# ============================================================
# TURBOQUANT PLUS SUPPORT
# ============================================================

set(ENABLE_TURBOQUANT ON CACHE BOOL "Enable TurboQuant Plus" FORCE)

if(ENABLE_TURBOQUANT)
    message(STATUS "[TurboQuant] Enabled with CUDA ${CUDAToolkit_VERSION}")
    add_compile_definitions(TURBOQUANT_PLUS_ENABLED)
    add_compile_definitions(CUDA_11_4_MINIMUM)
endif()

# ============================================================
# LLAMA.CPP INTEGRATION
# ============================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/llama.cpp/CMakeLists.txt")
    message(STATUS "[llama.cpp] Found, integrating...")
    set(GGML_CUDA ON CACHE BOOL "" FORCE)
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
    set(GGML_CUDA_F16 ON CACHE BOOL "" FORCE)
    add_subdirectory(vendor/llama.cpp)
else()
    message(WARNING "[llama.cpp] Not found in vendor/. Skipping integration.")
endif()

# ============================================================
# CORE LIBRARY
# ============================================================

add_library(OmniCore STATIC
    src/omni_engine.cpp
    src/tool_calling_interface.cpp
    src/comfyui_integration.cpp
    src/dify_integration.cpp
    src/rag_integration.cpp
    src/turboquant_pipeline.cpp
)

target_include_directories(OmniCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(OmniCore PUBLIC
    CUDA::cudart
    CUDA::cublas
    CUDA::cublasLt
    ${CURL_LIBRARIES}
    Vulkan::Vulkan
)

target_compile_definitions(OmniCore PRIVATE
    TURBOQUANT_PLUS_ENABLED
    CUDA_MINIMUM_VERSION=11400
    SUPPORT_CUDA_11_4=1
    SUPPORT_VULKAN_AMD=1
    SUPPORT_VULKAN_INTEL=1
)

# ============================================================
# MAIN APPLICATION
# ============================================================

add_executable(OmniInference
    src/gui_visual_control.cpp
    src/gui_integration_panel.cpp
    src/gui_turboquant_panel.cpp
    src/omni_server.cpp
    src/monitoring.cpp
    src/main_visual.cpp
    ${ImGui_SOURCE_DIR}/imgui.cpp
    ${ImGui_SOURCE_DIR}/imgui_draw.cpp
    ${ImGui_SOURCE_DIR}/imgui_widgets.cpp
    ${ImGui_SOURCE_DIR}/imgui_tables.cpp
    ${ImGui_SOURCE_DIR}/imgui_demo.cpp
    ${ImGui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${ImGui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(OmniInference PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ImGui_SOURCE_DIR}
    ${ImGui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIRS}
    ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(OmniInference PRIVATE
    OmniCore
    ${SDL2_LIBRARIES}
    OpenGL::OpenGL
    Vulkan::Vulkan
)

if(WIN32)
    target_link_libraries(OmniInference PRIVATE opengl32 gdi32 ws2_32)
endif()

# ============================================================
# INSTALLATION
# ============================================================

install(TARGETS OmniInference RUNTIME DESTINATION bin)
install(DIRECTORY models DESTINATION . OPTIONAL)
install(DIRECTORY workflows DESTINATION . OPTIONAL)

# ============================================================
# CPACK (Installation Package)
# ============================================================

set(CPACK_PACKAGE_NAME "OmniInference")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION "Production-Ready LLM/MLLM Inference Engine with TurboQuant Plus")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "OmniInference v${PROJECT_VERSION}")
    set(CPACK_NSIS_CONTACT "support@omni-inference.dev")
else()
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcurl4, libvulkan1, libsdl2-2.0-0, libnvidia-gl-530, libcuda1")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OmniInference Team")
endif()

include(CPack)

# ============================================================
# BUILD SUMMARY
# ============================================================

message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "  OmniInference v${PROJECT_VERSION} - Build Configuration")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "  CUDA: ${CUDAToolkit_VERSION} (Minimum: ${MINIMUM_CUDA_VERSION})")
message(STATUS "  GPU Architectures: ${CMAKE_CUDA_ARCHITECTURES}")
message(STATUS "  TurboQuant Plus: ${ENABLE_TURBOQUANT}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "═══════════════════════════════════════════════════════════════")
message(STATUS "")