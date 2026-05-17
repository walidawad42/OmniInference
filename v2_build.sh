#!/bin/bash

# ============================================================
# OmniInference Production Build Script
# CUDA Support: 11.4 through 13.x (Kepler K5100M onwards)
# Windows 10/11 & Pop!_OS/Ubuntu Native
# ============================================================

set -e

echo "╔════════════════════════════════════════════════════════════════"
echo "║ OmniInference v2.0.0 - Production Build"
echo "║ CUDA: 11.4 through 13.x | All GPU Architectures"
echo "║ Platforms: Windows 10/11, Pop!_OS, Ubuntu 20.04+"
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

# ============================================================
# CUDA VERIFICATION (Must be 11.4 minimum)
# ============================================================

check_cuda() {
    echo "[Build] Checking CUDA installation..."

    if ! command -v nvcc &> /dev/null; then
        echo "[ERROR] CUDA Toolkit not found!"
        echo "[ERROR] Please install CUDA Toolkit 11.4 or higher"
        echo "[ERROR] Download from: https://developer.nvidia.com/cuda-toolkit-archive"
        exit 1
    fi

    CUDA_VERSION=$(nvcc --version | grep "release" | awk '{print $5}' | tr -d ',')
    echo "[Build] Found CUDA: $CUDA_VERSION"

    # Parse version
    MAJOR=$(echo $CUDA_VERSION | cut -d. -f1)
    MINOR=$(echo $CUDA_VERSION | cut -d. -f2)

    # Check minimum version (11.4)
    if [ "$MAJOR" -lt 11 ] || ([ "$MAJOR" -eq 11 ] && [ "$MINOR" -lt 4 ]); then
        echo "[ERROR] CUDA $CUDA_VERSION is below minimum requirement 11.4"
        echo "[ERROR] Please upgrade to CUDA 11.4 or higher"
        exit 1
    fi

    echo "[Build] ✓ CUDA $CUDA_VERSION verified (11.4+)"
}

# ============================================================
# GPU ARCHITECTURE DETECTION
# ============================================================

detect_architectures() {
    echo "[Build] Detecting available GPU architectures..."

    if ! command -v nvidia-smi &> /dev/null; then
        echo "[WARNING] nvidia-smi not found. Using default architectures."
        echo "[WARNING] Supported: Kepler (3.0) to Blackwell (9.0)"
        ARCHITECTURES="30;50;60;70;75;80;86;89;90"
    else
        # Get compute capability
        GPU_CC=$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader | head -1 | tr '.' '')
        echo "[Build] Detected GPU compute capability: ${GPU_CC:0:1}.${GPU_CC:1}"

        # Support all from Kepler onwards
        ARCHITECTURES="30;50;60;70;75;80;86;89;90"
        echo "[Build] Building for all supported architectures: $ARCHITECTURES"
    fi
}

# ============================================================
# DRIVER VERIFICATION
# ============================================================

check_driver() {
    echo "[Build] Checking NVIDIA driver..."

    if ! command -v nvidia-smi &> /dev/null; then
        echo "[WARNING] nvidia-smi not found. Continuing without driver check."
        return
    fi

    DRIVER_VERSION=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader | head -1)
    echo "[Build] Found driver: $DRIVER_VERSION"

    # Minimum driver for CUDA 11.4 is R470
    DRIVER_MAJOR=$(echo $DRIVER_VERSION | cut -d. -f1)
    
    if [ "$DRIVER_MAJOR" -lt 470 ]; then
        echo "[WARNING] Driver version $DRIVER_VERSION may be incompatible with CUDA 11.4"
        echo "[WARNING] Recommended: R470 or higher"
        echo "[WARNING] Continuing anyway..."
    else
        echo "[Build] ✓ Driver verified (R470+)"
    fi
}

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Create build directory
if [ ! -d "build" ]; then
    echo -e "${YELLOW}[Build] Creating build directory...${NC}"
    mkdir -p build
fi

cd build

# ============================================================
# CMAKE CONFIGURATION (CUDA 11.4 COMPLIANT)
# ============================================================

echo -e "${YELLOW}[Build] Verifying prerequisites...${NC}"
check_cuda
check_driver
detect_architectures

echo -e "${YELLOW}[Build] Configuring CMake...${NC}"

if [ "$OS" == "windows" ]; then
    cmake -G "Visual Studio 17 2022" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CUDA_COMPILER=nvcc \
        -DCMAKE_CUDA_HOST_COMPILER=cl \
        -DCMAKE_CUDA_ARCHITECTURES="${ARCHITECTURES}" \
        -DCUDA_TOOLKIT_ROOT_DIR="$(dirname $(dirname $(which nvcc)))" \
        -DENABLE_CUDA=ON \
        -DENABLE_VULKAN=ON \
        -DENABLE_TURBOQUANT=ON \
        -DMINIMUM_CUDA_VERSION=11.4 \
        ..
else
    cmake -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CUDA_COMPILER=nvcc \
        -DCMAKE_CUDA_ARCHITECTURES="${ARCHITECTURES}" \
        -DCUDA_TOOLKIT_ROOT_DIR="$(dirname $(dirname $(which nvcc)))" \
        -DENABLE_CUDA=ON \
        -DENABLE_VULKAN=ON \
        -DENABLE_TURBOQUANT=ON \
        -DMINIMUM_CUDA_VERSION=11.4 \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_CUDA_FLAGS="-gencode arch=compute_30,code=sm_30 -gencode arch=compute_50,code=sm_50 -gencode arch=compute_60,code=sm_60 -gencode arch=compute_70,code=sm_70 -gencode arch=compute_75,code=sm_75 -gencode arch=compute_80,code=sm_80 -gencode arch=compute_86,code=sm_86 -gencode arch=compute_89,code=sm_89 -gencode arch=compute_90,code=sm_90" \
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
# VERIFICATION
# ============================================================

echo -e "${YELLOW}[Build] Verifying build artifacts...${NC}"

if [ -f "./OmniInference" ] || [ -f "./OmniInference.exe" ]; then
    echo -e "${GREEN}[Build] ✓ Executable found${NC}"
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
mkdir -p dist/{bin,lib,models,workflows,configs,docs}

# Copy executables
if [ -f "build/OmniInference" ]; then
    cp build/OmniInference dist/bin/
    chmod +x dist/bin/OmniInference
elif [ -f "build/Release/OmniInference.exe" ]; then
    cp build/Release/OmniInference.exe dist/bin/
fi

# Copy models directory structure
if [ -d "models" ]; then
    cp -r models/* dist/models/ 2>/dev/null || true
fi

# Copy workflows
if [ -d "workflows" ]; then
    cp -r workflows/* dist/workflows/ 2>/dev/null || true
fi

# Create comprehensive config
cat > dist/configs/omni_config.json << 'EOF'
{
  "version": "2.0.0",
  "build_info": {
    "cuda_minimum": "11.4",
    "cuda_maximum": "13.x",
    "supported_architectures": ["sm_30", "sm_50", "sm_60", "sm_70", "sm_75", "sm_80", "sm_86", "sm_89", "sm_90"],
    "platforms": ["Windows 10", "Windows 11", "Ubuntu 20.04+", "Pop!_OS 22.04+"]
  },
  "backend": "auto",
  "cuda": {
    "enabled": true,
    "minimum_version": "11.4",
    "compute_capability_min": "3.0",
    "device_id": 0,
    "use_tensor_cores": true,
    "use_graph_capture": false
  },
  "vulkan": {
    "enabled": true,
    "minimum_version": "1.2",
    "device_index": 0
  },
  "quantization": {
    "enabled": true,
    "mode": "turbo_quant_3bit",
    "key_bits": 4,
    "value_bits": 2,
    "use_polar_transform": true,
    "use_qjl_correction": true,
    "enable_block_sparse": false,
    "block_size": 64
  },
  "inference": {
    "n_ctx": 4096,
    "n_batch": 512,
    "n_gpu_layers": -1,
    "n_threads": 0,
    "use_flash_attention": true,
    "rope_freq_base": 500000.0,
    "rope_freq_scale": 1.0
  },
  "server": {
    "port": 8000,
    "host": "0.0.0.0",
    "enable_api": true,
    "api_version": "v1"
  }
}
EOF

# Create system info file
cat > dist/configs/SYSTEM_INFO.txt << 'EOF'
OmniInference v2.0.0 - System Information

MINIMUM REQUIREMENTS:
  CUDA: 11.4 (R470+ driver)
  GPU Memory: 4GB
  System RAM: 8GB
  Disk: 20GB SSD

SUPPORTED GPUs:
  NVIDIA:
    - Kepler (sm_30) - K5100M, GTX 750Ti, etc.
    - Maxwell (sm_50) - GTX 750, GTX Titan X, etc.
    - Pascal (sm_60/61) - GTX 1080, GTX 1070, etc.
    - Volta (sm_70) - Tesla V100
    - Turing (sm_75) - RTX 2080, RTX 2070, etc.
    - Ampere (sm_80/86) - RTX 3090, RTX 3080, RTX 4090, etc.
    - Ada (sm_89) - RTX 6000 Ada
    - Hopper (sm_90) - H100

  AMD (Vulkan 1.2):
    - RDNA (RX 5700, RX 6800)
    - RDNA 2 (RX 6900 XT)
    - RDNA 3 (RX 7900 XTX)

  Intel (Vulkan 1.2):
    - Iris Xe (Arc A770, A750)
    - Future discrete GPUs

CUDA COMPATIBILITY MATRIX:
  CUDA 11.4: Windows 10 Build 1909+, Ubuntu 20.04, Pop!_OS 20.04+
  CUDA 12.x: Windows 10 Build 2004+, Windows 11, Ubuntu 22.04+, Pop!_OS 22.04+
  CUDA 13.x: Windows 11, Ubuntu 22.04+, Pop!_OS 22.04+

DRIVER REQUIREMENTS (NVIDIA):
  CUDA 11.4: Driver R470+
  CUDA 12.0: Driver R525+
  CUDA 12.1: Driver R530+
  CUDA 12.2: Driver R535+
  CUDA 12.3: Driver R545+
  CUDA 13.0: Driver R550+

For full system compatibility check:
  https://github.com/walidawad42/OmniInference/wiki/Compatibility
EOF

echo -e "${GREEN}[Build] Distribution package created${NC}"

# ============================================================
# FINAL SUMMARY
# ============================================================

echo ""
echo "╔════════════════════════════════════════════════════════════════"
echo "║ Build Complete!"
echo "╚════════════════════════════════════════════════════════════════"
echo ""
echo -e "${GREEN}Distribution location: ./dist${NC}"
echo ""
echo "Build Details:"
echo "  CUDA Version: $CUDA_VERSION"
echo "  GPU Architectures: $ARCHITECTURES"
echo "  Platform: $OS"
echo ""
echo "Next steps:"
echo "  1. Review: ./dist/configs/SYSTEM_INFO.txt"
echo "  2. Configure: ./dist/configs/omni_config.json"
echo "  3. Run: ./dist/bin/OmniInference (Linux)"
echo "       : .\dist\bin\OmniInference.exe (Windows)"
echo ""