# Building Mikoto Engine

The Mikoto Engine build process is currently verified on both **Ubuntu 24.04 LTS** and **Windows**.

## Prerequisites

### Windows

The following are required:

* [Visual Studio](https://visualstudio.microsoft.com/)
* [Vulkan SDK](https://vulkan.lunarg.com/)

Precompiled Lua binaries are included with the repository for MSVC builds.

### Linux

Mikoto requires **Lua 5.1 or newer** for scripting. Lua can be installed manually before building the engine.

## Installing Lua on Linux

Download Lua from the [official Lua downloads page](https://www.lua.org/download.html) and obtain the `.tar.gz` archive.

Extract and install Lua:

```bash
tar -xvf lua-5.4.8.tar.gz
cd lua-5.4.8

make
make test
sudo make install
```

Verify the installation:

```bash
lua -v
```

You should see output similar to:

```text
Lua 5.4.8  Copyright (C) 1994-2025 Lua.org, PUC-Rio
```

> **Note:** Precompiled Lua binaries are shipped with Mikoto for MSVC builds on Windows.

## Installing Linux Dependencies

Mikoto provides a CMake target for installing the required Linux dependencies automatically.

From the build directory, run:

```bash
cmake --build . --target InstallDependencies --config Release
```

You may be prompted for sudo privileges during installation.

### Required Libraries

The following packages are required on Ubuntu 24.04:

```bash
# Vulkan
sudo apt install vulkan-tools
sudo apt install libvulkan-dev vulkan-validationlayers
sudo apt install vulkan-utility-libraries-dev spirv-tools

# Native file dialog
sudo apt install libgtk-3-dev

# GLFW / Wayland / X11
sudo apt install libwayland-dev libxkbcommon-dev xorg-dev
```

These commands are also reflected in Mikoto's Linux installation [script](/Mikoto/Resources/installs.sh).

## Building the Project

Clone the repository (currently there are no submodules all dependencies are pulled from CMake):

```bash
git clone https://github.com/kateBea/Mikoto.git
cd Mikoto
```

Create a build directory:

```bash
mkdir build
cd build
```

Generate the platform-specific build files:

```bash
cmake -S .. -B .
```

Build Mikoto in Release configuration:

```bash
cmake --build . --config Release
```

## Development Environments

### Visual Studio

On Windows, CMake generates Visual Studio solution files by default.

Open the generated `.sln` file in Visual Studio and build the project from there.

### CLion

The project can be opened directly in CLion. CMake will handle project configuration and generation automatically.

## Linux Build Example

A complete Linux build can be performed with:

```bash
git clone --recursive https://github.com/kateBea/Mikoto.git
cd Mikoto

mkdir build
cd build

cmake -S .. -B .

cmake --build . --target InstallDependencies --config Release
cmake --build . --config Release
```
