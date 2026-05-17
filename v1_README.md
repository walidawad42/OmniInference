# 🚀 OmniInference v2.0.0

**Production-Ready LLM/MLLM Inference Engine with TurboQuant Plus Native Integration**

[![Build Status](https://github.com/walidawad42/OmniInference/workflows/Build/badge.svg)](https://github.com/walidawad42/OmniInference/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Windows](https://img.shields.io/badge/Windows-10%2F11-blue)](https://www.microsoft.com/windows)
[![Linux](https://img.shields.io/badge/Linux-Ubuntu%2FPop%21OS-orange)](https://ubuntu.com)

---

## ✨ Key Features

### 🎯 Core Capabilities
- **100% C++/C** - No Python runtime required
- **TurboQuant Plus** - Native polar quantization with QJL correction
- **Cross-Platform** - Windows 10/11, Ubuntu 20.04+, Pop!_OS 22.04+
- **Multi-Backend** - NVIDIA CUDA 11.4+, AMD/Intel Vulkan 1.2+
- **Tool Calling** - OpenAI-compatible function calling API
- **RAG Integration** - UltraRAG 3.0 + MiniCPM-O 4.5 multimodal

### 🔄 Integrations
- **ComfyUI** - Local image/video generation workflows
- **Dify** - Open-source LLM app development platform
- **GitIngest** - Repository ingestion and synthesis
- **GitReverse** - Code repository analysis

### ⚡ Performance
- **6x KV-Cache Compression** - Via TurboQuant 3-bit mode
- **+20% Throughput** - With attention-gated delta networks
- **Needle-in-Haystack** - 95%+ accuracy at 32k context
- **CUDA 11.4 Compatible** - Support for legacy Kepler (K5100M) to Blackwell

---

## 📋 System Requirements

### Minimum
- **OS**: Windows 10 Build 1909+, Ubuntu 20.04+, Pop!_OS 22.04+
- **CPU**: Intel i5-8400 / AMD Ryzen 5 2600
- **RAM**: 8 GB
- **Disk**: 20 GB SSD
- **GPU**: Optional (4GB VRAM minimum if used)

### Recommended
- **CPU**: Intel i7-12700K / AMD Ryzen 7 5800X
- **RAM**: 32 GB
- **GPU**: NVIDIA RTX 3090 / RTX 4090 (12-24GB VRAM)
- **Disk**: 100 GB NVMe SSD

---

## 🚀 Quick Start

### Linux (Ubuntu/Pop!_OS)

```bash
# Download latest release
wget https://github.com/walidawad42/OmniInference/releases/download/v2.0.0/OmniInference-Linux-x64.tar.gz
tar -xzf OmniInference-Linux-x64.tar.gz
cd OmniInference

# Run
./bin/OmniInference