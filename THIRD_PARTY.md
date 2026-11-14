# Third-Party Libraries

Mikoto uses a number of open-source libraries for the engine, details listed below:


| Library                       | Description                                                                                           | Link                                                                                                                |
|-------------------------------|-------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| **Asio**                      | C++ networking and asynchronous I/O library                                                           | [chriskohlhoff/asio](https://github.com/chriskohlhoff/asio)                                                         |
| **EnTT**                      | Entity Component System used for scene and game object management                                     | [skypjack/entt](https://github.com/skypjack/entt)                                                                   |
| **Assimp**                    | Asset importer used for loading a wide range of model formats                                         | [assimp/assimp](https://github.com/assimp/assimp)                                                                   |
| **fastgltf**                  | glTF and GLB importer                                                                                | [spnda/fastgltf](https://github.com/spnda/fastgltf)                                                               |
| **stb_image**                 | Image and HDR image loading                                                                           | [nothings/stb](https://github.com/nothings/stb)                                                                     |
| **KTX-Software**              | Khronos texture container library and tooling for GPU texture assets                                  | [KhronosGroup/KTX-Software](https://github.com/KhronosGroup/KTX-Software)                                           |
| **JoltPhysics**               | Rigid-body physics and collision detection                                                            | [jrouwe/JoltPhysics](https://github.com/jrouwe/JoltPhysics)                                                         |
| **miniaudio**                 | Audio playback and audio resource handling                                                            | [mackron/miniaudio](https://github.com/mackron/miniaudio)                                                           |
| **ozz-animation**             | Skeletal animation runtime and animation processing                                                   | [guillaumeblanc/ozz-animation](https://github.com/guillaumeblanc/ozz-animation)                                     |
| **FreeType**                  | Font loading and glyph generation                                                                     | [freetype/freetype](https://github.com/freetype/freetype)                                                           |
| **SlugHorn**                  | Curve/band atlas generation for the Slug text backend; reference shader adapted to Slang/RHI (MIT) | [AlphaPixel/slughorn](https://github.com/AlphaPixel/slughorn)                                                       |
| **msdfgen**                   | Multi-channel signed distance field generation for text rendering                                     | [Chlumsky/msdfgen](https://github.com/Chlumsky/msdfgen)                                                             |
| **msdf-atlas-gen**            | MSDF glyph atlas generation                                                                           | [Chlumsky/msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen)                                               |
| **Sol2**                      | C++ binding library used to integrate Lua scripting                                                   | [ThePhD/sol2](https://github.com/ThePhD/sol2)                                                                       |
| **SDL3**                      | Cross-platform window creation, input, and platform integration                                       | [libsdl-org/SDL](https://github.com/libsdl-org/SDL)                                                               |
| **GLM**                       | Mathematics library for graphics and 3D calculations                                                  | [g-truc/glm](https://github.com/g-truc/glm)                                                                         |
| **Dear ImGui**                | Immediate-mode GUI framework used by the editor and runtime                                           | [ocornut/imgui](https://github.com/ocornut/imgui)                                                                   |
| **ImGuiColorTextEdit**        | Script editor and previews; MIT, pinned to `133614b0d5e1008527a26f46a93fdb1d751ca115`                 | [goossens/ImGuiColorTextEdit](https://github.com/goossens/ImGuiColorTextEdit)                                       |
| **ImGuizmo**                  | Transform manipulation gizmos integrated with ImGui                                                   | [CedricGuillemet/ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)                                             |
| **Volk**                      | Vulkan API meta-loader                                                                                | [zeux/volk](https://github.com/zeux/volk)                                                                           |
| **Vulkan Memory Allocator**   | GPU memory allocation and resource management for Vulkan                                              | [GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) |
| **D3D12 Memory Allocator**    | GPU memory allocation and resource management for Direct3D 12                                         | [GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)   |
| **SPIRV-Cross**               | SPIR-V parsing, reflection, and shader translation                                                    | [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)                                             |
| **SPIRV-Reflect**             | SPIR-V shader reflection and resource layout inspection                                               | [KhronosGroup/SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)                                         |
| **Slang**                     | Shader language and compiler used for shader development                                              | [shader-slang/slang](https://github.com/shader-slang/slang)                                                         |
| **Lettier film grain**         | Film-grain formula adapted in `PostProcessMaterial_Frag.slang` (BSD-3-Clause) | [Film grain tutorial](https://lettier.github.io/3d-game-shaders-for-beginners/film-grain.html) |
| **Tracy**                     | Real-time CPU, GPU, memory, and frame profiler                                                        | [wolfpld/tracy](https://github.com/wolfpld/tracy)                                                                   |
| **FMT**                       | Modern C++ formatting library                                                                         | [fmtlib/fmt](https://github.com/fmtlib/fmt)                                                                         |
| **spdlog**                    | Fast C++ logging library                                                                              | [gabime/spdlog](https://github.com/gabime/spdlog)                                                                   |
| **yaml-cpp**                  | YAML parser and emitter                                                                               | [jbeder/yaml-cpp](https://github.com/jbeder/yaml-cpp)                                                               |
| **tomlplusplus**              | TOML parser used for configuration and structured data                                                | [marzer/tomlplusplus](https://github.com/marzer/tomlplusplus)                                                       |
| **nlohmann/json**             | JSON parsing and serialization for modern C++                                                         | [nlohmann/json](https://github.com/nlohmann/json)                                                                   |
| **nativefiledialog-extended** | Native file and directory dialogs                                                                     | [btzy/nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended)                                 |
| **portable-file-dialogs**     | Portable native GUI file dialogs                                                                      | [samhocevar/portable-file-dialogs](https://github.com/samhocevar/portable-file-dialogs)                             |
| **Taskflow**                  | Task-based parallel execution library                                                                 | [taskflow/taskflow](https://github.com/taskflow/taskflow)                                                           |
| **unordered_dense**           | High-performance dense hash map and hash set                                                          | [martinus/unordered_dense](https://github.com/martinus/unordered_dense)                                             |
| **cpptrace**                  | Stack trace generation and debugging utilities for C++                                                | [jeremy-rifkin/cpptrace](https://github.com/jeremy-rifkin/cpptrace.git)                                             |
## ozz armature visualization

The bone mesh, bone coordinate frame, and ambient shading in `DebugModule.cc`
and `Armature_Frag.slang` adapt the ozz sample renderer:
https://github.com/guillaumeblanc/ozz-animation/tree/master/samples/framework/internal

Copyright (c) Guillaume Blanc

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
