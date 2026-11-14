# Building Mikoto Engine

Mikoto uses C++20 and CMake. The instructions below cover Windows x64 with MSVC
and Ubuntu 24.04 x64 with GCC.

## Prerequisites

### Windows

Install:

- [Visual Studio](https://visualstudio.microsoft.com/) with **Desktop development with C++** and the Windows SDK.
- **CMake 3.30 or newer**, **Ninja**, **Git**, and **Python 3**.
- [Vulkan SDK](https://vulkan.lunarg.com/) and a current GPU driver.
- [.NET 10 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/10.0) for C# scripting. The runtime alone is not enough.

Run the build commands from an **x64 Native Tools Command Prompt** or an
x64 Developer PowerShell so that MSVC and the Windows SDK are available.
Windows builds require an MSVC-compatible toolchain; MinGW is not supported.

### Linux

Install the development packages before configuring CMake:

```bash
sudo apt update
sudo apt install -y \
  build-essential gcc-13 g++-13 git ninja-build python3 pkg-config \
  curl wget ca-certificates zip unzip tar autoconf automake libtool \
  bison flex gperf patch \
  xorg-dev libwayland-dev wayland-protocols libxkbcommon-dev \
  libxkbcommon-x11-dev libgtk-3-dev libdbus-1-dev \
  libasound2-dev libpulse-dev libudev-dev libdrm-dev libgbm-dev \
  libgl1-mesa-dev libegl1-mesa-dev \
  libvulkan-dev vulkan-tools vulkan-validationlayers \
  libssl-dev libpng-dev libprotobuf-dev protobuf-compiler
```

Install the **.NET 10 SDK** using the
[Ubuntu installation instructions](https://learn.microsoft.com/en-us/dotnet/core/install/linux-ubuntu-install).
Check that `dotnet --list-sdks` lists a 10.x SDK.

Lua 5.5.1 is supplied by the build in both dependency modes. A separate Lua
installation is not required.

## Dependencies

CMake downloads and configures dependencies automatically. The first configuration
can take a while.

By default, Mikoto uses **vcpkg** when it is available, otherwise **FetchContent**.
Set `VCPKG_ROOT` to an existing vcpkg installation to use it. You do not need to
run `vcpkg install` separately.

To install vcpkg on Windows:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
& C:\dev\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = 'C:\dev\vcpkg'
```

On Linux:

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg"
```

These variables apply to the current shell. Set `VCPKG_ROOT` in your environment
permanently if you want your IDE and future shells to use it.

Keep the repository's custom triplets: `x64-windows-mikoto` and
`x64-linux-mikoto`.

Library versions are pinned to stable releases. Source builds use Slang
**2026.17.1**, including its matching optimizer plugin. Updating these pins can
rebuild affected packages; subsequent builds reuse vcpkg's binary cache.

### Precompiled Slang (optional)

Slang is built from source by default in both dependency modes. To use a local
precompiled SDK instead, configure with `-DMIKOTO_USE_PRECOMPILED_SLANG=ON`.
CMake then skips Slang's source dependency and uses these default SDK directories:

- Windows x64/MSVC: `Mikoto/Vendor/Slang/slang-2026.17-windows-x86_64`
- Linux x64: `Mikoto/Vendor/Slang/slang-2026.17-linux-x86_64-glibc-2.28`

Restore the extracted SDK there, or override its directory with
`-DMIKOTO_SLANG_ROOT="C:/path/to/slang-sdk"`. The SDK must include its `include`,
`lib`, and runtime libraries. Its shared libraries and matching plugins are copied
beside the editor and standalone player. These SDK files are not downloaded by
CMake when using this option.

## Building the Project

Clone the repository:

```bash
git clone https://github.com/kateBea/Mikoto.git
cd Mikoto
```

### Windows

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel 4
cd build/windows-release/Mikoto-Editor
.\MikotoEditor.exe
```

### Linux

```bash
CC=gcc-13 CXX=g++-13 cmake --preset linux-release
cmake --build --preset linux-release --parallel 4
cd build/linux-release/Mikoto-Editor
./MikotoEditor
```

Run from the executable's directory so the editor can find its resources.
Use `windows-debug` or `linux-debug` in both commands for a Debug build.
`windows-relwithdebinfo` provides optimization and debug symbols for profiling.

To explicitly use FetchContent, choose `windows-source-release` or
`linux-source-release` instead. Linux source builds also use the system PNG,
OpenSSL, and Protobuf packages installed above.

Use a new build directory when changing dependency providers, compilers, or
triplets. The source presets already use separate directories.

## Build Options

Windowing and input use SDL3 for both the editor and standalone runtime.

Pass options when configuring, for example:

```bash
cmake --preset windows-release -DMIKOTO_ENABLE_DOTNET_SCRIPTING=OFF
```

| Option | Purpose |
| --- | --- |
| `MIKOTO_BUILD_EDITOR=OFF` | Skip the editor. |
| `MIKOTO_BUILD_RUNTIME=OFF` | Skip the standalone player. |
| `MIKOTO_ENABLE_DOTNET_SCRIPTING=OFF` | Build without C# hosting or the .NET SDK requirement. |
| `MIKOTO_ENABLE_TRACY_PROFILING=ON` | Enable Tracy profiling. |
| `MIKOTO_EDITOR_TRACY_VIEWER=ON` | Add the Instrumentation panel for saved `.tracy` captures. |
| `MIKOTO_DEPENDENCY_PROVIDER=VCPKG` | Require vcpkg instead of automatic detection. |
| `MIKOTO_USE_PRECOMPILED_SLANG=ON` | Use a local Slang SDK instead of building Slang (default: OFF). |
| `MIKOTO_SLANG_ROOT=/path/to/slang-sdk` | Override the precompiled Slang SDK directory. |
| `MIKOTO_MODULES=Networking` | Request additional engine modules, including for application-free builds. |

Each engine module keeps its dependency lookup, source download, and linkage in
`Mikoto/<module>/Dependencies.cmake`. The module's `CMakeLists.txt` lists its own
sources. Shared application resource and runtime copying lives in
`CMake/Deployment.cmake`.

## Development Environments

### Visual Studio

Open the repository folder and select a Windows CMake preset. Build
`MikotoEditor` and use its output directory as the debugging working directory.

### CLion

Open the repository and select an x64 Visual Studio toolchain on Windows, or GCC
on Linux. Use the matching CMake preset and set the run working directory to the
editor's output directory.

## Standalone Player

The `MikotoRuntime` executable is built in the corresponding `Mikoto-Runtime`
output directory. Run it from there:

```powershell
.\MikotoRuntime.exe --project C:\path\to\Game.mktprj --api vulkan
```

On Linux, use `./MikotoRuntime`. The startup scene must have an active Main Camera.

To package a game into a new directory:

```powershell
.\MikotoRuntime.exe --project C:\path\to\Game.mktprj --package C:\path\to\ShipGame
```

The package contains a `game.yaml` manifest. Launch the player beside that
manifest to load the game. Keep game assets inside the project before packaging.

## C# Scripts

The native build produces `Mikoto.ScriptCore.dll` and stages it under
`Resources/Scripting`. Game scripts are built separately by the editor.

Opening a game project creates `Mikoto.Scripts.slnx` beside the project and
`Assets/Mikoto.Scripts.csproj` if they do not exist. Scripts under `Assets`
compile together through `dotnet build` (MSBuild). Loose scripts must belong to a
`.csproj`; there is no separate in-process compiler. Choose Visual Studio or VS
Code in **Preferences > External tools**, then select **Open script project**.

Keep the solution, project, and script files in version control. Exclude
`Mikoto.ScriptCore.generated.props`, `bin/`, and `obj/`: the generated properties
contain the local engine API path. Compilation errors appear in Compiler Status;
restart Play to use rebuilt scripts.
