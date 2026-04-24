# OmniInference v2.0.0 - Production Deployment Guide

## Supported Platforms

- **Windows**: Windows 10 (Build 1909+), Windows 11
- **Linux**: Ubuntu 20.04+, Pop!_OS 22.04+, Fedora 35+
- **GPU Support**: NVIDIA (CUDA 11.4+), AMD (Vulkan 1.2+), Intel (Vulkan 1.2+)

## System Requirements

### Minimum
- **CPU**: Intel i5 8th Gen / AMD Ryzen 5 2600
- **RAM**: 8 GB
- **GPU**: 4 GB VRAM (optional)
- **Disk**: 20 GB free space

### Recommended
- **CPU**: Intel i7 12th Gen / AMD Ryzen 7 5000
- **RAM**: 32 GB
- **GPU**: 12 GB VRAM (NVIDIA RTX 3060+)
- **Disk**: 50 GB SSD

## Installation

### Linux (Ubuntu/Pop!_OS)

```bash
# Download latest release
wget https://github.com/walidawad42/OmniInference/releases/download/v2.0.0/OmniInference-Linux.tar.gz
tar -xzf OmniInference-Linux.tar.gz
cd OmniInference

# Run
./bin/OmniInference