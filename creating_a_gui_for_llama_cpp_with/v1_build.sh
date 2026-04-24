#!/bin/bash

# ============================================================
# OmniInference Production Build Script
# Supports: Ubuntu 20.04+, Pop!_OS 22.04+, Windows 10/11
# ============================================================

set -e

echo "╔════════════════════════════════════════════════════════════════"
echo "║ OmniInference v2.0.0 - Production Build"
echo "║ Cross-Platform: Linux (Pop!_OS/Ubuntu) & Windows"
echo "╚════════════════════════════════════════════════════════════════"

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    echo "[Build] Detected OS: Linux"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    OS="windows"
    echo "[Build] Detected OS: Windows"
else
    echo "[ERROR] Unsupported OS: $OSTYPE"
    exit 1
fi

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create build directory
if [ ! -d "build" ]; then
    echo -e "${YELLOW}[Build] Creating build directory...${NC}"
    mkdir -p build
fi

cd build

# ============================================================
# CMAKE CONFIGURATION
# ============================================================

echo -e "${YELLOW}[Build] Configuring CMake...${NC}"

if [ "$OS" == "windows" ]; then
    cmake -G "Visual Studio 17 2022" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CUDA_ARCHITECTURES="30;50;60;70;75;80;86;89;90" \
        -DENABLE_CUDA=ON \
        -DENABLE_VULKAN=ON \
        ..
else
    cmake -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CUDA_ARCHITECTURES="30;50;60;70;75;80;86;89;90" \
        -DENABLE_CUDA=ON \
        -DENABLE_VULKAN=ON \
        -DCMAKE_CXX_COMPILER=g++ \
        ..
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] CMake configuration failed!${NC}"
    exit 1
fi

echo -e "${GREEN}[Build] CMake configured successfully${NC}"

# ============================================================
# BUILD
# ============================================================

echo -e "${YELLOW}[Build] Building OmniInference...${NC}"

if [ "$OS" == "windows" ]; then
    cmake --build . --config Release --parallel 8
else
    make -j$(nproc)
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}[ERROR] Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}[Build] Build completed successfully${NC}"

# ============================================================
# TESTING
# ============================================================

echo -e "${YELLOW}[Build] Running tests...${NC}"

if [ -f "./OmniInference" ] || [ -f "./OmniInference.exe" ]; then
    echo -e "${GREEN}[Build] Executable found${NC}"
else
    echo -e "${RED}[ERROR] Executable not found!${NC}"
    exit 1
fi

# ============================================================
# PACKAGING
# ============================================================

echo -e "${YELLOW}[Build] Creating distribution packages...${NC}"

cd ..

# Create dist directory
mkdir -p dist/{bin,lib,models,workflows,configs}

# Copy executables
if [ -f "build/OmniInference" ]; then
    cp build/OmniInference dist/bin/
elif [ -f "build/Release/OmniInference.exe" ]; then
    cp build/Release/OmniInference.exe dist/bin/
fi

# Copy models
if [ -d "models" ]; then
    cp -r models/* dist/models/ 2>/dev/null || true
fi

# Copy workflows
if [ -d "workflows" ]; then
    cp -r workflows/* dist/workflows/ 2>/dev/null || true
fi

# Copy config
cat > dist/configs/omni_config.json << 'EOF'
{
  "version": "2.0.0",
  "backend": "auto",
  "cuda": {
    "enabled": true,
    "compute_capability_min": "3.0"
  },
  "quantization": {
    "mode": "turbo_quant_3bit",
    "key_bits": 4,
    "value_bits": 2,
    "use_polar_transform": true,
    "use_qjl_correction": true
  },
  "inference": {
    "n_ctx": 4096,
    "n_batch": 512,
    "n_gpu_layers": -1,
    "use_flash_attention": true
  }
}
EOF

echo -e "${GREEN}[Build] Distribution package created${NC}"

# ============================================================
# INSTALLATION SUMMARY
# ============================================================

echo ""
echo "╔════════════════════════════════════════════════════════════════"
echo "║ Build Complete!"
echo "╚════════════════════════════════════════════════════════════════"
echo ""
echo -e "${GREEN}Distribution location: ./dist${NC}"
echo ""
echo "Next steps:"
echo "  1. Linux:   ./dist/bin/OmniInference"
echo "  2. Windows: dist\\bin\\OmniInference.exe"
echo ""