#!/bin/bash

set -e

echo "╔════════════════════════════════════════════════════════════════"
echo "║ OmniInference v2.0.0 - Production Build"
echo "║ CUDA: 11.4+ | Platforms: Linux, Pop!_OS, Windows"
echo "╚════════════════════════════════════════════════════════════════"

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    echo "[Build] OS: Linux detected"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    OS="windows"
    echo "[Build] OS: Windows detected"
else
    echo "[ERROR] Unsupported OS: $OSTYPE"
    exit 1
fi

# Check CUDA
echo "[Build] Checking CUDA Toolkit..."
if ! command -v nvcc &> /dev/null; then
    echo "[ERROR] CUDA Toolkit not found!"
    echo "[ERROR] Please install CUDA 11.4 or higher"
    exit 1
fi

CUDA_VERSION=$(nvcc --version | grep "release" | awk '{print $5}' | tr -d ',')
echo "[Build] Found CUDA: $CUDA_VERSION"

# Create build directory
mkdir -p build
cd build

# Configure CMake
echo "[Build] Configuring CMake..."

if [ "$OS" == "windows" ]; then
    cmake -G "Visual Studio 17 2022" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CUDA_ARCHITECTURES="30;50;60;70;75;80;86;89;90" \
        ..
else
    cmake -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CUDA_ARCHITECTURES="30;50;60;70;75;80;86;89;90" \
        ..
fi

if [ $? -ne 0 ]; then
    echo "[ERROR] CMake configuration failed!"
    exit 1
fi

# Build
echo "[Build] Building OmniInference..."

if [ "$OS" == "windows" ]; then
    cmake --build . --config Release --parallel 8
else
    make -j$(nproc)
fi

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed!"
    exit 1
fi

# Verify
if [ -f "./OmniInference" ] || [ -f "./Release/OmniInference.exe" ]; then
    echo "[Build] ✓ Build successful!"
    echo ""
    echo "Output:"
    if [ "$OS" == "windows" ]; then
        echo "  Windows: ./build/Release/OmniInference.exe"
    else
        echo "  Linux: ./build/OmniInference"
    fi
else
    echo "[ERROR] Executable not found!"
    exit 1
fi