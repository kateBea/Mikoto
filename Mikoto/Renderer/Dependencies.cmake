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

if(WIN32 AND MIKOTO_ENABLE_TRACY_PROFILING)
    if(MIKOTO_USE_VCPKG)
        get_target_property(tracy_public_dir Tracy::TracyClient INTERFACE_INCLUDE_DIRECTORIES)
    else()
        FetchContent_GetProperties(tracy SOURCE_DIR tracy_source_dir)
        set(tracy_public_dir "${tracy_source_dir}/public")
    endif()

    # Tracy v0.14.1 drains D3D12 queries in its destructor, even after reconnecting.
    # An old context's timestamps have no corresponding zones in the new trace.
    # https://github.com/wolfpld/tracy/blob/v0.14.1/public/tracy/TracyD3D12.hpp
    find_file(tracy_d3d12_header TracyD3D12.hpp PATHS ${tracy_public_dir}
        PATH_SUFFIXES tracy NO_DEFAULT_PATH NO_CACHE REQUIRED)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${tracy_d3d12_header}")
    file(READ "${tracy_d3d12_header}" tracy_d3d12_source)
    string(REPLACE "#include \"Tracy.hpp\"" "#include <tracy/Tracy.hpp>" tracy_d3d12_source "${tracy_d3d12_source}")
    string(REPLACE "#include \"../client/TracyFastVector.hpp\"" "#include <client/TracyFastVector.hpp>"
        tracy_d3d12_source "${tracy_d3d12_source}")
    string(REPLACE "#include \"../common/TracyAssert.hpp\"" "#include <common/TracyAssert.hpp>"
        tracy_d3d12_source "${tracy_d3d12_source}")
    string(REPLACE "int32_t m_contextId = InvalidGpuContextId;" [=[int32_t m_contextId = InvalidGpuContextId;
#ifdef TRACY_ON_DEMAND
        const uint64_t m_captureConnection = GetProfiler().ConnectionId();
#endif]=] tracy_d3d12_source "${tracy_d3d12_source}")
    string(REPLACE "            Drain(lastIssuedTicket, 200);" [=[#ifdef TRACY_ON_DEMAND
            if (GetProfiler().IsConnected() && GetProfiler().ConnectionId() == m_captureConnection)
#endif
                Drain(lastIssuedTicket, 200);]=] tracy_d3d12_source "${tracy_d3d12_source}")
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tracy/TracyD3D12Capture.hpp"
        CONTENT "${tracy_d3d12_source}" @ONLY)
    target_include_directories(Mikoto PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/tracy")
endif()

if(MIKOTO_USE_VCPKG)
    find_package(volk CONFIG REQUIRED)
    find_package(spirv_cross_core CONFIG REQUIRED)
    find_package(spirv_cross_glsl CONFIG REQUIRED)
    find_package(unofficial-shaderc CONFIG REQUIRED)
    find_package(unofficial-spirv-reflect CONFIG REQUIRED)
    find_package(VulkanHeaders CONFIG REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    if(NOT MIKOTO_USE_PRECOMPILED_SLANG)
        find_package(slang CONFIG REQUIRED)
        find_package(mikoto-slang-glslang CONFIG REQUIRED)

        # Preserve the application's compiler and optimizer-plugin target names.
        add_library(slang ALIAS slang::slang)
        add_library(mikoto-slang-glslang ALIAS mikoto::slang-glslang)
    endif()

    target_link_libraries(Mikoto PUBLIC volk::volk
        spirv-cross-core spirv-cross-glsl unofficial::shaderc::shaderc
        unofficial::spirv-reflect Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
    target_compile_definitions(Mikoto PUBLIC MKT_USE_PACKAGED_SPIRV_REFLECT)

    if(WIN32)
        find_package(directx-headers CONFIG REQUIRED)
        find_package(D3D12MemoryAllocator CONFIG REQUIRED)
        target_link_libraries(Mikoto PUBLIC GPUOpen::D3D12MemoryAllocator Microsoft::DirectX-Headers)
        target_compile_definitions(Mikoto PUBLIC VK_USE_PLATFORM_WIN32_KHR)
    endif()

else()
    option(MIKOTO_OPTIMIZE_SHADER_COMPILERS_IN_DEBUG
        "Optimize native Slang and shaderc compiler dependencies in Debug builds" ON)

    # Called inside dependency-fetch functions so these flags are inherited only
    # by their subdirectories, never by Mikoto or the editor. Preserve debug symbols,
    # assertions and the selected CRT/iterator ABI; only change code optimization.
    function(mikoto_shader_compiler_debug_flags)
        if(NOT MIKOTO_OPTIMIZE_SHADER_COMPILERS_IN_DEBUG)
            return()
        endif()
        foreach(language IN ITEMS C CXX)
            set(flags "${CMAKE_${language}_FLAGS_DEBUG}")
            if(MSVC)
                string(REGEX REPLACE "(^|[ \t])[-/]O[^ \t]*" " " flags "${flags}")
                # MSVC runtime instrumentation cannot be combined with /O2.
                string(REGEX REPLACE "(^|[ \t])[-/]RTC[^ \t]*" " " flags "${flags}")
                string(REPLACE "/ZI" "/Zi" flags "${flags}")
                string(APPEND flags " /O2 /Ob2")
            elseif(CMAKE_${language}_COMPILER_ID MATCHES "GNU|Clang")
                string(REGEX REPLACE "(^|[ \t])-O[^ \t]*" " " flags "${flags}")
                string(APPEND flags " -O2")
            endif()
            set(CMAKE_${language}_FLAGS_DEBUG "${flags}" PARENT_SCOPE)
        endforeach()
    endfunction()

    if(WIN32)
        # Direct3D 12 headers (According to their cmake comments this is technically
        # not needed because they are set if its cmake is top level)
        set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "")
        set(DXHEADERS_INSTALL OFF CACHE BOOL "")
        set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "")

        FetchContent_Declare(
            DirectX-Headers
            GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
            GIT_TAG        v1.619.5
        )

        FetchContent_MakeAvailable(DirectX-Headers)
    endif()

    # SPIRV-Reflect
    set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build SPIRV-Reflect as a static library")
    set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "Disable SPIRV-Reflect tests")
    set(SPIRV_REFLECT_ENABLE_ASSERTS OFF CACHE BOOL "Disable asserts for SPIRV-Reflect")
    set(SPIRV_REFLECT_ENABLE_ASAN OFF CACHE BOOL "Disable address sanitizer")
    set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "Disable SPIRV-Reflect install targets")
    set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Build spirv-reflect executable")
    FetchContent_Declare(spirv_reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG vulkan-sdk-1.4.363.0
    )
    FetchContent_MakeAvailable(spirv_reflect)

    # SPIRV-Cross
    set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        spirv_cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG        vulkan-sdk-1.4.363.0
    )
    FetchContent_MakeAvailable(spirv_cross)

    # shaderc
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL "" FORCE)
    set(SHADERC_ENABLE_WERROR_COMPILE OFF CACHE BOOL "" FORCE)
    # Mikoto uses the static MSVC CRT (/MT and /MTd); shaderc must use the same
    # runtime or the linker rejects the mixed C/C++ runtime libraries.
    set(SHADERC_ENABLE_SHARED_CRT OFF CACHE BOOL "" FORCE)

    # shaderc does not vendor glslang and SPIRV-Tools as Git submodules. FetchContent
    # runs PATCH_COMMAND after cloning shaderc but before adding it to this build; the
    # command uses shaderc's DEPS manifest to fetch those required source dependencies.
    FetchContent_Declare(shaderc
        GIT_REPOSITORY https://github.com/google/shaderc.git
        # Pinned release rather than the moving main development branch.
        GIT_TAG v2026.4
        PATCH_COMMAND ${Python3_EXECUTABLE} utils/git-sync-deps
    )

    # shaderc's bundled glslang still declares install exports while it is configured
    # as a build-only dependency, causing an invalid SPIRV-Tools export dependency.
    # Suppress install(...) rules only while shaderc is added, then restore the prior
    # project setting so Mikoto's own install rules below remain unaffected.
    function(mikoto_fetch_shaderc)
        mikoto_shader_compiler_debug_flags()
        set(CMAKE_SKIP_INSTALL_RULES TRUE)
        FetchContent_MakeAvailable(shaderc)
    endfunction()
    mikoto_fetch_shaderc()

    # volk
    FetchContent_Declare(volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG vulkan-sdk-1.4.363.0
    )
    FetchContent_MakeAvailable(volk)

    # VulkanMemoryAllocator
    FetchContent_Declare(VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(VulkanMemoryAllocator)

    if(WIN32)
        # D3D12MemoryAllocator
        set(BUILD_DOCUMENTATION OFF CACHE BOOL "")
        FetchContent_Declare(
            D3D12MemoryAllocator
            GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
            GIT_TAG v3.2.0
        )

        FetchContent_MakeAvailable(D3D12MemoryAllocator)

        # Use the same D3D12 declarations in the allocator and its consumers. Loading
        # Windows SDK d3d12.h first hides the fetched header behind its shared guard,
        # leaving the newer d3dx12 helpers without declarations such as tight alignment.
        # These overrides belong to Mikoto's build. D3D12MA's standalone install export
        # must not reference our fetched DirectX targets, whose installation is disabled.
        target_compile_definitions(D3D12MemoryAllocator PUBLIC
            $<BUILD_INTERFACE:D3D12MA_USING_DIRECTX_HEADERS>)
        target_link_libraries(D3D12MemoryAllocator PUBLIC
            $<BUILD_INTERFACE:Microsoft::DirectX-Headers>
            $<BUILD_INTERFACE:Microsoft::DirectX-Guids>)
    endif()

    # Slang compiler, built from the pinned upstream source release.
    # Keep options scoped: Slang configures its own helper targets and subprojects.
    # Shaderc already owns glslang/SPIRV-Tools. Slang's direct SPIR-V emitter still
    # uses its wrapper for optimization/validation, so build that wrapper against
    # the existing targets without adding a second copy of their dependency trees.
    function(mikoto_fetch_slang)
        mikoto_shader_compiler_debug_flags()
        set(SLANG_EMBED_CORE_MODULE ON CACHE BOOL "Embed Slang core module" FORCE)
        set(SLANG_EMBED_CORE_MODULE_SOURCE OFF CACHE BOOL "Exclude core module source" FORCE)
        if(MSVC)
            # Slang's PCH force-includes slang-compiler.h, which needs generated
            # capability/fiddle headers. CLion probes the compiler before those
            # build steps run, so /FI makes project reload fail with C1083.
            # Normal compilation still generates the headers through Slang's targets.
            set(SLANG_ENABLE_PCH OFF CACHE BOOL "Keep MSVC compiler probes independent of generated Slang headers" FORCE)
        endif()
        foreach(option IN ITEMS DXIL GFX SLANGD SLANGC SLANGI SLANGRT SLANG_GLSLANG
                TESTS EXAMPLES RELEASE_LTO SLANG_RHI REPLAYER SLANG_PROXY
                CUDA OPTIX NVAPI AFTERMATH)
            set(SLANG_ENABLE_${option} OFF CACHE BOOL "Minimal Mikoto Slang compiler build" FORCE)
        endforeach()
        set(SLANG_LIB_TYPE STATIC CACHE STRING "Slang compiler library type" FORCE)
        set(SLANG_ENABLE_MIMALLOC OFF CACHE BOOL "Use Slang static-build allocator defaults" FORCE)
        set(SLANG_USE_SYSTEM_VULKAN_HEADERS OFF CACHE BOOL "Use Slang's Vulkan headers" FORCE)
        # Core already provides this header-only target; don't define it twice.
        set(SLANG_USE_SYSTEM_UNORDERED_DENSE ON CACHE BOOL "Reuse Mikoto unordered_dense" FORCE)
        # This is a build dependency, not a separately installed SDK. Avoid exporting
        # references to the engine's existing third-party targets in Slang's exports.
        set(CMAKE_SKIP_INSTALL_RULES TRUE)
        FetchContent_Declare(slang
            GIT_REPOSITORY https://github.com/shader-slang/slang.git
            GIT_TAG v2026.17.1
            GIT_SHALLOW TRUE
            GIT_SUBMODULES_RECURSE TRUE
        )
        FetchContent_MakeAvailable(slang)
        include("${CMAKE_CURRENT_LIST_DIR}/../../CMake/Ports/shader-slang/FixIncludeSourceLocation.cmake")
        mikoto_fix_slang_include_source_location("${slang_SOURCE_DIR}")

        # Compile the runtime helper from the SAME Slang checkout as the compiler.
        # Disabling Slang's own glslang subproject above avoids duplicate targets;
        # it does not stop the compiler from looking for slang-glslang at runtime.
        add_library(mikoto-slang-glslang MODULE
            "${slang_SOURCE_DIR}/source/slang-glslang/slang-glslang.cpp")
        target_include_directories(mikoto-slang-glslang PRIVATE "${slang_SOURCE_DIR}/include")
        target_link_libraries(mikoto-slang-glslang PRIVATE
            glslang SPIRV SPIRV-Tools-opt SPIRV-Tools-link)
        set_target_properties(mikoto-slang-glslang PROPERTIES
            DEBUG_POSTFIX ""
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON)
        if(WIN32)
            set_target_properties(mikoto-slang-glslang PROPERTIES OUTPUT_NAME "slang-glslang")
        else()
            set_target_properties(mikoto-slang-glslang PROPERTIES
                OUTPUT_NAME "slang-glslang-${SLANG_VERSION_NUMERIC}")
        endif()
    endfunction()
    if(NOT MIKOTO_USE_PRECOMPILED_SLANG)
        mikoto_fetch_slang()
    endif()

    target_sources(Mikoto PRIVATE
        "${spirv_reflect_SOURCE_DIR}/spirv_reflect.c")
    target_link_libraries(Mikoto PUBLIC
        volk spirv-cross-core spirv-cross-glsl shaderc)
    target_include_directories(Mikoto PUBLIC
        "${spirv_reflect_SOURCE_DIR}" "${spirv_cross_SOURCE_DIR}"
        "${vulkanmemoryallocator_SOURCE_DIR}/include")
    if(WIN32)
        target_link_libraries(Mikoto PUBLIC D3D12MemoryAllocator Microsoft::DirectX-Headers)
        target_include_directories(Mikoto PUBLIC "${directx-headers_SOURCE_DIR}/include")
        target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
    endif()
endif()

if(MIKOTO_USE_PRECOMPILED_SLANG)
    mikoto_import_precompiled_slang()
endif()
target_link_libraries(Mikoto PUBLIC slang)

# The bundled OpenGL loader is shared by both dependency providers.
target_sources(Mikoto PRIVATE "${MIKOTO_SOURCE_DIR}/Vendor/glad/src/gl.c")
target_include_directories(Mikoto PUBLIC "${MIKOTO_SOURCE_DIR}/Vendor/glad/include")
target_compile_definitions(Mikoto PUBLIC VK_NO_PROTOTYPES MKT_USE_VULKAN_DYNAMIC_RENDERING)

# DCC volume interchange is a CPU authoring dependency, independent of graphics backends.
if(MIKOTO_ENABLE_OPENVDB)
    find_package(OpenVDB CONFIG QUIET)
    if(NOT TARGET OpenVDB::openvdb)
        find_package(OpenVDB REQUIRED)
    endif()
    target_link_libraries(Mikoto PUBLIC OpenVDB::openvdb)
    # OpenVDB's vcpkg Find module discovers compression libraries but omits
    # them from its static imported target. Carry their transitive targets
    # through Mikoto so editor/runtime executables resolve compressed VDB I/O.
    get_target_property(mikoto_openvdb_type OpenVDB::openvdb TYPE)
    if(mikoto_openvdb_type STREQUAL "STATIC_LIBRARY")
        if(OpenVDB_USES_BLOSC)
            foreach(mikoto_blosc_target IN ITEMS blosc_static Blosc::blosc blosc)
                if(TARGET ${mikoto_blosc_target})
                    target_link_libraries(Mikoto PUBLIC ${mikoto_blosc_target})
                    break()
                endif()
            endforeach()
        endif()
        if(OpenVDB_USES_ZLIB)
            target_link_libraries(Mikoto PUBLIC ZLIB::ZLIB)
        endif()
    endif()
    unset(mikoto_openvdb_type)
    unset(mikoto_blosc_target)
    target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_OPENVDB=1)
else()
    target_compile_definitions(Mikoto PUBLIC MIKOTO_ENABLE_OPENVDB=0)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../CMake/NRD.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/../../CMake/OIDN.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/../../CMake/DLSS.cmake")
