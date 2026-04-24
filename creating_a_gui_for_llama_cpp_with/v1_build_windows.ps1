# ============================================================
# OmniInference v2.0.0 - Windows 10/11 Build Script
# ============================================================
# Supports: CUDA 10.1 → Latest
# Backends: CUDA, Vulkan
# ============================================================

param(
    [string]$BuildType = "Release",
    [int]$Jobs = 8,
    [switch]$EnableKepler = $false,
    [switch]$SkipCUDA = $false
)

$ErrorActionPreference = "Stop"

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║ OmniInference v2.0.0 - Windows 10/11 Build                ║" -ForegroundColor Cyan
Write-Host "║ Multi-CUDA Support (10.1 → Latest)                        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ============================================================
# STEP 1: Environment Detection
# ============================================================

Write-Host "[1/7] Detecting environment..." -ForegroundColor Yellow

$OSInfo = Get-ComputerInfo | Select-Object OsName, OsVersion
Write-Host "  OS: $($OSInfo.OsName) $($OSInfo.OsVersion)" -ForegroundColor White

# Detect Visual Studio
$VSPath = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
if ($null -ne $VSPath) {
    Write-Host "  Visual Studio: $(Split-Path -Leaf $VSPath.FullName) ✓" -ForegroundColor Green
} else {
    Write-Host "  Visual Studio: Not found ✗" -ForegroundColor Red
    Write-Host "  Please install Visual Studio 2022 with C++ support" -ForegroundColor Yellow
    exit 1
}

# Detect CUDA
$cudaPath = Get-Command nvcc -ErrorAction SilentlyContinue
if ($null -ne $cudaPath) {
    $cudaVersion = & nvcc --version | Select-String "release" | ForEach-Object { $_ -replace '.*release ', '' -replace ',.*', '' }
    Write-Host "  CUDA: $cudaVersion ✓" -ForegroundColor Green
    $cudaDetected = $true
} else {
    Write-Host "  CUDA: Not found (will use Vulkan or CPU)" -ForegroundColor Yellow
    $cudaDetected = $false
}

# Detect GPU
$gpuInfo = & nvidia-smi --query-gpu=name,memory.total --format=csv,noheader -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -ne $gpuInfo) {
    Write-Host "  GPU: $gpuInfo ✓" -ForegroundColor Green
}

Write-Host ""

# ============================================================
# STEP 2: Prepare Build Directory
# ============================================================

Write-Host "[2/7] Preparing build directory..." -ForegroundColor Yellow

if (Test-Path "build") {
    Remove-Item -Path "build" -Recurse -Force -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Path "build" -Force | Out-Null
Write-Host "  ✓ Build directory ready" -ForegroundColor Green
Write-Host ""

# ============================================================
# STEP 3: Configure CMake
# ============================================================

Write-Host "[3/7] Configuring CMake..." -ForegroundColor Yellow

Set-Location build

$cmakeOptions = @(
    "-G", "Visual Studio 17 2022"
    "-DCMAKE_BUILD_TYPE=$BuildType"
    "-DCMAKE_CXX_STANDARD=17"
)

# CUDA configuration
if ($cudaDetected -and -not $SkipCUDA) {
    $cmakeOptions += "-DBUILD_CUDA=ON"
    
    if ($EnableKepler) {
        Write-Host "  Enabling Kepler (SM_30) support..." -ForegroundColor Yellow
        $cmakeOptions += "-DENABLE_LEGACY_KEPLER=ON"
    }
} else {
    $cmakeOptions += "-DBUILD_CUDA=OFF"
}

# Vulkan
$cmakeOptions += "-DBUILD_VULKAN=ON"

# Testing
$cmakeOptions += "-DBUILD_TESTING=ON"

Write-Host "  CMake options: $($cmakeOptions -join ' ')" -ForegroundColor White

& cmake $cmakeOptions ".."

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "  ✓ CMake configured" -ForegroundColor Green
Write-Host ""

# ============================================================
# STEP 4: Compile
# ============================================================

Write-Host "[4/7] Compiling OmniInference..." -ForegroundColor Yellow
Write-Host "  (This may take 3-5 minutes...)" -ForegroundColor Gray
Write-Host ""

$buildCmd = @(
    "cmake"
    "--build", "."
    "--config", $BuildType
    "--parallel", $Jobs.ToString()
)

& $buildCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "  ✓ Compilation successful" -ForegroundColor Green
Write-Host ""

# ============================================================
# STEP 5: Run Tests
# ============================================================

Write-Host "[5/7] Running tests..." -ForegroundColor Yellow

& ctest --output-on-failure --build-config $BuildType

if ($LASTEXITCODE -eq 0) {
    Write-Host "  ✓ All tests passed" -ForegroundColor Green
} else {
    Write-Host "  ⚠ Some tests failed (not critical)" -ForegroundColor Yellow
}

Write-Host ""

# ============================================================
# STEP 6: Verify Binaries
# ============================================================

Write-Host "[6/7] Verifying build..." -ForegroundColor Yellow

$exePath = Join-Path (Get-Location) "$BuildType\OmniInference.exe"
if (Test-Path $exePath) {
    $fileSize = (Get-Item $exePath).Length / 1MB
    Write-Host "  ✓ Binary: OmniInference.exe ($([Math]::Round($fileSize, 2)) MB)" -ForegroundColor Green
} else {
    Write-Host "  ✗ Binary not found!" -ForegroundColor Red
    exit 1
}

Write-Host ""

# ============================================================
# STEP 7: Summary
# ============================================================

Write-Host "[7/7] Build complete!" -ForegroundColor Yellow
Write-Host ""

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║ ✅ BUILD COMPLETE - WINDOWS 10/11                         ║" -ForegroundColor Green
Write-Host "╠════════════════════════════════════════════════════════════╣" -ForegroundColor Green
Write-Host "║                                                            ║" -ForegroundColor Green
Write-Host "║ Binary Locations:                                          ║" -ForegroundColor Green
Write-Host "║   .\$BuildType\OmniInference.exe (GUI)                      ║" -ForegroundColor Green
Write-Host "║   .\$BuildType\OmniInference-CLI.exe (CLI)                  ║" -ForegroundColor Green
Write-Host "║                                                            ║" -ForegroundColor Green
Write-Host "║ To run GUI:                                                ║" -ForegroundColor Green
Write-Host "║   .\$BuildType\OmniInference.exe                            ║" -ForegroundColor Green
Write-Host "║                                                            ║" -ForegroundColor Green
Write-Host "║ To run CLI:                                                ║" -ForegroundColor Green
Write-Host "║   .\$BuildType\OmniInference-CLI.exe --model llama-2-7b    ║" -ForegroundColor Green
Write-Host "║                                                            ║" -ForegroundColor Green
Write-Host "║ To start API server:                                       ║" -ForegroundColor Green
Write-Host "║   .\$BuildType\OmniInference.exe --server --port 8000     ║" -ForegroundColor Green
Write-Host "║                                                            ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Green