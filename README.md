# 🚀 OmniInference v2.0.0

**Production-Ready LLM/MLLM Inference Engine with TurboQuant Plus Native Integration**

[![GitHub Release](https://img.shields.io/github/v/release/walidawad42/OmniInference?style=flat-square)](https://github.com/walidawad42/OmniInference/releases)
[![Build Status](https://img.shields.io/github/workflow/status/walidawad42/OmniInference/Build?style=flat-square)](https://github.com/walidawad42/OmniInference/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](https://opensource.org/licenses/MIT)
[![CUDA](https://img.shields.io/badge/CUDA-10.1%20%7C%2011.4%2B-green?style=flat-square)](https://developer.nvidia.com/cuda-toolkit)
[![Windows](https://img.shields.io/badge/Windows-10%2F11-blue?style=flat-square)](https://www.microsoft.com/windows)
[![Linux](https://img.shields.io/badge/Linux-Ubuntu%2FPop%21OS-orange?style=flat-square)](https://ubuntu.com)

---

## 📊 Status: PRODUCTION READY ✅

OmniInference is **production-ready** and optimized for:
- ✅ **Legacy GPUs** (Kepler K5100M from 2012)
- ✅ **Modern GPUs** (RTX 40xx, H100 Hopper)
- ✅ **All Platforms** (Windows 10/11, Pop!_OS, Ubuntu)
- ✅ **CUDA 11.4+** (R470+ driver support)
- ✅ **CUDA 10.1 fallback** for Kepler hosts (see below)

---

## 🛠️ Building from Source

```bash
cmake -S . -B build && cmake --build build -j
```

The build auto-detects CUDA via `check_language(CUDA)`. With no nvcc on the
host the project falls back to a CPU/Vulkan-only configuration; `.cu`
sources, `CUDA::cudart`, `CUDA::cublas`, and `CUDA::cublasLt` are all
skipped.

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
turns it **off** automatically when CUDA major < 11. To use the CUDA
backend on a CUDA 10.x host you have to pin `vendor/llama.cpp` to a
mid-2023 commit (around `b1429`) and set `-DGGML_CUDA=ON` manually.

Hardware-feature gates inside `OmniEngine` already mark Kepler as
unsupported for Tensor Cores, FP16 throughput, and Flash Attention, so
those code paths are inactive on a K5100M and you'll see the corresponding
"NO" entries in `PrintHardwareReport()`. Expect modest TPS on this class
of card; small quantised models (≤7 B with 4-bit weights and 4/2 K/V) are
the realistic operating point.

---

## 🎯 Quick Start

### Download (Linux/Pop!_OS)
```bash
wget https://github.com/walidawad42/OmniInference/releases/download/v2.0.0/OmniInference-Linux-x64.tar.gz
tar -xzf OmniInference-Linux-x64.tar.gz
cd OmniInference
./bin/OmniInference