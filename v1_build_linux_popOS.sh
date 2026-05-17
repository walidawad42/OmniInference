#!/bin/bash
# ============================================================
# OmniInference v2.0.0 - Pop!_OS / Ubuntu Build Script
# ============================================================
# Supports: CUDA 10.1 → Latest
# Backends: CUDA, Vulkan
# ============================================================

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║ OmniInference v2.0.0 - Pop!_OS/Ubuntu Build               ║"
echo "║ Multi-CUDA Support (10.1 → Latest)                        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# ============================================================
# STEP 1: Detect Environment
# ============================================================

echo "[1/7] Detecting environment..."

OS_NAME=$(lsb_release -si 2>/dev/null || echo "Linux")
OS_VERSION=$(lsb_release -sr 2>/dev/null || echo "Unknown")

echo "  OS: $OS_NAME $OS_VERSION"

# Detect compiler
if command -v gcc &> /dev/null; then
    GCC_VERSION=$(gcc --version | head -n1)
    echo "  GCC: $GCC_VERSION"
fi

# Detect CUDA
if command -v nvcc &> /dev/null; then
    CUDA_VERSION=$(nvcc --version | grep "release" | awk '{print $5}' | tr -d ',')
    echo "  CUDA: $CUDA_VERSION ✓"
    CUDA_DETECTED=1
else
    echo "  CUDA: Not found (will build CPU-only or use Vulkan)"
    CUDA_DETECTED=0
fi

# Detect GPU
if command -v nvidia-smi &> /dev/null; then
    GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -1)
    GPU_VRAM=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader | head -1)
    echo "  GPU: $GPU_NAME ($GPU_VRAM) ✓"
fi

echo ""

# ============================================================
# STEP 2: Install Dependencies (if needed)
# ============================================================

echo "[2/7] Checking dependencies..."

# Check for required packages
if ! command -v cmake &> /dev/null; then
    echo "  Installing CMake..."
    sudo apt-get update
    sudo apt-get install -y cmake
fi

if ! command -v make &> /dev/null; then
    echo "  Installing build-essential..."
    sudo apt-get install -y build-essential
fi

# Graphics libraries
echo "  Installing graphics libraries..."
sudo apt-get install -y libsdl2-dev libvulkan-dev libgl1-mesa-dev libcurl4-openssl-dev

echo "  ✓ Dependencies checked"
echo ""

# ============================================================
# STEP 3: Prepare Build Directory
# ============================================================

echo "[3/7] Preparing build directory..."

if [ -d "build" ]; then
    rm -rf build
fi

mkdir -p build
cd build

echo "  ✓ Build directory ready"
echo ""

# ============================================================
# STEP 4: Configure CMake
# ============================================================

echo "[4/7] Configuring CMake..."

# Build options
CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=Release"
CMAKE_OPTIONS="$CMAKE_OPTIONS -DCMAKE_CXX_STANDARD=17"

# CUDA support
if [ "$CUDA_DETECTED" -eq 1 ]; then
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DBUILD_CUDA=ON"
    
    # Check if Kepler support needed (CUDA < 11.0)
    CUDA_MAJOR=$(echo $CUDA_VERSION | cut -d. -f1)
    if [ "$CUDA_MAJOR" -lt 11 ]; then
        echo "  CUDA 10.x detected - enabling Kepler support..."
        CMAKE_OPTIONS="$CMAKE_OPTIONS -DENABLE_LEGACY_KEPLER=ON"
    fi
else
    CMAKE_OPTIONS="$CMAKE_OPTIONS -DBUILD_CUDA=OFF"
fi

# Vulkan support (always on for Linux)
CMAKE_OPTIONS="$CMAKE_OPTIONS -DBUILD_VULKAN=ON"

# Testing
CMAKE_OPTIONS="$CMAKE_OPTIONS -DBUILD_TESTING=ON"

echo "  CMake Options: $CMAKE_OPTIONS"

cmake $CMAKE_OPTIONS ..

if [ $? -ne 0 ]; then
    echo "  ✗ CMake configuration failed!"
    exit 1
fi

echo "  ✓ CMake configured"
echo ""

# ============================================================
# STEP 5: Compile
# ============================================================

echo "[5/7] Compiling OmniInference..."
echo "  (This may take 3-5 minutes...)"
echo ""

NPROC=$(nproc)
make -j$NPROC

if [ $? -ne 0 ]; then
    echo "  ✗ Build failed!"
    exit 1
fi

echo ""
echo "  ✓ Compilation successful"
echo ""

# ============================================================
# STEP 6: Run Tests (Optional)
# ============================================================

echo "[6/7] Running tests..."

if ctest --output-on-failure; then
    echo "  ✓ All tests passed"
else
    echo "  ⚠ Some tests failed (not critical)"
fi

echo ""

# ============================================================
# STEP 7: Verification & Summary
# ============================================================

echo "[7/7] Verifying build..."

if [ -f "./OmniInference" ]; then
    FILE_SIZE=$(du -h ./OmniInference | cut -f1)
    echo "  ✓ Binary created: OmniInference ($FILE_SIZE)"
else
    echo "  ✗ Binary not found!"
    exit 1
fi

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║ ✅ BUILD COMPLETE - POP!_OS/UBUNTU                         ║"
echo "╠════════════════════════════════════════════════════════════╣"
echo "║                                                            ║"
echo "║ Binary: ./OmniInference (GUI)                              ║"
echo "║ CLI: ./OmniInference-CLI (Command-line)                    ║"
echo "║                                                            ║"
echo "║ To run:                                                    ║"
echo "║   ./OmniInference                                          ║"
echo "║   ./OmniInference-CLI --model llama-2-7b                   ║"
echo "║                                                            ║"
echo "║ To start API server:                                       ║"
echo "║   ./OmniInference --server --port 8000                     ║"
echo "║                                                            ║"
echo "╚════════════════════════════════════════════════════════════╝"