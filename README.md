# Mikoto Engine

[![CodeFactor](https://www.codefactor.io/repository/github/katebea/mikoto/badge)](https://www.codefactor.io/repository/github/katebea/mikoto)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](https://opensource.org/licenses/Apache-2.0)

**Mikoto** is an open-source, Vulkan-based game engine written in modern C++. Developed as an educational project, the engine aims to provide hands-on learning experiences in graphics programming while helping me explore the capabilities of the Vulkan API.

The ``develop`` branch is a WIP for the Mikoto's new architecture. It brings some new features, like resources pools, automatic resource cleanup, and abstraction on top of render
passes, amongst other features.

---

![Mikoto Engine](Resources/Screenshots/img19.png)

---

![Mikoto Engine](Resources/Screenshots/img18.png)

---

![Mikoto Engine](Resources/Screenshots/img17.png)

---

### Feature List

| **Category**       | **Feature Name**               | **Feature Description**                                                                             | **Supported** |
|--------------------|--------------------------------|-----------------------------------------------------------------------------------------------------|---------------|
| **Core Engine**    | Model Loading                  | Load 3D models via GLTF (for gltf scenes), defaults to Assimp for other formats                     | ✔️            |
|                    | Image Loading                  | Texture/Image loading via STB_Image                                                                 | ✔️            |
|                    | Cube maps                      | Load equirectangular HDR images and use them as cube maps, requires pass to project from 2D to Cube | ✔️            |
|                    | Entity Component System        | ECS for scene/game object management                                                                | ✔️            |
|                    | Scene Serialization            | Editor scene save/load                                                                              | ❌             |
|                    | Particle System                | GPU particle simulation (fire, smoke, sparks, etc.)                                                 | ❌             |
|                    | Ray Tracing                    | Hardware accelerated RT                                                                             | ❌             |
|                    | Physics Integration            | Basic collision detection with Jolt                                                                 | ✔️            |
|                    | UI Integration (ImGui)         | Runtime + editor ImGui                                                                              | ✔️            |
|                    | Animation System               | Skeletal animation, skinning                                                                        | ❌             |
|                    | Audio Support                  | Load and play audio                                                                                 | ✔️            |
|                    | Text Rendering / Overlay       | MSDF-based text rendering                                                                           | ✔️            |
| **Visual Effects** | Clustered Forward+             | Main render path with clustered/forward+ lighting                                                   | ✔️            |
|                    | Clustered Light Culling        | Per-tile/cluster light assignment                                                                   | ✔️            |
|                    | Mesh Culling                   | CPU mesh visibility culling                                                                         | ❌             |
|                    | IBL (Image-Based Lighting)     | Diffuse irradiance + specular reflections                                                           | ❌             |
|                    | Shadows                        | Directional, point, spot shadows                                                                    | ❌             |
|                    | Cascaded Shadow Maps (CSM)     | Multi-split directional shadows                                                                     | ❌             |
|                    | Outline Pass                   | Object outlining effect                                                                             | ❌             |
|                    | Infinite Grid                  | Procedural grid for editor/world                                                                    | ✔️            |
|                    | Bloom                          | Multi-pass bright blur                                                                              | ❌             |
|                    | Depth of Field (DoF)           | DoF effect                                                                                          | ❌             |
|                    | Screen-Space Reflections (SSR) | Reflections in screen space                                                                         | ❌             |
|                    | Screen-Space GI (SSGI)         | Screen-space diffuse bounce lighting                                                                | ❌             |
| **Editor / Tools** | Gizmos (ImGuizmo)              | Move/rotate/scale gizmos                                                                            | ✔️            |
|                    | Profiling / GPU Timers         | Pass timing, pipeline stats                                                                         | ❌             |
|                    | Asset Streaming                | Task-based async resource loading                                                                   | ❌             |
|                    | Shader hot reloading           | Shader hot reload                                                                                   | ❌             |
|                    | Asset hot reloading            | Asset hot reload for scripts, etc.                                                                  | ✔️            |


### **Platform Support**

| Platform / Toolchain          | Architecture | Status             | Notes |
|-------------------------------|--------------|--------------------|-------|
| Ubuntu 24.04 LTS (GCC 13.3.0) | x86_64       | Supported (Tested) | -     |
| Other Linux Distros           | x86_64       | Untested           | -     |
| Windows (MSVC)                | x86_64       | Supported (Tested) | -     |
| Windows (MinGW-w64)           | x86_64       | Untested           | -     |

> Note: This project has been tested on Ubuntu 24.04 for Linux compatibility. While it 
> works properly on Windows, other Linux distributions are currently untested.
---

## Requirements

### Software Requirements

- **CMake 3.22+** – Required for configuring and building the project.
- **Vulkan SDK** – Install from the official [LunarG SDK](https://vulkan.lunarg.com/).
- **C++20-compatible compiler** – Tested with **GCC 13.3.0**; other C++20 compilers should work but are untested.
- **Visual Studio 2022 onwards (Windows)** – Recommended IDE and toolchain for Windows builds.

---

## Folder Structure

- **`Resources/`**: Resources screenshots and some models to play around with.
- **`Mikoto-Engine/`**: The core engine that powers the editor.
- **`Mikoto-Editor/`**: The editor project for creating and managing game scenes.
- **`Mikoto-Sandbox/`**: A sample project that demonstrates some of the engine's features.
- **`Mikoto-Tests/`**: Contains lists of tests against the core engine.
- **`Mikoto-Apps/`**: Standalone applications that showcase Mikoto features

>Some models used for demos were downloaded from Morgan McGuire's Computer Graphics Archive https://casual-effects.com/data

## Building

Mikoto is currently verified to build on both **Windows** and **Linux**.

The build setup is managed through **CMake**, with platform-specific instructions covering dependencies, prerequisites, and development environments.

For a complete guide, including:

* Windows and Linux prerequisites
* Lua setup
* Linux dependency installation
* Repository and submodule setup
* CMake configuration and build commands
* Visual Studio and CLion workflows

see the **[Building Guide](BUILDING.md)**.

> **Windows:** Visual Studio and the Vulkan SDK are required.
> **Linux:** Required system dependencies must be installed before building.

## Dependencies

The development of Mikoto Engine is made possible thanks to these fantastic third-party libraries:

| Library                       | Description                                   | Link                                                                                                                |
|-------------------------------|-----------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| **FMT**                       | Modern C++ formatting library                 | [fmtlib/fmt](https://github.com/fmtlib/fmt)                                                                         |
| **GLEW**                      | OpenGL Extension Wrangler Library             | [GLEW](https://glew.sourceforge.net/)                                                                               |
| **GLFW**                      | Multi-platform library for window management  | [glfw/glfw](https://github.com/glfw/glfw)                                                                           |
| **GLM**                       | OpenGL Mathematics library                    | [g-truc/glm](https://github.com/g-truc/glm)                                                                         |
| **ImGui**                     | Immediate Mode GUI library                    | [ocornut/imgui](https://github.com/ocornut/imgui)                                                                   |
| **Spdlog**                    | Fast C++ logging library                      | [gabime/spdlog](https://github.com/gabime/spdlog)                                                                   |
| **EnTT**                      | Fast and efficient Entity-Component System    | [skypjack/entt](https://github.com/skypjack/entt)                                                                   |
| **Volk**                      | Meta-loader for Vulkan API                    | [zeux/volk](https://github.com/zeux/volk)                                                                           |
| **Assimp**                    | Asset importer library                        | [assimp/assimp](https://github.com/assimp/assimp)                                                                   |
| **VulkanMemoryAllocator**     | Memory allocation for Vulkan resources        | [GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) |
| **ImGuizmo**                  | Gizmo manipulator for ImGui                   | [CedricGuillemet/ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)                                             |
| **yaml-cpp**                  | YAML parser and emitter for C++               | [jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp)                                                               |
| **nativefiledialog-extended** | File dialog library for native UIs            | [btzy/nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended)                                 |
| **JoltPhysics**               | Physics engine library                        | [jrouwe/JoltPhysics](https://github.com/jrouwe/JoltPhysics)                                                         |
| **tomlplusplus**              | TOML configuration file parser for C++        | [marzer/tomlplusplus](https://github.com/marzer/tomlplusplus)                                                       |
| **stb_image**                 | Image loading library                         | [nothings/stb](https://github.com/nothings/stb.git)                                                                 |
| **msdf-atlas-gen**            | Multi-channel signed distance field generator | [Chlumsky/msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen.git)                                           |
| **TaskFlow**                  | Moder C++ Task library                        | [taskflow/taskflow](https://github.com/taskflow/taskflow)                                                           |
| **Sol2**                      | Moder C++ Library for Scripting with Lua      | [ThePhD/sol2](https://github.com/ThePhD/sol2)                                                                       |

## Networking
Mikoto includes a networking layer built on top of ASIO to support TCP sockets.
The engine supports both HTTP and HTTPS connections, but HTTPS requires OpenSSL to be installed on your system.
If OpenSSL is not available the engine falls back to HTTP support only.

## Profiling with Tracy

Mikoto integrates the Tracy Profiler for CPU, GPU, and memory profiling.
>When Tracy instrumentation is enabled, the Tracy Profiler GUI must be running.
If the profiler is not connected, Tracy will continue to run internally, which can lead to memory leaks.
> By default Tracy is disabled, in order to enabled one must compile with the ``MIKOTO_ENABLE_TRACY_PROFILING``
> flag enabled in the ``CmakeLists.txt`` file. See the [Editor CMake](Mikoto-Editor/CMakeLists.txt) file for reference.
> 
> User will need to run Mikoto with Tracy's Profiler v3.3.0 which is the version used by the engine.

## Slang in Mikoto

Mikoto uses the **Slang shading language** for runtime shader compilation (Reflection is still done by spirv-reflect).  
The engine ships with **precompiled Slang binaries**, so no manual setup is required to use Slang with Mikoto.

If you prefer to download or update Slang manually, you can find the official releases here:

- **Slang GitHub Repository:** https://github.com/shader-slang/slang  
- **Latest Slang Release Used by Mikoto (v2026.10):** https://github.com/shader-slang/slang/releases/tag/v2026.10

Mikoto will automatically use the bundled version, but replacing it with another official release is supported as long as the Slang directory structure remains unchanged.

## Goals

The primary goal of Mikoto Engine is to serve as a learning platform for exploring modern graphics programming techniques. Features are implemented progressively as new concepts and ideas are explored.

---

## Special Thanks

The development of Mikoto has been inspired by the work of the following:

- **Joey De Vries** for the incredible [LearnOpenGL](https://learnopengl.com/) tutorials.
- **Yan Chernikov** for his insightful [YouTube videos](https://www.youtube.com/@TheCherno).
- **Cem Yuksel** for his educational [graphics programming videos](https://www.youtube.com/@cem_yuksel/videos).
- **Jason Gregory** for the book *[Game Engine Architecture](https://www.gameenginebook.com/)*.
- **Matt Pharr, Wenzel Jakob, Greg Humphreys** for *[Physically Based Rendering: From Theory to Implementation](https://www.pbr-book.org/)*.
- **Sascha Willems** for the [Vulkan examples repository](https://github.com/SaschaWillems/Vulkan).
- **Marco Castorina and Gabriel Sassone** for the Mastering Graphics Programming with Vulkan Book.

---

## Future Development

Mikoto Engine is still in its early stages, and additional features and optimizations will be added over time.
Feedback is always welcome!

---

## License

This project is licensed under the **Apache License 2.0**.

You are free to use, modify, and distribute this software under the terms of the Apache License 2.0. A copy of the license is included in the repository.

For the full license text, see the [LICENSE](LICENSE) file in the repository.

