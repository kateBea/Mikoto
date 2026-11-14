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

include("${CMAKE_CURRENT_SOURCE_DIR}/../CMake/Slang.cmake")
project(Mikoto VERSION 1.0 LANGUAGES C CXX)

set(MIKOTO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

option(MIKOTO_USE_XCB_WINDOW "Use XCB for Linux windowing" OFF)
option(MIKOTO_USE_WIN32_WINDOW "Use Win32 for Windows windowing" OFF)

set(MIKOTO_TRACY_CALLSTACK_DEPTH 0 CACHE STRING "Tracy zone callstack depth (0 disables unwinding)")

if(NOT MIKOTO_TRACY_CALLSTACK_DEPTH MATCHES "^(0|[1-9][0-9]*)$" OR MIKOTO_TRACY_CALLSTACK_DEPTH GREATER 62)
    message(FATAL_ERROR "MIKOTO_TRACY_CALLSTACK_DEPTH must be between 0 and 62")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/Modules.cmake")

mikoto_resolve_modules()

add_library(Mikoto STATIC)
add_library(Mikoto::Mikoto ALIAS Mikoto)

if(MIKOTO_USE_VCPKG)
    set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
endif()
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Configure dependencies in a stable order. These include files run in this
# directory so FetchContent paths are also available to each module below.
set(MIKOTO_DEPENDENCY_ORDER
        Core
        Math
        Threading
        Logging
        Filesystem
        Platform
        Scene
        Audio
        Text
        Physics
        Assets
        Animation
        Renderer
        ImGui
        Scripting
        Networking
        Video)

foreach(module IN LISTS MIKOTO_DEPENDENCY_ORDER)
    string(TOUPPER "${module}" module_upper)

    if(MIKOTO_MODULE_${module_upper})
        include("${CMAKE_CURRENT_SOURCE_DIR}/${module}/Dependencies.cmake")
    endif()
endforeach()

target_compile_features(Mikoto PUBLIC cxx_std_20)

target_include_directories(Mikoto PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(Mikoto PUBLIC
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    GLM_ENABLE_EXPERIMENTAL)

if(MIKOTO_ENABLE_TRACY_PROFILING)
    target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_TRACY
        MIKOTO_TRACY_CALLSTACK_DEPTH=${MIKOTO_TRACY_CALLSTACK_DEPTH})
    target_link_libraries(Mikoto PUBLIC Tracy::TracyClient)
endif()

foreach(module IN LISTS MIKOTO_FOUNDATION_MODULES MIKOTO_FEATURE_MODULES)
    string(TOUPPER "${module}" module_upper)

    if(MIKOTO_MODULE_${module_upper})
        target_compile_definitions(Mikoto PUBLIC MIKOTO_MODULE_${module_upper}=1)
        add_subdirectory("${module}")
        message(STATUS "Mikoto module: ${module}")
    else()
        target_compile_definitions(Mikoto PUBLIC MIKOTO_MODULE_${module_upper}=0)
    endif()
endforeach()

