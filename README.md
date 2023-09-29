# Mikoto Engine
This is a simple engine written in modern C++. For now and quite some time 
it will use OpenGL as its main rendering API, but the end goal is to integrate Vulkan.

![Mikoto Engine](assets/image/img9.png)

# Requirements
<h3>Software Requirements:</h3>
<ol type="1">
  <li>CMake 3.21 (This is what the project uses, but an earlier version is most likely to work)</li>
  <li>A compiler capable of C++20 (G++ 12.3.0 was used for the tests)</li>
  <li>The Vulkan SDK</li>
  <li>OpenGL library</li>
</ol>

# Building

Clone the repository, at the moment I only tested building on Linux. 
The project already comes with a CMake file ready to use. 

```shell
  # Clone the repository to your desired directory
  git clone --recursive https://github.com/kateBea/Mikoto-Engine.git
  
  # Change directory to the repo folder
  cd Mikoto-Engine
  
  # Make a build directory (preferable)
  mkdir build 
  
  # and open the build directory
  cd build
  
  # Run CMake on the CMake file from the repo root directory
  cmake ..
  
  # Finally build the project
  cmake --build .
  
  # and run the executable
  ./Mikoto
```

This project is done thanks to various third-party libraries:

1. [FMT (Modern formatting library)](https://github.com/fmtlib/fmt)
2. [GLEW (Open GL extension Wrangler)](https://glew.sourceforge.net/)
3. [GLFW (Multiplatform Library for Window, Event handling, etc.)](https://github.com/glfw/glfw)
4. [GLM (Open GL Mathematics Library for C++)](https://github.com/g-truc/glm)
5. [ImGui (Graphical User interface Library for C++)](https://github.com/ocornut/imgui)
6. [Spdlog (Fast C++ Logging Library)](https://github.com/gabime/spdlog)

The GLFW library is not necessary to be installed on the system since it 
is included as a submodule and build along with the project.

# Goals

I spend my spare time on improving this project, which is mainly a way for me to learn software design and mostly 3D graphics programming.
The main goal is really just offer support for 3D and 2D graphics rendering. At the moment, many configurations that are necessary for
the engine are hardcoded in the translation units, the idea is to be able to load these configurations from regular files.

# Special thanks and mentions to
  - Yan Chernikov for his [YouTube videos](https://www.youtube.com/@TheCherno) and live streams
  - Cem Yuksel for his [YouTube videos](https://www.youtube.com/@cem_yuksel/videos) about graphics programing
  - Jason Gregory for the [Game Engine Architecture](https://www.gameenginebook.com/) book
  - Matt Pharr, Wenzel Jakob, Greg Humphreys for the [Physically Based Rendering: From Theory to Implementation](https://www.pbr-book.org/)
  - Sascha Willems for the [Vulkan examples](https://github.com/SaschaWillems/Vulkan)
