# setup_llama_cpp_kepler.ps1
#
# PowerShell port of scripts/setup_llama_cpp_kepler.sh, intended for native
# Windows hosts (Windows 10 / 11) where Git Bash isn't installed but Git
# for Windows + PowerShell are. Mirrors the bash version's behaviour
# exactly: pins vendor/llama.cpp to a Kepler / sm_30-compatible tag so
# the project's vendored llama.cpp can be built with its CUDA backend on
# a CUDA 10.x toolchain (e.g. Quadro K5100M + CUDA 10.1).
#
# Usage (from the repository root, in PowerShell):
#     .\scripts\setup_llama_cpp_kepler.ps1
#     cmake -S . -B build -DOMNI_LLAMACPP_KEPLER_OVERRIDE=ON
#     cmake --build build --config Release -j
#
# Environment overrides:
#     $env:LLAMA_CPP_REMOTE = "<git URL>"   (default: upstream ggerganov/llama.cpp)
#     $env:LLAMA_CPP_TAG    = "<git ref>"   (default: b1500)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Resolve-Path (Join-Path $ScriptDir '..')
$VendorDir = Join-Path $RepoRoot 'vendor\llama.cpp'

$LlamaCppRemote = if ($env:LLAMA_CPP_REMOTE) { $env:LLAMA_CPP_REMOTE } `
                  else { 'https://github.com/ggerganov/llama.cpp.git' }
$LlamaCppTag    = if ($env:LLAMA_CPP_TAG)    { $env:LLAMA_CPP_TAG }    `
                  else { 'b1500' }

if (-not (Test-Path (Join-Path $RepoRoot 'vendor'))) {
    New-Item -ItemType Directory -Force -Path (Join-Path $RepoRoot 'vendor') | Out-Null
}

if (Test-Path (Join-Path $VendorDir '.git')) {
    Write-Host "[llama.cpp-kepler] vendor/llama.cpp already present; fetching tag $LlamaCppTag..."
    Push-Location $VendorDir
    try {
        & git fetch --tags --force origin
        if ($LASTEXITCODE -ne 0) { throw "git fetch failed (exit $LASTEXITCODE)" }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "[llama.cpp-kepler] cloning $LlamaCppRemote into $VendorDir..."
    & git clone $LlamaCppRemote $VendorDir
    if ($LASTEXITCODE -ne 0) { throw "git clone failed (exit $LASTEXITCODE)" }
}

Push-Location $VendorDir
try {
    & git checkout --detach $LlamaCppTag
    if ($LASTEXITCODE -ne 0) { throw "git checkout $LlamaCppTag failed (exit $LASTEXITCODE)" }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "[llama.cpp-kepler] vendor/llama.cpp pinned to $LlamaCppTag."
Write-Host "[llama.cpp-kepler] Now configure with:"
Write-Host "    cmake -S . -B build -G `"Visual Studio 15 2017`" -A x64 -T v141,cuda=10.1 -DOMNI_LLAMACPP_KEPLER_OVERRIDE=ON"
Write-Host "    cmake --build build --config Release -j"
