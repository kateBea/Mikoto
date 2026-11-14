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

# Include before the first project(): vcpkg installs the manifest from its toolchain.
set(MIKOTO_DEPENDENCY_PROVIDER AUTO CACHE STRING "Dependencies: AUTO, VCPKG or FETCHCONTENT")
set_property(CACHE MIKOTO_DEPENDENCY_PROVIDER PROPERTY STRINGS AUTO VCPKG FETCHCONTENT)
string(TOUPPER "${MIKOTO_DEPENDENCY_PROVIDER}" MIKOTO_DEPENDENCY_PROVIDER)
if(NOT MIKOTO_DEPENDENCY_PROVIDER MATCHES "^(AUTO|VCPKG|FETCHCONTENT)$")
    message(FATAL_ERROR "MIKOTO_DEPENDENCY_PROVIDER must be AUTO, VCPKG or FETCHCONTENT")
endif()

# CMake normally reads this environment variable inside project(), too late
# for provider selection. Respect it before searching for an installation.
if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{CMAKE_TOOLCHAIN_FILE})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{CMAKE_TOOLCHAIN_FILE}" CACHE FILEPATH "User toolchain")
endif()
set(mikoto_vcpkg_toolchain "")
if(CMAKE_TOOLCHAIN_FILE MATCHES "(^|[/\\\\])vcpkg\\.cmake$")
    set(mikoto_vcpkg_toolchain "${CMAKE_TOOLCHAIN_FILE}")
elseif(NOT CMAKE_TOOLCHAIN_FILE)
    if(VCPKG_ROOT)
        set(mikoto_vcpkg_root "${VCPKG_ROOT}")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(mikoto_vcpkg_root "$ENV{VCPKG_ROOT}")
    else()
        find_program(mikoto_vcpkg_program NAMES vcpkg NO_CACHE)
        if(mikoto_vcpkg_program)
            get_filename_component(mikoto_vcpkg_root "${mikoto_vcpkg_program}" DIRECTORY)
        endif()
    endif()
    if(EXISTS "${mikoto_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        set(mikoto_vcpkg_toolchain "${mikoto_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
    endif()
endif()

set(MIKOTO_USE_VCPKG OFF)
if(MIKOTO_DEPENDENCY_PROVIDER STREQUAL "FETCHCONTENT")
    if(mikoto_vcpkg_toolchain AND CMAKE_TOOLCHAIN_FILE)
        message(FATAL_ERROR "FETCHCONTENT cannot use a vcpkg toolchain. Use a fresh build directory without CMAKE_TOOLCHAIN_FILE.")
    endif()
elseif(mikoto_vcpkg_toolchain AND EXISTS "${mikoto_vcpkg_toolchain}")
    set(MIKOTO_USE_VCPKG ON)
elseif(MIKOTO_DEPENDENCY_PROVIDER STREQUAL "VCPKG")
    message(FATAL_ERROR "vcpkg is unavailable. Set VCPKG_ROOT or CMAKE_TOOLCHAIN_FILE, or select AUTO/FETCHCONTENT.")
endif()

# Pre-manifest build trees have no provider marker yet. Do not retrofit a
# toolchain into a configured FetchContent tree when vcpkg is first installed.
if(MIKOTO_USE_VCPKG AND NOT DEFINED MIKOTO_CONFIGURED_WITH_VCPKG
    AND EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt" AND NOT DEFINED VCPKG_INSTALLED_DIR)
    message(FATAL_ERROR "This existing build tree predates vcpkg integration. Use a fresh build directory, or select FETCHCONTENT for this one.")
endif()
# Do not mix packages and source-built libraries in an existing CMake cache.
if(DEFINED MIKOTO_CONFIGURED_WITH_VCPKG AND NOT "${MIKOTO_CONFIGURED_WITH_VCPKG}" STREQUAL "${MIKOTO_USE_VCPKG}")
    message(FATAL_ERROR "The dependency provider changed. Select a fresh build directory to avoid mixed dependency/CRT settings.")
endif()
set(MIKOTO_CONFIGURED_WITH_VCPKG "${MIKOTO_USE_VCPKG}" CACHE INTERNAL "Dependency provider for this build")

if(MIKOTO_USE_VCPKG)
    set(CMAKE_TOOLCHAIN_FILE "${mikoto_vcpkg_toolchain}" CACHE FILEPATH "vcpkg toolchain")
    include("${CMAKE_CURRENT_LIST_DIR}/Vcpkg.cmake")
    message(STATUS "Mikoto dependencies: vcpkg manifest (${VCPKG_TARGET_TRIPLET})")
else()
    message(STATUS "Mikoto dependencies: CMake/FetchContent (vcpkg unavailable or explicitly disabled)")
endif()
