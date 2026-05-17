This final, high-accuracy master prompt establishes **Google TurboQuant Plus** as the non-negotiable standard for all hardware. By removing RotorQuant, the engine eliminates geometric distortion in the KV-cache, ensuring that every token generated is mathematically precise.

### **Omni-Inference Engine: Ultimate Deterministic Master Prompt**

**Role:** Principal C++ GPU Architect & Mathematical Optimization Lead.

**Goal:** Build a self-contained, high-performance C++ tool and **Dear ImGui** GUI (the "Omni-Inference Engine") for local LLM/MLLM execution with **zero external dependencies**.

#### ---

**1\. The Universal Standard: Google TurboQuant Plus**

* **Mandatory Engine:** Integrate **Google TurboQuant Plus** as the exclusive KV-cache optimization engine for all Laptops, PCs, and GPUs.  
* **Accuracy Protocol:** Utilize **PolarQuant** (recursive polar transformation) and **QJL (Quantized Johnson-Lindenstrauss)** residual correction to ensure **lossless 3-bit compression**.  
* **Error Elimination:** Strictly **BLOCK** RotorQuant or block-diagonal methods. All rotations must be global (Haar-based) to ensure 100% "Needle-in-a-Haystack" retrieval at 32k+ context.  
* **Asymmetric Precision:** Default to **Turbo-V (Value)** at 2-bits and **Turbo-K (Key)** at 4-bits for the optimum balance between memory savings and coding logic fidelity.

#### **2\. Mathematical Optimization Design Matrix ($M$)**

* **Resource Solver:** Execute an internal solver that maps hardware constraints to model weights.  
* **Objective:** Maximize $TPS$ (Tokens Per Second) while enforcing $VRAM\_{Total} \- 1GB \\ge VRAM\_{Model} \+ VRAM\_{KV\\\_Cache}$.  
* **Precision Tuning:** Automatically select the maximum possible TurboQuant bit-depth (3-bit or 4-bit) that satisfies the VRAM constraint without offloading to RAM.

#### **3\. Multi-Generation Infrastructure**

* **Automated Tooling:** Detect hardware and download the best-fit binaries:  
  * **NVIDIA:** Legacy Kepler (R470 / CUDA 11.8) to Blackwell (R595+ / CUDA 13).  
  * **AMD:** Legacy GCN 1.0 to RDNA 4 (Vulkan 1.2 / ROCm 7).  
  * **Intel:** HD 4600 to Xe3 (Vulkan 1.2 / SYCL).  
* **Deterministic Buffer:** Enforce a **1GB VRAM Buffer** for OS stability. Automated overflow to **System RAM** if model size $+ 32k$ context exceeds VRAM.

#### **4\. Unified UI & Parameter Control**

* **Sampling:** Precise sliders for **Temperature (T)** and **TopP (P)**.  
* **Inference:** Control over n\_gpu\_layers, n\_threads, context\_size, flash\_attn (Auto-OFF for legacy), and rope\_freq.  
* **Sparse Decoding:** Integrated **Attention-Gated Delta Net** support to boost long-context decoding speeds by **\+20%**.

#### **5\. Model & API Versatility**

* **Supported Engines:** GGUF models (LLM, SLM, MoE, LAM).  
* **Multimodal:** Support for **VLMs/MLLMs** (Llama-Vision, Qwen-VL) and **Computer Vision** (ViT, SAM, YOLO26).  
* **Local Copilot:** Built-in OpenAI-compatible API (/v1/chat/completions) for local IDE integration.

### ---

**System Target Matrix (2026 Strategy)**

| Hardware Tier | Standard KV Engine | Optimization Logic |
| :---- | :---- | :---- |
| **Legacy (e.g., K5100M)** | **TurboQuant Plus** | 6x memory reduction; maintains accuracy on 2012 silicon. |
| **Newer Laptops (RTX 40/50)** | **TurboQuant Plus** | Maximum throughput; support for 128k+ windows. |
| **Intel iGPU (HD 4600\)** | **TurboQuant Plus (Vulkan)** | Offloads KV-cache tasks to shared memory for stability. |

**Final Instruction for Agent:** Code must be pure C++. No Python. The tool must be brutally efficient and deterministic. Ensure the **Mathematical Optimization Matrix** prevents all "faulty" configurations that compromise generation quality.