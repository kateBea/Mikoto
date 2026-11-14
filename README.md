# Mikoto Engine

[![CodeFactor](https://www.codefactor.io/repository/github/katebea/mikoto/badge)](https://www.codefactor.io/repository/github/katebea/mikoto)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](LICENSE)

Mikoto is an open-source C++20 game engine for exploring graphics programming techniques and software architecture. It supports Vulkan and Direct3D 12 rendering backends
through an abstraction layer called RHI (Render Hardware Interface), OpenGL and D3D11 implementations are experimental and not any close to what Vulkan and D3D12 have to offer.

This project is under active development. Rendering features, editor workflows, and engine APIs continue to evolve.

[Features](#features) · [Building](#building) · [Configuration](#configuration) · [Troubleshooting](#troubleshooting) · [Dependencies](#dependencies) · [License](#license)

![Mikoto editor](Resources/Screenshots/img28.png)

<details>
<summary>More screenshots</summary>

![Mikoto example](Resources/Screenshots/img30.png)
![Mikoto example](Resources/Screenshots/img27.png)
![Mikoto example](Resources/Screenshots/img26.png)
![Mikoto example](Resources/Screenshots/img24.png)
![Mikoto example](Resources/Screenshots/img23.png)
![Mikoto example](Resources/Screenshots/img22.png)
![Mikoto example](Resources/Screenshots/img21.png)
![Mikoto example](Resources/Screenshots/img19.png)
![Mikoto example](Resources/Screenshots/img18.png)

</details>

## Features

Mikoto is built around Vulkan and Direct3D 12 renderers and a collection of focused, open-source libraries. The currently supported features include:

**Core**

* **Entity Component System:** Scene and game object management with [skypjack/entt](https://github.com/skypjack/entt).
* **3D Model Loading:** glTF support through [spnda/fastgltf](https://github.com/spnda/fastgltf), with [assimp/assimp](https://github.com/assimp/assimp) used for support for other formats.
* **Image Loading:** Texture and image loading through [nothings/stb](https://github.com/nothings/stb), with KTX texture support through [KhronosGroup/KTX-Software](https://github.com/KhronosGroup/KTX-Software).
* **HDR & Environment Maps:** HDR image loading also with `stb_image`, with equirectangular-to-cubemap conversion.
* **Physics:** Rigid-body physics, collision handling, and vehicle constraint with [jrouwe/JoltPhysics](https://github.com/jrouwe/JoltPhysics).
* **Audio:** Audio playback and mixing through [mackron/miniaudio](https://github.com/mackron/miniaudio), with spatial audio through [ValveSoftware/steam-audio](https://github.com/ValveSoftware/steam-audio).
* **Scripting:** Lua scripting through [ThePhD/sol2](https://github.com/ThePhD/sol2) and C# scripting through .NET hosting (scripting with C++ planned for later).
* **Skeletal Animation:** Animation playback, blending, controllers, and inverse kinematics through [guillaumeblanc/ozz-animation](https://github.com/guillaumeblanc/ozz-animation).
* **Runtime Canvas UI:** Hierarchical canvas widgets with buttons, toggles, sliders, sprites, and text, including input handling.
* **Video Playback:** Experimental H.264 playback through Vulkan Video, with MP4 demuxing through [lieff/minimp4](https://github.com/lieff/minimp4) and synchronized audio playback.

**Rendering**

* **Vulkan & Direct3D 12:** Rendering backends behind a shared RHI and frame graph. Support for OpenGL and D3D11 is experimental as they do not fit well in the frame graph yet.
* **Shader Compilation & Caching:** [shader-slang/slang](https://github.com/shader-slang/slang) compilation with a shader-library cache for compiled SPIR-V and DXIL.
* **Clustered Forward+:** Physically based shading with clustered lighting, light culling, instancing, and GPU indirect drawing.
* **Ray Tracing & Path Tracing:** Hardware ray-traced shadows, reflections, GI, and AO, plus progressive path tracing with temporal accumulation and depth of field, following [PBRT](https://pbr-book.org/4ed/).
* **Lighting & Shadows:** Point, spot, and directional lights, cascaded shadows, optional raster soft shadows, and path-traced mesh and rectangular area lights.
* **Material Models:** Metallic/roughness PBR, dielectric and conductor materials, and subsurface scattering, with editable optical properties.
* **Height Mapping:** Bump, parallax, and displacement mapping with material controls.
* **HDR & Atmospheric Scattering:** HDR rendering, image-based lighting, skyboxes, and experimental atmospheric scattering with aerial perspective.
* **Post Processing:** Bloom, tone mapping, SSAO, depth of field, vignette, film grain, and composable post-process materials.
* **Terrain:** Experimental heightmap terrain with GPU geometry generation, procedural noise, and landscape import and preview tools.
* **Particles:** Configurable emitters with CPU or experimental GPU simulation, playback controls, and reusable particle assets.
* **Infinite Grid:** Procedural editor/world grid.
* **Text Rendering:** MSDF text through [Chlumsky/msdfgen](https://github.com/Chlumsky/msdfgen) and experimental Slug text through [AlphaPixel/slughorn](https://github.com/AlphaPixel/slughorn), with Unicode and colour emoji support.

**Editor**

* **ImGui Integration:** Dockable editor panels through [ocornut/imgui](https://github.com/ocornut/imgui), with separate Scene and Game views and play/pause/simulation controls.
* **Transform Gizmos:** Move, rotate, and scale tools through [CedricGuillemet/ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo), with smooth camera transitions and orbit/free-fly navigation.
* **Viewport Selection & Drag-and-Drop:** Object picking, visible rectangle selection, and texture/material drops onto scene meshes or environments.
* **Asset & Material Tools:** Content browser thumbnails, material previews and persistence, prefab workflows, and asset hot reloading.
* **Animation & Debug Views:** Animation graph and timeline tools, armature overlays, and rendering debug views including SSAO.
* **Profiling:** Performance plots through [epezent/implot](https://github.com/epezent/implot) and optional CPU/GPU profiling through [wolfpld/tracy](https://github.com/wolfpld/tracy).

For a complete list of third-party libraries and how they are used throughout the engine, see the **[Third-Party Libraries](THIRD_PARTY.md)** page. See [FEATURE.md](FEATURE.md) for the detailed feature checklist and roadmap.

## **Platform Support**

Mikoto is currently developed and tested on **Windows** and **Ubuntu 24.04 LTS**.
The primary development toolchains are **MSVC** on Windows and **GCC 13.3.0** on Linux.

| Platform        | Architecture | Compiler | CMake Generator      | Rendering API       |
|-----------------|--------------|----------|----------------------|---------------------|
| **Windows 10+** | x86_64       | MSVC     | Visual Studio, Ninja | Vulkan, Direct3D 12 |
| **Linux**       | x86_64       | GCC      | Ninja, Make          | Vulkan              |

Other distributions, architectures, compilers, and build generators may be supported, but have not been tested.

## Building

For package installation commands, vcpkg setup, and IDE-specific instructions, see [BUILDING.md](BUILDING.md).

### Requirements

| Requirement        | Windows x64                                                                               | Ubuntu 24.04 LTS x64                                     |
|--------------------|-------------------------------------------------------------------------------------------|----------------------------------------------------------|
| C++ compiler       | Visual Studio 2022 or newer with Desktop development with C++                             | GCC 13 / G++ 13                                          |
| Build tools        | CMake **3.30+**, Git                                                                      | CMake **3.30+**, Git                                     |
| Graphics           | Vulkan SDK and an up-to-date GPU driver for developement; <br/>Vulkan or D3D12 at runtime | Vulkan development packages and a compatible GPU driver  |
| C# scripting       | .NET **10 SDK**, including the native hosting pack                                        | .NET **10 SDK**, including the native hosting pack       |
| Dependency manager | vcpkg recommended; CMake/FetchContent fallback available                                  | vcpkg recommended; CMake/FetchContent fallback available |

The editor currently enables C# support unconditionally, so the .NET SDK is required even if your project only uses Lua. A runtime-only installation is not sufficient to build the editor.

The first configure downloads and prepares third-party dependencies and can take substantially longer than later builds. vcpkg does not replace the platform prerequisites above.

### Windows

Install the [Windows prerequisites](BUILDING.md#windows-prerequisites), then open an **x64 Visual Studio Developer PowerShell** so that `cl`, the Windows SDK, CMake, and Ninja are available.

```powershell
git clone https://github.com/kateBea/Mikoto.git
Set-Location Mikoto

cmake --preset windows-release
cmake --build --preset windows-release --parallel 4

Set-Location out/build/windows-release/Mikoto-Editor
.\MikotoEditor.exe
```

For Debug, replace `windows-release` with `windows-debug` in both commands and the output path.

**CLion:** select the Visual Studio toolchain with **x64** architecture and the **Windows release (automatic dependencies)** profile. The preset does not select CLion's toolchain for you; MinGW is not supported by the Windows build. See [Running from an IDE](BUILDING.md#running-from-an-ide).

### Ubuntu 24.04 LTS

First follow the [Ubuntu setup](BUILDING.md#ubuntu-2404-lts-prerequisites) to install the system development packages, .NET SDK, and CMake 3.30 or newer. Ubuntu 24.04's original CMake package is too old for this project.

```bash
git clone https://github.com/kateBea/Mikoto.git
cd Mikoto

cmake --preset linux-release -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
cmake --build --preset linux-release --parallel 4

cd build/linux-release/Mikoto-Editor
```

You can change graphics API by editing the existing `[renderer]` section in the copied `app-config.toml`:

```toml
[renderer]
api = "vulkan" # ogl(OpenGL), d3d11(Direc2D 11), d3d12(Direct3D 12)
```

Then run:

```bash
./MikotoEditor
```

The checked-in configuration currently selects Vulkan. Do not add a duplicate `[renderer]` section. For Debug, use `linux-debug` in place of `linux-release`.

### Choosing a dependency provider

The default `AUTO` provider uses vcpkg when it can find a valid installation; otherwise, it uses the CMake/FetchContent dependencies. Set `VCPKG_ROOT` to your vcpkg installation before configuring. Manifest dependencies are installed automatically from [vcpkg.json](vcpkg.json).

| Preset                                            | Configuration | Dependencies        |
|---------------------------------------------------|---------------|---------------------|
| `windows-release` / `linux-release`               | Release       | Automatic selection |
| `windows-debug` / `linux-debug`                   | Debug         | Automatic selection |
| `windows-source-release` / `linux-source-release` | Release       | CMake/FetchContent  |

For example, to explicitly use FetchContent on Windows:

```powershell
cmake --preset windows-source-release
cmake --build --preset windows-source-release --parallel 4
```

To require vcpkg, pass `-DMIKOTO_DEPENDENCY_PROVIDER=VCPKG` during the first configure. Automatic fallback applies only when vcpkg is unavailable; a failed package build remains an error so its cause can be fixed.

Use a fresh build directory when changing dependency provider, compiler, or triplet. The provided vcpkg triplets select Mikoto's library and runtime linkage; standard `x64-windows` packages are not interchangeable with the project's static MSVC runtime configuration.

## Configuration

Run the editor with `build/PRESET/Mikoto-Editor` as its **working directory**. The build copies `app-config.toml` and runtime resources there. Keep the staged files with the executable.

- **Rendering API:** edit `renderer.api` in `app-config.toml` to select `"vulkan"` or, on Windows, `"d3d12"`.
- **Rendering quality:** use the editor's Renderer settings for resolution, MSAA, tone mapping, and ray-tracing options.
- **Scripts:** choose Lua or C# in a Script component. C# scripts are compiled by the editor; you do not need to maintain a separate script `.csproj`.

For a persistent local configuration change, edit [Mikoto/Resources/app-config.toml](Mikoto/Resources/app-config.toml). With every build
this file gets copied and replaced to the build directory.

## Troubleshooting

Some problems that might arise during build process or when running the application. 

| Symptom                                            | What to check                                                                                                                        |
|----------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| CMake cannot find `cl` or selects MinGW            | Use an x64 Visual Studio developer shell, or select CLion's Visual Studio toolchain. Start with a fresh cache after switching compilers. |
| Configuration fails during `vcpkg install`         | Read the first failing package's log, whose path vcpkg prints. A later Ninja error may be a consequence of the earlier failure.      |
| `CMAKE_MAKE_PROGRAM` is missing                    | Ensure Ninja is available. In CLion, select Bundled Ninja in the toolchain settings.                                                 |
| .NET hosting files cannot be found                 | Install the .NET 10 SDK and native hosting pack. For a custom location, configure with `-DMIKOTO_DOTNET_ROOT=/path/to/dotnet`.       |
| Missing resources or configuration at startup      | Set the working directory to `build/<preset>/Mikoto-Editor`.                                                                         |
| Windows linker cannot overwrite `MikotoEditor.exe` | Close the running editor and retry the build.                                                                                        |
| Build runs out of memory or is resource intensive  | Lower `--parallel`; set `VCPKG_MAX_CONCURRENCY` separately to limit dependency builds.                                               |

Avoid configuring or building the same output directory simultaneously from an IDE and a terminal. More details are in [Useful options and troubleshooting](BUILDING.md#useful-options-and-troubleshooting).

## Dependencies

Third-party libraries and their roles are listed in [THIRD_PARTY.md](THIRD_PARTY.md). Dependency declarations are split between two supported build paths:

| Location                                                        | Purpose                                             |
|-----------------------------------------------------------------|-----------------------------------------------------|
| [vcpkg.json](vcpkg.json)                                        | Manifest packages, features, and pinned versions    |
| [CMake/Ports](CMake/Ports) and [CMake/Triplets](CMake/Triplets) | Package customizations and platform build settings  |
| `Mikoto/<Module>/Dependencies.cmake`                            | Package lookup, FetchContent fallback, and module linkage |

When adding a library, update both dependency paths and link it to the module that uses it. See [Dependency layout and adding a library](BUILDING.md#dependency-layout-and-adding-a-library) for a worked example.

## Repository layout

| Directory                            | Contents                                                                         |
|--------------------------------------|----------------------------------------------------------------------------------|
| [Mikoto](Mikoto)                     | Engine modules: rendering, scenes, assets, scripting, physics, and other systems |
| [Mikoto-Editor](Mikoto-Editor)       | Editor application and panels                                                    |
| [Mikoto/Resources](Mikoto/Resources) | Configuration and runtime resources copied into the editor build                 |
| [Mikoto/Vendor](Mikoto/Vendor)       | Vendored and locally maintained third-party code                                 |
| [CMake](CMake)                       | Dependency provider, package integration, ports, and triplets                    |
| [Resources](Resources)               | Screenshots and additional repository assets                                     |

Applications declare their engine module requirements through `mikoto_use_modules()`. See [the module definitions](Mikoto/Modules.cmake) and [the editor's CMake configuration](Mikoto-Editor/CMakeLists.txt) for examples. Engine-only builds can disable the editor with `MIKOTO_BUILD_EDITOR=OFF` and request modules through `MIKOTO_MODULES`.

## Future Development

Mikoto Engine is still in its early stages, and additional features and optimizations will be added over time.

## Acknowledgments

The development of Mikoto has been inspired by the work of the following:

* Joey de Vries  for the incredible [LearnOpenGL](https://learnopengl.com/) tutorials.
* Yan Chernikov for his insightful [YouTube videos](https://www.youtube.com/@TheCherno).
* Cem Yuksel for his educational [graphics programming lectures](https://www.youtube.com/@cem_yuksel/videos).
* Jason Gregory for his [Game Engine Architecture](https://www.gameenginebook.com/) book.
* Matt Pharr, Wenzel Jakob, and Greg Humphreys for the [Physically Based Rendering](https://www.pbr-book.org/) book.
* Sascha Willems for his amazing [Vulkan examples](https://github.com/SaschaWillems/Vulkan).
* Marco Castorina and Gabriel Sassone for the **Mastering Graphics Programming with Vulkan** book.

Some demonstration models come from [Morgan McGuire's Computer Graphics Archive](https://casual-effects.com/data/). Thanks also to the authors and maintainers of the engine's third-party libraries; their licenses and asset terms remain applicable.

## License

Mikoto is licensed under the [Apache License 2.0](LICENSE).
