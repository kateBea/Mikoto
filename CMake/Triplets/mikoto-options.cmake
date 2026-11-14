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

# Shader compiler code is optimized even when the editor uses Debug. Keep the
# debug CRT and symbols; never link a release static CRT into a debug executable.
# Building compiler in Debug makes it very slow.
if(PORT MATCHES "^(shader-slang|shaderc|glslang|spirv-tools)$")
    if(VCPKG_CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(mikoto_debug_flags "-g -O2")
    else()
        # Embed compiler debug info in the static archives. These ports do not
        # consistently install the separate compiler PDBs produced by /Zi.
        set(mikoto_debug_flags "/MTd /D_DEBUG /Z7 /O2 /Ob2")
    endif()
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS_DEBUG
        "-DCMAKE_C_FLAGS_DEBUG=${mikoto_debug_flags}"
        "-DCMAKE_CXX_FLAGS_DEBUG=${mikoto_debug_flags}")
endif()

# ImPlot includes ImGui's font types in its ABI. Keep it in lockstep with the
# wchar32 ImGui feature selected by the engine manifest.
if(PORT STREQUAL "implot")
    if(VCPKG_CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(mikoto_imgui_wchar32_flag "-DIMGUI_USE_WCHAR32")
    else()
        set(mikoto_imgui_wchar32_flag "/DIMGUI_USE_WCHAR32")
    endif()
    # vcpkg's CMake wrapper requires C and C++ flag variables to be defined
    # together, even though ImPlot only needs this definition in C++ sources.
    set(VCPKG_C_FLAGS "${VCPKG_C_FLAGS} ${mikoto_imgui_wchar32_flag}")
    set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} ${mikoto_imgui_wchar32_flag}")
endif()

if(PORT STREQUAL "joltphysics" AND NOT VCPKG_CMAKE_SYSTEM_NAME)
    # vcpkg requires C and C++ flag variables to be defined together, even
    # though these Jolt compatibility switches apply only to C++ compilation.
    if(NOT DEFINED VCPKG_C_FLAGS)
        set(VCPKG_C_FLAGS "")
    endif()
    set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} /wd4865 /WX- /Zc:enumTypes-")
endif()
