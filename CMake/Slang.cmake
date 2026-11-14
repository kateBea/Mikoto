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

include_guard(GLOBAL)

# Read before project(): the vcpkg manifest must also skip Slang in SDK mode.
option(MIKOTO_USE_PRECOMPILED_SLANG "Use a local Slang SDK instead of building Slang" OFF)
set(MIKOTO_SLANG_ROOT "" CACHE PATH "Slang SDK directory (empty uses Mikoto/Vendor/Slang's platform default)")

function(mikoto_import_precompiled_slang)
    set(slang_architecture "${CMAKE_SYSTEM_PROCESSOR}")
    if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
        set(slang_architecture "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
    endif()
    string(TOLOWER "${slang_architecture}" slang_architecture)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT slang_architecture MATCHES "^(amd64|x86_64|x64)$")
        message(FATAL_ERROR "Precompiled Slang currently requires an x86_64 target; selected architecture: ${slang_architecture}")
    endif()

    if(WIN32 AND MSVC)
        set(slang_sdk_name "slang-2026.17-windows-x86_64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(slang_sdk_name "slang-2026.17-linux-x86_64-glibc-2.28")
    else()
        message(FATAL_ERROR "Precompiled Slang is supported on Windows with MSVC and on Linux. Disable MIKOTO_USE_PRECOMPILED_SLANG to build from source.")
    endif()

    if(MIKOTO_SLANG_ROOT)
        get_filename_component(slang_root "${MIKOTO_SLANG_ROOT}" ABSOLUTE BASE_DIR "${MIKOTO_SOURCE_DIR}")
    else()
        set(slang_root "${MIKOTO_SOURCE_DIR}/Vendor/Slang/${slang_sdk_name}")
    endif()

    set(slang_required_files "${slang_root}/include/slang.h")
    if(WIN32)
        set(slang_library "${slang_root}/lib/slang.lib")
        set(slang_compiler_library "${slang_root}/lib/slang-compiler.lib")
        set(slang_runtime "${slang_root}/bin/slang.dll")
        set(slang_compiler_runtime "${slang_root}/bin/slang-compiler.dll")
        list(APPEND slang_required_files "${slang_runtime}" "${slang_compiler_runtime}")
        file(GLOB slang_runtime_files CONFIGURE_DEPENDS
            "${slang_root}/bin/*.dll" "${slang_root}/lib/*.dll")
    else()
        set(slang_library "${slang_root}/lib/libslang.so")
        set(slang_compiler_library "${slang_root}/lib/libslang-compiler.so")
        # Include versioned names as well, so the ELF loader can resolve SONAMEs.
        file(GLOB slang_runtime_files CONFIGURE_DEPENDS
            "${slang_root}/lib/*.so*" "${slang_root}/bin/*.so*")
    endif()
    list(APPEND slang_required_files "${slang_library}" "${slang_compiler_library}")
    foreach(slang_file IN LISTS slang_required_files)
        if(NOT EXISTS "${slang_file}")
            message(FATAL_ERROR
                "Precompiled Slang file is missing: ${slang_file}. "
                "Restore the SDK to ${slang_root}, set MIKOTO_SLANG_ROOT to its location, "
                "or disable MIKOTO_USE_PRECOMPILED_SLANG.")
        endif()
    endforeach()

    add_library(slang SHARED IMPORTED GLOBAL)
    add_library(mikoto-slang-compiler SHARED IMPORTED GLOBAL)
    if(WIN32)
        # The .lib files are DLL import libraries, not static Slang builds.
        set_target_properties(slang PROPERTIES
            IMPORTED_IMPLIB "${slang_library}" IMPORTED_LOCATION "${slang_runtime}")
        set_target_properties(mikoto-slang-compiler PROPERTIES
            IMPORTED_IMPLIB "${slang_compiler_library}" IMPORTED_LOCATION "${slang_compiler_runtime}")
    else()
        set_target_properties(slang PROPERTIES IMPORTED_LOCATION "${slang_library}")
        set_target_properties(mikoto-slang-compiler PROPERTIES IMPORTED_LOCATION "${slang_compiler_library}")
    endif()
    set_target_properties(slang PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${slang_root}/include"
        INTERFACE_LINK_LIBRARIES mikoto-slang-compiler
        MIKOTO_RUNTIME_FILES "${slang_runtime_files}")

    message(STATUS "Slang: precompiled x86_64 SDK at ${slang_root}")
endfunction()
