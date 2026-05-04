cmake_minimum_required(VERSION 3.20)

# CUDA is enabled when the CUDA Toolkit is present on the host. On machines
# without nvcc (CI / dev boxes / Linux laptops without a GPU) we fall back
# to a CPU/Vulkan-only build so the rest of the codebase still compiles.
project(OmniInference VERSION 2.0.0 LANGUAGES C CXX)

# 10.1 is the floor because that's the last toolkit family that natively
# targets the Kepler / sm_30 GPU generation (e.g. Quadro K5100M). Modern
# build hosts with CUDA 11+ are still preferred and exercise the full
# kernel matrix; anything in between works as long as nvcc supports the
# arch list we hand it.
set(MINIMUM_CUDA_VERSION "10.1" CACHE STRING "Minimum CUDA version required")

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(OMNI_HAVE_CUDA ON)
    message(STATUS "[OmniInference] CUDA language enabled (${CMAKE_CUDA_COMPILER})")
else()
    set(OMNI_HAVE_CUDA OFF)
    message(STATUS "[OmniInference] CUDA toolchain not found - building CPU/Vulkan-only fallback")
endif()

# ============================================================
# PLATFORM & COMPILER SETUP
# ============================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cross-platform setup
if(WIN32)
    message(STATUS "[OmniInference] Building for Windows 10/11")
    add_compile_options(/W4 /permissive-)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    message(STATUS "[OmniInference] Building for Linux (Ubuntu/Pop!_OS)")
    # `-Werror` is intentionally omitted: this codebase still has plenty of
    # benign warnings (unused params, signed/unsigned comparisons in vendored
    # code) and turning warnings into errors blocks the build before we can
    # even surface real problems. Promote individual warnings as they get
    # fixed instead.
    add_compile_options(-Wall -Wextra -fPIC)
endif()

# ============================================================
# CUDA CONFIGURATION (10.1 minimum, 11.4+ recommended)
# ============================================================

if(OMNI_HAVE_CUDA)
    message(STATUS "[CUDA] Minimum requirement: ${MINIMUM_CUDA_VERSION}")

    find_package(CUDAToolkit ${MINIMUM_CUDA_VERSION} REQUIRED)

    message(STATUS "[CUDA] Found CUDA: ${CUDAToolkit_VERSION}")
    message(STATUS "[CUDA] CUDA Runtime: ${CUDAToolkit_LIBRARY}")

    # nvcc only knows about architectures that shipped with its toolkit. The
    # full sm_30..sm_90 list assumes CUDA >= 11.8; on older toolkits we have
    # to clamp before nvcc rejects the unknown -gencode arguments.
    #
    #   Toolkit | Highest supported arch
    #   --------+-----------------------
    #   10.x    | sm_75 (Turing)
    #   11.0..1 | sm_80 (Ampere A100)
    #   11.2..7 | sm_86 (Ampere consumer)
    #   11.8+   | sm_89 / sm_90 (Ada Lovelace / Hopper)
    if(CUDAToolkit_VERSION_MAJOR LESS 11)
        set(_OMNI_CUDA_ARCHS 30 50 60 70 75)
    elseif(CUDAToolkit_VERSION_MAJOR EQUAL 11 AND CUDAToolkit_VERSION_MINOR LESS 2)
        set(_OMNI_CUDA_ARCHS 30 50 60 70 75 80)
    elseif(CUDAToolkit_VERSION_MAJOR EQUAL 11 AND CUDAToolkit_VERSION_MINOR LESS 8)
        set(_OMNI_CUDA_ARCHS 50 60 70 75 80 86)
    else()
        set(_OMNI_CUDA_ARCHS 50 60 70 75 80 86 89 90)
    endif()
    set(CMAKE_CUDA_ARCHITECTURES ${_OMNI_CUDA_ARCHS}
        CACHE STRING "CUDA GPU architectures to compile for" FORCE)

    message(STATUS "[CUDA] Compiling for architectures: ${CMAKE_CUDA_ARCHITECTURES}")

    # CUDA-specific flags. The -gencode flags previously set here duplicated
    # CMAKE_CUDA_ARCHITECTURES, which CMake already translates into the right
    # -gencode arguments for nvcc, so we omit them and let CMake handle it.
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Xptxas -O3")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -lineinfo")
endif()

# ============================================================
# DEPENDENCIES
# ============================================================

find_package(Vulkan)
find_package(SDL2 REQUIRED)
find_package(OpenGL)
find_package(CURL REQUIRED)

if(Vulkan_FOUND)
    add_compile_definitions(OMNI_HAVE_VULKAN)
endif()

include(FetchContent)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

# Pin to a known-stable docking-branch tag so the API surface used in
# src/gui_*.cpp (Columns / NextColumn / Combo / ImGuiCond_FirstUseEver / ...)
# stays available across rebuilds. v1.89.9 is intentionally chosen because
# imgui-node-editor v0.9.3 ships its own ImVec2 operator==/operator!= in
# imgui_extra_math.inl which collide with the versions added to ImGui in
# v1.90+. Both are header-only, so pinning ImGui keeps the API stable while
# avoiding the redefinition error at compile time.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)

# imgui-node-editor provides the ax::NodeEditor namespace used by
# include/gui_node_editor.h.
FetchContent_Declare(imgui_node_editor
    GIT_REPOSITORY https://github.com/thedmd/imgui-node-editor.git
    GIT_TAG v0.9.3
)

# Note: TurboQuant Plus was previously fetched from an external GitHub
# repository that no longer exists. The TurboQuant pipeline implementation
# now lives directly in src/turboquant_pipeline.cpp and src/turbo_quant_kernels.cu
# and is built as part of OmniCore below, so no FetchContent declaration is
# required for it.
FetchContent_MakeAvailable(json imgui imgui_node_editor)

# Neither ImGui nor imgui-node-editor ships a CMake target on their pinned
# tags, so build small static libraries here that downstream targets link
# against. Keeping ImGui in its own target avoids defining its symbols twice
# (once in the executable, once in imgui_node_editor).
add_library(imgui_static STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_static PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Enable ImGui's math operators (`ImVec2 + ImVec2`, etc.) for both ImGui's
# own translation units and any downstream consumer that links us.
target_compile_definitions(imgui_static PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
if(SDL2_INCLUDE_DIRS)
    target_include_directories(imgui_static PUBLIC ${SDL2_INCLUDE_DIRS})
endif()
set_target_properties(imgui_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(imgui_node_editor STATIC
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor_api.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_canvas.cpp
    ${imgui_node_editor_SOURCE_DIR}/crude_json.cpp
)
target_include_directories(imgui_node_editor PUBLIC
    ${imgui_node_editor_SOURCE_DIR}
)
target_link_libraries(imgui_node_editor PUBLIC imgui_static)
set_target_properties(imgui_node_editor PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ============================================================
# TURBOQUANT PLUS SUPPORT
# ============================================================

set(ENABLE_TURBOQUANT ON CACHE BOOL "Enable TurboQuant Plus" FORCE)

if(ENABLE_TURBOQUANT)
    if(OMNI_HAVE_CUDA)
        message(STATUS "[TurboQuant] Enabled with CUDA ${CUDAToolkit_VERSION}")
    else()
        message(STATUS "[TurboQuant] Enabled (host-only, no CUDA toolchain)")
    endif()
    add_compile_definitions(TURBOQUANT_PLUS_ENABLED)
    if(OMNI_HAVE_CUDA AND NOT CUDAToolkit_VERSION_MAJOR LESS 11)
        add_compile_definitions(CUDA_11_4_MINIMUM)
    endif()
endif()

# ============================================================
# LLAMA.CPP INTEGRATION
# ============================================================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/llama.cpp/CMakeLists.txt")
    message(STATUS "[llama.cpp] Found, integrating...")

    # Modern llama.cpp's GGML_CUDA backend assumes CUDA >= 11. On older
    # toolkits (e.g. CUDA 10.1 hosts targeting Kepler) we have to either
    # build llama.cpp without its CUDA backend or pin an older llama.cpp
    # commit. We default to the safe choice (CUDA backend off) and emit a
    # status line so the operator knows what happened.
    if(OMNI_HAVE_CUDA AND CUDAToolkit_VERSION_MAJOR LESS 11)
        message(STATUS "[llama.cpp] CUDA ${CUDAToolkit_VERSION} < 11 detected; "
                       "forcing GGML_CUDA=OFF. To use the CUDA backend on "
                       "this host, pin vendor/llama.cpp to a commit from "
                       "around mid-2023 (e.g. b1429) and re-enable manually.")
        set(GGML_CUDA OFF CACHE BOOL "" FORCE)
    else()
        set(GGML_CUDA ${OMNI_HAVE_CUDA} CACHE BOOL "" FORCE)
    endif()
    set(GGML_VULKAN ON CACHE BOOL "" FORCE)
    set(GGML_CUDA_F16 ${GGML_CUDA} CACHE BOOL "" FORCE)
    add_subdirectory(vendor/llama.cpp)
else()
    message(WARNING "[llama.cpp] Not found in vendor/. Skipping integration.")
endif()

# ============================================================
# CORE LIBRARY
# ============================================================

set(OMNI_CORE_SOURCES
    src/omni_engine.cpp
    src/tool_calling_interface.cpp
    src/comfyui_integration.cpp
    src/dify_integration.cpp
    src/rag_integration.cpp
    src/minicpm_integration.cpp
    src/turboquant_pipeline.cpp
    src/memory_manager.cpp
    src/monitoring.cpp
    src/markdown_exporter.cpp
    src/omni_server.cpp
)

if(OMNI_HAVE_CUDA)
    list(APPEND OMNI_CORE_SOURCES src/turbo_quant_kernels.cu)
endif()

add_library(OmniCore STATIC ${OMNI_CORE_SOURCES})

target_include_directories(OmniCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(OmniCore PUBLIC ${CURL_LIBRARIES})

if(OMNI_HAVE_CUDA)
    target_link_libraries(OmniCore PUBLIC
        CUDA::cudart
        CUDA::cublas
    )
    # cuBLASLt only ships in CUDA >= 10.1 as a separate component and is
    # most reliably present on CUDA >= 11. Keep it gated so CUDA 10.x
    # builds don't fail to find the import target.
    if(NOT CUDAToolkit_VERSION_MAJOR LESS 11)
        target_link_libraries(OmniCore PUBLIC CUDA::cublasLt)
    endif()

    # nvcc on CUDA 10.x tops out at C++14 (with experimental C++17 on 10.2).
    # The host TUs in OmniCore continue to compile as C++20 (set above) but
    # the .cu kernel file has to be down-leveled to keep nvcc happy.
    if(CUDAToolkit_VERSION_MAJOR LESS 11)
        set_source_files_properties(src/turbo_quant_kernels.cu PROPERTIES
            CUDA_STANDARD 14
            CUDA_STANDARD_REQUIRED ON)
    endif()
endif()

if(Vulkan_FOUND)
    target_link_libraries(OmniCore PUBLIC Vulkan::Vulkan)
endif()

target_compile_definitions(OmniCore PRIVATE
    TURBOQUANT_PLUS_ENABLED
    SUPPORT_VULKAN_AMD=1
    SUPPORT_VULKAN_INTEL=1
)
if(OMNI_HAVE_CUDA)
    # CUDA_MINIMUM_VERSION encodes major*1000 + minor*10 (matches the
    # CUDA_VERSION macro nvcc emits) so feature gates can compare against
    # the active toolkit at compile time.
    math(EXPR _OMNI_CUDA_VERSION_INT
         "${CUDAToolkit_VERSION_MAJOR} * 1000 + ${CUDAToolkit_VERSION_MINOR} * 10")
    target_compile_definitions(OmniCore PRIVATE
        CUDA_MINIMUM_VERSION=${_OMNI_CUDA_VERSION_INT}
    )
    if(NOT CUDAToolkit_VERSION_MAJOR LESS 11)
        target_compile_definitions(OmniCore PRIVATE SUPPORT_CUDA_11_4=1)
    endif()
endif()

# ============================================================
# MAIN APPLICATION
# ============================================================

add_executable(OmniInference
    src/gui_app.cpp
    src/gui_main.cpp
    src/gui_node_editor.cpp
    src/gui_visual_control.cpp
    src/gui_integration_panel.cpp
    src/gui_turboquant_panel.cpp
    src/gui_memory_buffer_panel.cpp
    src/main_visual.cpp
)

target_include_directories(OmniInference PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
    ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(OmniInference PRIVATE
    OmniCore
    imgui_static
    imgui_node_editor
    ${SDL2_LIBRARIES}
)

if(OPENGL_FOUND)
    target_link_libraries(OmniInference PRIVATE OpenGL::GL)
endif()
if(Vulkan_FOUND)
    target_link_libraries(OmniInference PRIVATE Vulkan::Vulkan)
endif()

if(WIN32)
    # DXGI / D3D11 are only available on Windows; the Linux path uses the
    # Vulkan / NVML / CUDA fallbacks compiled into OmniCore.
    target_link_libraries(OmniInference PRIVATE opengl32 gdi32 ws2_32 d3d11 dxgi d3dcompiler)
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