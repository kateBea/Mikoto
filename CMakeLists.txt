#    Copyright 2026 ケイト
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.30)

# Resolve profiling before vcpkg selects the manifest features in project().
option(MIKOTO_BUILD_EDITOR "Build the Mikoto editor" ON)
option(MIKOTO_ENABLE_TRACY_PROFILING "Enable Tracy profiling" ${MIKOTO_BUILD_EDITOR})

include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/Slang.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/DependencyProvider.cmake")
project(MikotoRoot VERSION 1.0)

if(WIN32 AND NOT MSVC)
    message(FATAL_ERROR
        "Mikoto's Windows build requires an MSVC-compatible toolchain. "
        "Selected compiler: ${CMAKE_CXX_COMPILER}. "
        "If you are in CLion, select the Visual Studio toolchain with x64 architecture, "
        "then reset the CMake cache and reload. Do not reuse a MinGW build cache.")
endif()

# C++ language requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Mikoto uses headers, not C++20 named modules. Avoid unnecessary module scans
# and Ninja's stale-dyndep scheduler assertion on incremental builds (See issue #2592).
# Ordinary header dependency tracking remains enabled.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Platform-specific settings
if(MSVC)
    message("Target platform Windows")

    # Compile multithreaded
    add_compile_options(/MP)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>"
            CACHE STRING "" FORCE)
elseif(LINUX)
    message("Target platform Linux")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g3 -Wall)
        if(NOT MIKOTO_USE_VCPKG)
            add_compile_options(-D_GLIBCXX_DEBUG)
        endif()
    else()
        add_compile_options(-O3 -Wall)
    endif()
endif()

# Add engine and applications
option(MIKOTO_BUILD_RUNTIME "Build the standalone game player" ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/Mikoto/Modules.cmake")

# Add editor
if(MIKOTO_BUILD_EDITOR)
    add_subdirectory(Mikoto-Editor)
endif()

# Add standalone runtime
if(MIKOTO_BUILD_RUNTIME)
    add_subdirectory(Mikoto-Runtime)
endif()

add_subdirectory(Mikoto)

# Keep IDE targets flat, including targets configured by dependencies.
set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
