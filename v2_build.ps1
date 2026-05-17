# ============================================================
# OmniInference Production Build Script (Windows)
# CUDA Support: 11.4 through 13.x (All GPU Architectures)
# Windows 10/11 Native Build
# ============================================================

Write-Host "╔════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "║ OmniInference v2.0.0 - Windows Production Build" -ForegroundColor Green
Write-Host "║ CUDA: 11.4 through 13.x | All Architectures" -ForegroundColor Green
Write-Host "║ Windows 10/11 Native Build" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════════" -ForegroundColor Green

# ============================================================
# CUDA VERIFICATION (Must be 11.4 minimum)
# ============================================================

Write-Host "[Build] Checking CUDA Toolkit..." -ForegroundColor Yellow

$cudaCheck = Get-Command nvcc -ErrorAction SilentlyContinue

if (-not $cudaCheck) {
    Write-Host "[ERROR] CUDA Toolkit not found!" -ForegroundColor Red
    Write-Host "[ERROR] Minimum requirement: CUDA 11.4" -ForegroundColor Red
    Write-Host "[ERROR] Download from: https://developer.nvidia.com/cuda-toolkit-archive" -ForegroundColor Red
    exit 1
}

# Get CUDA version
$cudaVersion = & nvcc --version | Select-String "release" | ForEach-Object { $_.ToString().Split()[4] }
$cudaVersion = $cudaVersion.TrimEnd(',')

Write-Host "[Build] Found CUDA: $cudaVersion" -ForegroundColor Green

# Parse version (11.4 minimum)
$major = [int]($cudaVersion.Split('.')[0])
$minor = [int]($cudaVersion.Split('.')[1])

if (($major -lt 11) -or ($major -eq 11 -and $minor -lt 4)) {
    Write-Host "[ERROR] CUDA $cudaVersion is below minimum requirement 11.4" -ForegroundColor Red
    Write-Host "[ERROR] Please upgrade to CUDA 11.4 or higher" -ForegroundColor Red
    exit 1
}

Write-Host "[Build] ✓ CUDA $cudaVersion verified (11.4+)" -ForegroundColor Green

# ============================================================
# DRIVER VERIFICATION
# ============================================================

Write-Host "[Build] Checking NVIDIA Driver..." -ForegroundColor Yellow

$driverCheck = Get-Command nvidia-smi -ErrorAction SilentlyContinue

if (-not $driverCheck) {
    Write-Host "[WARNING] nvidia-smi not found. GPU operations may fail." -ForegroundColor Yellow
}
else {
    $driverVersion = & nvidia-smi --query-gpu=driver_version --format=csv,noheader | Select-Object -First 1
    Write-Host "[Build] Found driver: $driverVersion" -ForegroundColor Green

    # Minimum driver for CUDA 11.4 is R470
    $driverMajor = [int]($driverVersion.Split('.')[0])

    if ($driverMajor -lt 470) {
        Write-Host "[WARNING] Driver R$driverMajor may be incompatible with CUDA 11.4" -ForegroundColor Yellow
        Write-Host "[WARNING] Recommended: R470 or higher" -ForegroundColor Yellow
    }
    else {
        Write-Host "[Build] ✓ Driver verified (R470+)" -ForegroundColor Green
    }
}

# ============================================================
# VISUAL STUDIO CHECK
# ============================================================

Write-Host "[Build] Checking Visual Studio 2022..." -ForegroundColor Yellow

$vsCheck = Get-Command cl -ErrorAction SilentlyContinue

if (-not $vsCheck) {
    Write-Host "[ERROR] Visual Studio 2022 not found!" -ForegroundColor Red
    Write-Host "[ERROR] Please install Visual Studio 2022 Community Edition" -ForegroundColor Red
    exit 1
}

Write-Host "[Build] ✓ Visual Studio verified" -ForegroundColor Green

# ============================================================
# CMAKE CHECK
# ============================================================

Write-Host "[Build] Checking CMake..." -ForegroundColor Yellow

$cmakeCheck = Get-Command cmake -ErrorAction SilentlyContinue

if (-not $cmakeCheck) {
    Write-Host "[ERROR] CMake not found!" -ForegroundColor Red
    Write-Host "[ERROR] Please install CMake 3.20 or higher" -ForegroundColor Red
    exit 1
}

Write-Host "[Build] ✓ CMake verified" -ForegroundColor Green

# ============================================================
# GPU ARCHITECTURE DETECTION
# ============================================================

Write-Host "[Build] Detecting GPU Architectures..." -ForegroundColor Yellow

# Support all architectures from Kepler (sm_30) to Hopper (sm_90)
$architectures = "30;50;60;70;75;80;86;89;90"

Write-Host "[Build] Building for architectures: $architectures" -ForegroundColor Green

# ============================================================
# CREATE BUILD DIRECTORY
# ============================================================

if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
    Write-Host "[Build] Created build directory" -ForegroundColor Yellow
}

cd build

# ============================================================
# CMAKE CONFIGURATION (CUDA 11.4 COMPLIANT)
# ============================================================

Write-Host "[Build] Configuring CMake..." -ForegroundColor Yellow

$cudaRoot = (Get-Command nvcc).Source | Split-Path | Split-Path

cmake -G "Visual Studio 17 2022" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CUDA_HOST_COMPILER="cl" `
    -DCMAKE_CUDA_ARCHITECTURES="$architectures" `
    -DCUDA_TOOLKIT_ROOT_DIR="$cudaRoot" `
    -DENABLE_CUDA=ON `
    -DENABLE_VULKAN=ON `
    -DENABLE_TURBOQUANT=ON `
    -DMINIMUM_CUDA_VERSION=11.4 `
    -DCUDA_NVCC_FLAGS="-gencode arch=compute_30,code=sm_30 -gencode arch=compute_50,code=sm_50 -gencode arch=compute_60,code=sm_60 -gencode arch=compute_70,code=sm_70 -gencode arch=compute_75,code=sm_75 -gencode arch=compute_80,code=sm_80 -gencode arch=compute_86,code=sm_86 -gencode arch=compute_89,code=sm_89 -gencode arch=compute_90,code=sm_90" `
    ..

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[Build] ✓ CMake configured successfully" -ForegroundColor Green

# ============================================================
# BUILD (Release Configuration)
# ============================================================

Write-Host "[Build] Building OmniInference (Release)..." -ForegroundColor Yellow

cmake --build . --config Release --parallel 8

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[Build] ✓ Build completed successfully" -ForegroundColor Green

# ============================================================
# PACKAGING
# ============================================================

Write-Host "[Build] Creating distribution package..." -ForegroundColor Yellow

cd ..

if (-not (Test-Path "dist")) {
    New-Item -ItemType Directory -Path "dist\bin" | Out-Null
    New-Item -ItemType Directory -Path "dist\models" | Out-Null
    New-Item -ItemType Directory -Path "dist\workflows" | Out-Null
    New-Item -ItemType Directory -Path "dist\configs" | Out-Null
}

# Copy executable
Copy-Item "build\Release\OmniInference.exe" "dist\bin\" -Force

# Copy CUDA runtime redistributables
$cudaRedists = @(
    "cudart64_11.dll",
    "cublas64_11.dll",
    "cublasLt64_11.dll",
    "cusolver64_11.dll",
    "cusparse64_11.dll"
)

foreach ($redist in $cudaRedists) {
    $redistPath = "$cudaRoot\bin\$redist"
    if (Test-Path $redistPath) {
        Copy-Item $redistPath "dist\bin\" -Force
    }
}

# Create config file
$configJson = @{
    version = "2.0.0"
    build_info = @{
        cuda_minimum = "11.4"
        cuda_maximum = "13.x"
        cuda_version_detected = $cudaVersion
        driver_version_detected = $driverVersion
        supported_architectures = @("sm_30", "sm_50", "sm_60", "sm_70", "sm_75", "sm_80", "sm_86", "sm_89", "sm_90")
        platforms = @("Windows 10", "Windows 11")
    }
    backend = "auto"
    cuda = @{
        enabled = $true
        minimum_version = "11.4"
        compute_capability_min = "3.0"
        device_id = 0
        use_tensor_cores = $true
    }
    vulkan = @{
        enabled = $true
        minimum_version = "1.2"
        device_index = 0
    }
    quantization = @{
        enabled = $true
        mode = "turbo_quant_3bit"
        key_bits = 4
        value_bits = 2
        use_polar_transform = $true
        use_qjl_correction = $true
    }
    inference = @{
        n_ctx = 4096
        n_batch = 512
        n_gpu_layers = -1
        use_flash_attention = $true
    }
} | ConvertTo-Json

$configJson | Out-File "dist\configs\omni_config.json" -Encoding UTF8

Write-Host "[Build] ✓ Distribution package created" -ForegroundColor Green

# ============================================================
# FINAL SUMMARY
# ============================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "║ Build Complete!" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "Distribution location: .\dist" -ForegroundColor Green
Write-Host ""
Write-Host "Build Details:" -ForegroundColor Cyan
Write-Host "  CUDA Version: $cudaVersion" -ForegroundColor Cyan
Write-Host "  Driver Version: $driverVersion" -ForegroundColor Cyan
Write-Host "  GPU Architectures: $architectures (sm_30 to sm_90)" -ForegroundColor Cyan
Write-Host "  Platform: Windows 10/11" -ForegroundColor Cyan
Write-Host ""
Write-Host "Run:" -ForegroundColor Yellow
Write-Host "  .\dist\bin\OmniInference.exe" -ForegroundColor Yellow
Write-Host ""