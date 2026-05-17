# 🚀 OmniInference v2.0.0

**Local OpenAI- and Anthropic-compatible API gateway, with optional Kepler-era CUDA acceleration via a pinned llama.cpp.**

Drop-in `http://localhost:8080` for any tool that already speaks the
OpenAI or Anthropic API (Aider, Cline, Continue.dev, Claude Code, the
official `openai` / `anthropic` SDKs, LangChain, …). Streaming, tool
calling, image upload, bearer auth, and a 44-assertion mock-mode
test suite are all included.

[![GitHub Release](https://img.shields.io/github/v/release/walidawad42/OmniInference?style=flat-square)](https://github.com/walidawad42/OmniInference/releases)
[![Build Status](https://img.shields.io/github/workflow/status/walidawad42/OmniInference/Build?style=flat-square)](https://github.com/walidawad42/OmniInference/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](https://opensource.org/licenses/MIT)
[![CUDA](https://img.shields.io/badge/CUDA-10.1%20%7C%2011.4%2B-green?style=flat-square)](https://developer.nvidia.com/cuda-toolkit)
[![Windows](https://img.shields.io/badge/Windows-10%2F11-blue?style=flat-square)](https://www.microsoft.com/windows)
[![Linux](https://img.shields.io/badge/Linux-Ubuntu%2FPop%21OS-orange?style=flat-square)](https://ubuntu.com)

---

## 📊 What's shipped

- ✅ **OpenAI-compatible** server: `/v1/chat/completions` (streaming + tools),
  `/v1/completions`, `/v1/embeddings`, `/v1/models`.
- ✅ **Anthropic-compatible** server: `/v1/messages` (streaming + `tool_use`
  blocks), shared with the OpenAI surface so registered tools work for
  both clients.
- ✅ **Image input**: `image_url` / `image` content blocks decoded
  server-side (`stb_image`: PNG / JPEG / BMP / GIF / TGA, base64 /
  raw data-URIs). HTTP(S) URL fetching is intentionally disabled.
- ✅ **Mock mode** (`--mock`) — exercises the full HTTP surface with no
  GGUF loaded. Used by the test suite.
- ✅ **Build profile** for **CUDA 10.1 + Kepler sm_30** (Quadro K5100M
  and friends) — see below.
- ✅ **Builds CPU-only** out of the box (no nvcc required) on Ubuntu /
  Pop!_OS / WSL2.

### Model compatibility

Text-only inference on the K5100M GPU (via `OMNI_LLAMACPP_KEPLER_OVERRIDE`
and the pinned `b1500` llama.cpp) covers the late-2023 model era —
Llama 2, Mistral 7B, Phi-2, CodeLlama, Yi-6B, the original Qwen 1, and
LLaVA-1.5 / 1.6 / BakLLaVA for vision. Architectures that landed in
llama.cpp **after** Nov 2023 (Llama 3, Phi-3, Gemma, Qwen 1.5+, MiniCPM-V
series, MiniCPM-O, LFM2 / LFM2-VL, DeepSeek V2/V3, etc.) are **not**
buildable for `sm_30` because NVIDIA dropped Kepler in CUDA 11.0; for
those you need a Pascal-or-newer card.

---

## 🛠️ Building from Source

### Linux / WSL2 / macOS

```bash
cmake -S . -B build && cmake --build build -j
```

The build auto-detects CUDA via `check_language(CUDA)`. With no nvcc on the
host the project falls back to a CPU/Vulkan-only configuration; `.cu`
sources, `CUDA::cudart`, `CUDA::cublas`, and `CUDA::cublasLt` are all
skipped.

### Windows 10 / 11 (64-bit) — native MSVC

The CMakeLists has been Windows-aware since the initial port (you'll see
`if(WIN32)` blocks setting `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"`,
gating DXGI / D3D11, etc.), so a native Visual Studio build is the
expected workflow on Windows.

**Prereqs (workstation install once):**

- **Visual Studio 2017** (toolset **v141**) — required if you intend to
  use **CUDA 10.1** for the Kepler GPU path; nvcc 10.1 is only reliably
  compatible with the v141 host compiler. VS 2019/2022 are fine for
  CPU-only builds.
- **CUDA Toolkit 10.1** (Kepler hosts) **or** CUDA 11.4+ (Pascal-or-newer
  hosts). Skip entirely for CPU-only.
- **CMake** ≥ 3.18.
- **Git for Windows** (provides Git Bash, useful for the round-trip test
  shell script).

**CPU-only build (no GPU; fastest sanity check):**

```powershell
cmake -S . -B build -G "Visual Studio 15 2017" -A x64
cmake --build build --config Release -j
.\build\Release\OmniServer.exe --mock --port 8080
```

The mock server exposes the full OpenAI + Anthropic API surface without
loading any model — handy for verifying the install before you wire up
GGUFs.

**GPU build (Quadro K5100M / Kepler / CUDA 10.1):**

```powershell
# 1. Pin vendor/llama.cpp to a Kepler-compatible tag.
.\scripts\setup_llama_cpp_kepler.ps1

# 2. Configure with the v141 toolset + CUDA 10.1, opt in to the Kepler override.
cmake -S . -B build -G "Visual Studio 15 2017" -A x64 -T v141,cuda=10.1 `
      -DOMNI_LLAMACPP_KEPLER_OVERRIDE=ON

# 3. Build (Release config; Debug also works but is much slower at runtime).
cmake --build build --config Release -j

# 4. Run.
.\build\Release\OmniServer.exe --port 8080
```

**Notes specific to Windows + CUDA 10.1:**

- The `-G "Visual Studio 15 2017"` selector is for **VS 2017**. For VS
  2019 / 2022 hosts that still have v141 installed, swap to
  `-G "Visual Studio 16 2019"` or `-G "Visual Studio 17 2022"` but **keep**
  `-T v141,cuda=10.1` — that's what tells MSBuild to use the v141 host
  compiler nvcc 10.1 actually supports.
- The bash script `scripts/setup_llama_cpp_kepler.sh` works under Git
  Bash on Windows. The PowerShell port `scripts/setup_llama_cpp_kepler.ps1`
  does the same thing without needing Git Bash.
- The round-trip test suite (`tests/api/run_tests.sh`) is bash-only and
  needs Git Bash (or WSL). On a stock cmd.exe / PowerShell shell, run
  the curl smoke tests manually against `--mock` instead.

### Kepler / CUDA 10.1 build profile

The legacy support path targets cards with compute capability 3.0 such as
the **NVIDIA Quadro K5100M**. CUDA 10.1 is the last toolkit family that
natively builds for sm_30, so the build system has special-cased it:

| Toolkit version | Behaviour                                                             |
|-----------------|-----------------------------------------------------------------------|
| 10.x            | Arch list clamped to `30 50 60 70 75`; `CUDA::cublasLt` not linked; `src/turbo_quant_kernels.cu` compiled with `CUDA_STANDARD 14`; `GGML_CUDA` forced **off** in vendored llama.cpp. |
| 11.0 / 11.1     | Arch list `30 50 60 70 75 80`; full feature set.                      |
| 11.2 – 11.7     | Arch list `50 60 70 75 80 86`.                                        |
| 11.8+           | Arch list `50 60 70 75 80 86 89 90` (Ada / Hopper).                   |

Because modern llama.cpp's GGML_CUDA backend assumes CUDA ≥ 11, the build
turns it **off** automatically when CUDA major < 11. To actually use the
K5100M for matmul on a CUDA 10.x host, opt in to the Kepler override:

```bash
# 1. Pin vendor/llama.cpp to a Kepler-compatible tag (b1500 by default).
scripts/setup_llama_cpp_kepler.sh

# 2. Re-configure with the override flag.
cmake -S . -B build -DOMNI_LLAMACPP_KEPLER_OVERRIDE=ON
cmake --build build -j
```

The override flips `GGML_CUDA=ON`, forces `GGML_CUDA_F16=OFF` (Kepler has
no FP16 throughput), enables `GGML_CUDA_FORCE_DMMV=ON` (the legacy
mat-vec kernel path that supports compute capability 3.0), and bumps
`GGML_CUDA_KQUANTS_ITER` to 2. The same flags are mirrored under their
`LLAMA_*` names in case the pinned llama.cpp tag predates the GGML_*
renaming. If you re-run `cmake` without the override, the build silently
falls back to the CPU/Vulkan-only configuration.

Hardware-feature gates inside `OmniEngine` already mark Kepler as
unsupported for Tensor Cores, FP16 throughput, and Flash Attention, so
those code paths are inactive on a K5100M and you'll see the corresponding
"NO" entries in `PrintHardwareReport()`. Expect modest TPS on this class
of card; small quantised models (≤7 B with 4-bit weights and 4/2 K/V) are
the realistic operating point.

---

## 🌐 API server (OpenAI- + Anthropic-compatible)

OmniServer is a separate, headless executable that exposes OmniCore over
HTTP. Any tool that already speaks the OpenAI or Anthropic API (Aider,
Cline, Continue.dev, Claude Code, the official `openai` / `anthropic`
SDKs, LangChain, …) talks to it without code changes — you just point
the tool at `http://localhost:8080`.

```bash
# Build everything (OmniServer is built alongside OmniInference)
cmake -S . -B build
cmake --build build -j

# Smoke-test the entire HTTP surface without loading a model
./build/OmniServer --mock --port 8080
```

```bash
# OpenAI-compatible
curl http://localhost:8080/v1/chat/completions \
    -H 'Content-Type: application/json' \
    -d '{"model":"omni-mock","messages":[{"role":"user","content":"hi"}]}'

# Anthropic-compatible
curl http://localhost:8080/v1/messages \
    -H 'Content-Type: application/json' \
    -H 'anthropic-version: 2023-06-01' \
    -d '{"model":"claude-omni-mock","max_tokens":256,
         "messages":[{"role":"user","content":"hi"}]}'
```

Both endpoints support streaming (`"stream": true`), tool calling, and a
shared bearer-token auth (`--api-key sk-omni-local`). Full details:

- [`docs/api/openai.md`](docs/api/openai.md) — OpenAI endpoints, request/response
  shapes, Aider / Cline / Continue.dev / LangChain config snippets.
- [`docs/api/anthropic.md`](docs/api/anthropic.md) — Anthropic `/v1/messages`,
  Claude Code / Aider (Anthropic mode) config snippets.

A round-trip integration test against a live `--mock` server lives at
[`tests/api/run_tests.sh`](tests/api/run_tests.sh) and exercises 44
assertions covering streaming, tools, embeddings 501, CORS preflight,
bearer auth, and base64 image-decode round-trip on both API surfaces.

---

## 🎯 Quick Start

### Download (Linux/Pop!_OS)
```bash
wget https://github.com/walidawad42/OmniInference/releases/download/v2.0.0/OmniInference-Linux-x64.tar.gz
tar -xzf OmniInference-Linux-x64.tar.gz
cd OmniInference
./bin/OmniInference