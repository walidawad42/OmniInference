# ---

**OmniInference Engine: Ultimate Deterministic Master Prompt**

**Role:** Principal C++ GPU Architect & Mathematical Optimization Lead

**Agent Identity:** Copilot Github

**Goal:** Build a self-contained, high-performance C++ tool and Dear ImGui GUI (the "OmniInference Engine") for local LLM/MLLM execution with zero external dependencies.

### ---

**1\. Agent Environment Protocol: Automated Setup**

**Copilot Github** is mandated to manage the complete development and runtime ecosystem without user intervention:

* **Automated Setup**: Download, verify, and integrate required drivers and toolkits (CUDA 11.4+ or CUDA 10.1 for Kepler K5100M compatibility, and Vulkan 1.2+ headers).  
* **Dependency Management**: Strictly integrate GCC 8.5+ or MSVC equivalents to support TurboQuant Plus Haar kernels.  
* **Architectural Span**: Ensure full support for hardware from 2012 (Kepler SM\_30) to 2024+ (Blackwell/Hopper SM\_90).

### **2\. Hybrid Compute Management Matrix**

The engine must support three distinct, deterministic execution modes selectable via the GUI:

* **Mode A: CPU & RAM**: 100% C++ execution utilizing AVX/AVX2/AVX-512 optimizations; no GPU required.  
* **Mode B: GPU & RAM**: Standard offloading where weights and KV-cache reside in VRAM, with overflow to System RAM via the Deterministic Buffer.  
* **Mode C: CPU & GPU & RAM**: Active hybrid inference using Unified Virtual Memory (UVM) or GGML System-RAM fallback to balance loads across all available silicon.

### **3\. The Universal Standard: Google TurboQuant Plus**

* **Mandatory Engine**: Integrate Google TurboQuant Plus as the exclusive KV-cache optimization engine.  
* **Accuracy Protocol**: Utilize PolarQuant (recursive polar transformation) and QJL (Quantized Johnson-Lindenstrauss) residual correction to ensure lossless 3-bit compression and 100% "Needle-in-a-Haystack" retrieval at 32k+ context.  
* **RotorQuant Blocking**: Strictly BLOCK block-diagonal methods. All rotations must be global (Haar-based) to prevent geometric distortion and memory spikes.  
* **Asymmetric Precision**: Default to 4-bit Key (K) for structural integrity and 2-bit Value (V) for semantic density.

### **4\. Deterministic Guardrails & Memory Management**

* **VRAM Safety Floor**: A user-controlled slider (200MB–1GB) to ensure OS stability and prevent driver timeouts.  
* **Overflow Cascade**: Automatic VRAM → RAM → Disk cascade to prevent OOM (Out-of-Memory) crashes.  
* **Context Shifting ( Killer)**: Implement native C++ prefix-matching logic to prevent reprocessing the prompt when only the context tail changes, exceeding efficiency.

### **5\. Unified 9-Panel GUI & API**

* **Real-time Monitoring**: A 9-panel ImGui interface showing VRAM/RAM consumption, TurboQuant compression ratios, and throughput (TPS).  
* **Local Copilot API**: Maintain a production-grade, OpenAI Compatibility & Anthropic Compatibility for external tool integration ,Function calling, chat , Tool use & agentic workflows.  
* **Multimodal Support**: Native integration for VLMs/MLLMs (Llama-Vision, MiniCPM-O 4.5) and UltraRAG 3.0.

---

**Ready to Build?** Copilot Github, initialize the environment, apply the **Optimization Matrix ($M$)**, and execute the build scripts for the detected platform.