cmake_minimum_required(VERSION 3.22)

project(MikotoApps)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===========================
# Add engine and editor
# ===========================
add_subdirectory(HelloWorld)
add_subdirectory(WindowLinux)
add_subdirectory(WindowWin32)
add_subdirectory(Sockets)
add_subdirectory(HelloTriangleWin32_D3D11)
add_subdirectory(HelloTriangleWin32_D3D12)