# 🛰️ KEPLAR-VK
*Vulkan C++ Framework*

---

## ✨ Project Overview

Keplar_vk is a cross-platform, modular real-time rendering framework built with C++17 and Vulkan 1.4+.
It demonstrates Vulkan API usage with a focus on resource management, synchronization, and core graphics concepts.
This framework provides a well structured foundation for building high performance graphics applications with explicit control over swapchains, command buffers, shaders, pipelines, buffers, descriptor sets, render passes, framebuffers, and synchronization primitives.

---

## ⚙️ Features

- Cross-platform architecture with platform abstraction
  - Windows support implemented
  - Linux support in progress
- Modern C++17 design with RAII-based resource lifetime management
- Explicit control over Vulkan resources, synchronization, and memory management
- Precompiled SPIR-V shaders for optimal runtime performance
- glTF 2.0 compatible asset and material workflow using tinygltf
- ImGui support with a dedicated render pass
- Vulkan validation layers with async logging enabled in debug builds for enhanced error checking and diagnostics
- Native MSAA support for enhanced rendering quality
- Extensible core designed for advanced rendering techniques 

---

## 📁 Project Structure

- `/core` — Core application framework (engine entry, managers)   
- `/platform` — Cross-platform windowing & OS abstraction  
- `/vulkan` — Vulkan-specific modules (swapchain, command buffers, pipelines, etc.)  
- `/graphics` — Graphics abstraction layer (texture, MSAA, ImGui, camera, etc) 
- `/shaders` — GLSL shader programs for Vulkan pipelines  
- `/resources` — Assets (SPIR-V binaries, textures, models, etc.)  
- `/utils` — utilities (logging, thread pool, helpers)  
- `/external` — Third-party dependencies (GLM, tinygltf, ImGui) 
- `/samples` — example applications built with the framework  
- `/screenshots` — Sample rendering screenshots
- `/CMakeLists.txt` — Build configuration  
- `/README.md` — This document  
- `/LICENSE` — MIT License  

---

## 🛠️ Build Instructions

### Prerequisites
- CMake (version 3.15 or higher)  
- C++17 compatible compiler (e.g., GCC 9+, Clang 10+, MSVC 2019+)  
- Vulkan SDK version 1.4.304 installed and `VULKAN_SDK` environment variable set correctly  
- Git (to clone the repository)

### Build Steps
1. **Clone the repository**  
   ```bash
   git clone https://github.com/yashPat98/keplar_vk
   cd keplar_vk
2. **Create and navigate to the build directory**
   ```bash
   mkdir build
   cd build
   ```
3. **Configure the build with the desired sample**  
Choose one sample from the following options: `TRIANGLE`, `TEXTURE`, `GLTFLOADER`, `PBR`
   ```bash
   cmake .. -DKEPLAR_SAMPLE=TRIANGLE
   ```

4. **Build the project:**
   ```bash
   cmake --build . --config Release
   ```
5. **Run the executable**
   ```bash
   .\keplar.exe
   ```

---

## 📄 License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---
   
