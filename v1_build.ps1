# ============================================================
# OmniInference Production Build Script (Windows)
# ============================================================

Write-Host "╔════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "║ OmniInference v2.0.0 - Windows Production Build" -ForegroundColor Green
Write-Host "║ Windows 10/11 Native Build" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════════" -ForegroundColor Green

# Check prerequisites
Write-Host "[Build] Checking prerequisites..." -ForegroundColor Yellow

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
$vs = Get-Command cl -ErrorAction SilentlyContinue
$cuda = Get-Command nvcc -ErrorAction SilentlyContinue

if (-not $cmake) {
    Write-Host "[ERROR] CMake not found! Install CMake 3.20+" -ForegroundColor Red
    exit 1
}

if (-not $vs) {
    Write-Host "[ERROR] Visual Studio not found! Install Visual Studio 2022" -ForegroundColor Red
    exit 1
}

if (-not $cuda) {
    Write-Host "[WARNING] CUDA not found. Some features will be disabled" -ForegroundColor Yellow
}

Write-Host "[Build] Prerequisites OK" -ForegroundColor Green

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

cd build

# ============================================================
# CMAKE CONFIGURATION
# ============================================================

Write-Host "[Build] Configuring CMake..." -ForegroundColor Yellow

cmake -G "Visual Studio 17 2022" `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_CUDA_ARCHITECTURES="30;50;60;70;75;80;86;89;90" `
    -DENABLE_CUDA=ON `
    -DENABLE_VULKAN=ON `
    ..

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[Build] CMake configured successfully" -ForegroundColor Green

# ============================================================
# BUILD
# ============================================================

Write-Host "[Build] Building OmniInference (Release)..." -ForegroundColor Yellow

cmake --build . --config Release --parallel 8

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[Build] Build completed successfully" -ForegroundColor Green

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

# Copy redistributables
$vcRedist = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\*\vcruntime140.dll"
Get-Item $vcRedist -ErrorAction SilentlyContinue | Copy-Item -Destination "dist\bin\" -Force

Write-Host "[Build] Distribution package created" -ForegroundColor Green

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "║ Build Complete!" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "Distribution location: .\dist" -ForegroundColor Green
Write-Host ""
Write-Host "Run: .\dist\bin\OmniInference.exe" -ForegroundColor Cyan