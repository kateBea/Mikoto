cmake_minimum_required(VERSION 3.22)

# ======================================
# CMake Policy Settings
# ======================================
# CMP0069: INTERPROCEDURAL_OPTIMIZATION (IPO / LTO)
# Reference: https://cmake.org/cmake/help/latest/policy/CMP0069.html
# --------------------------------------
# CMP0135: DOWNLOAD_EXTRACT_TIMESTAMP
# Reference: https://cmake.org/cmake/help/latest/policy/CMP0135.html
# --------------------------------------
# CMP0077
# For NFD
# Reference: https://cmake.org/cmake/help/latest/policy/CMP0077.html
# --------------------------------------
cmake_policy(SET CMP0069 NEW)
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0077 NEW)

project(Mikoto VERSION 1.0 )

# ======================================
# Mikoto Options
# ======================================
option(MIKOTO_USE_XCB_WINDOW "Use XCB for Linux windowing" OFF)
option(MIKOTO_USE_WIN32_WINDOW "Use Win32 for Windows windowing" OFF)
option(MIKOTO_ENABLE_TRACY_PROFILING "Enable tracy profiling" OFF)

# ======================================
# Language requirements
# ======================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ======================================
# CMake includes
# ======================================
include(FetchContent)

# ======================================
# Configuration for Networking
# ======================================
find_package(OpenSSL QUIET)

if (NOT OpenSSL_FOUND)
    message(WARNING "OpenSSL not found! Mikoto will use HTTP-only mode.")
    set(MIKOTO_ALLOW_SSL OFF)
else()
    message(STATUS "OpenSSL found: ${OpenSSL_VERSION}")
    set(MIKOTO_ALLOW_SSL ON)
endif()

# ======================================
# Third-Party dependencies import
# ======================================

# These options are intentionally global: upstream projects use these generic
# cache names, and Mikoto builds its third-party libraries statically.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

# =======================================================
# fmt
# =======================================================
FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.0.2
)
FetchContent_MakeAvailable(fmt)

# =======================================================
# GLFW
# =======================================================
set(GLFW_INSTALL OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
)
FetchContent_MakeAvailable(glfw)

# =======================================================
# ImGui (docking branch)
# =======================================================
FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.9b-docking
)
FetchContent_MakeAvailable(imgui)

# =======================================================
# ImGuizmo
# =======================================================
FetchContent_Declare(ImGuizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG 1.10
)
FetchContent_MakeAvailable(ImGuizmo)

# =======================================================
# cpptrace
# =======================================================
FetchContent_Declare(cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v0.7.3
)
FetchContent_MakeAvailable(cpptrace)

# =======================================================
# unordered_dense (header-only)
# =======================================================
FetchContent_Declare(unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v4.4.0
)
FetchContent_MakeAvailable(unordered_dense)

# =======================================================
# EnTT
# =======================================================
FetchContent_Declare(entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v4.0.0
)
FetchContent_MakeAvailable(entt)

# =======================================================
# Direct3D 12 headers (According to their cmake comments this is technically
# not needed because they are set if its cmake is top level)
# =======================================================
set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "")
set(DXHEADERS_INSTALL OFF CACHE BOOL "")
set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "")

FetchContent_Declare(
        DirectX-Headers
        GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
        GIT_TAG        v1.619.4
)

FetchContent_MakeAvailable(DirectX-Headers)

# =======================================================
# nativefiledialog-extended
# =======================================================
FetchContent_Declare(nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG v1.3.0
)
FetchContent_MakeAvailable(nfd)

# =======================================================
# PFD
# =======================================================
FetchContent_Declare(
        pfd
        GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
        GIT_TAG main
)
FetchContent_MakeAvailable(pfd)

# =======================================================
# nlohmann_json
# =======================================================
FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# =======================================================
# Miniaudio
# =======================================================
FetchContent_Declare(miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.23
)
FetchContent_MakeAvailable(miniaudio)

# =======================================================
# SPIRV-Reflect
# =======================================================
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build SPIRV-Reflect as a static library")
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "Disable SPIRV-Reflect tests")
set(SPIRV_REFLECT_ENABLE_ASSERTS OFF CACHE BOOL "Disable asserts for SPIRV-Reflect")
set(SPIRV_REFLECT_ENABLE_ASAN OFF CACHE BOOL "Disable address sanitizer")
set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "Disable SPIRV-Reflect install targets")
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Build spirv-reflect executable")
FetchContent_Declare(spirv_reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG main
)
FetchContent_MakeAvailable(spirv_reflect)

# =======================================================
# SPIRV-Cross
# =======================================================
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        spirv_cross
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG        main
)
FetchContent_MakeAvailable(spirv_cross)

# =======================================================
# shaderc
# =======================================================
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
        GIT_TAG v2026.2
        PATCH_COMMAND ${Python3_EXECUTABLE} utils/git-sync-deps
)

# shaderc's bundled glslang still declares install exports while it is configured
# as a build-only dependency, causing an invalid SPIRV-Tools export dependency.
# Suppress install(...) rules only while shaderc is added, then restore the prior
# project setting so Mikoto's own install rules below remain unaffected.
if(DEFINED CMAKE_SKIP_INSTALL_RULES)
    # Preserve an explicit setting from the parent project before overriding it.
    set(MIKOTO_PREVIOUS_SKIP_INSTALL_RULES ${CMAKE_SKIP_INSTALL_RULES})
    set(MIKOTO_HAD_SKIP_INSTALL_RULES TRUE)
endif()
set(CMAKE_SKIP_INSTALL_RULES TRUE)
FetchContent_MakeAvailable(shaderc)
if(MIKOTO_HAD_SKIP_INSTALL_RULES)
    # Restore the parent setting exactly when it existed before shaderc.
    set(CMAKE_SKIP_INSTALL_RULES ${MIKOTO_PREVIOUS_SKIP_INSTALL_RULES})
else()
    # Otherwise restore the original undefined state instead of leaving a false value.
    unset(CMAKE_SKIP_INSTALL_RULES)
endif()
unset(MIKOTO_PREVIOUS_SKIP_INSTALL_RULES)
unset(MIKOTO_HAD_SKIP_INSTALL_RULES)

# =======================================================
# volk
# =======================================================
FetchContent_Declare(volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.4.304
)
FetchContent_MakeAvailable(volk)

# =======================================================
# VulkanMemoryAllocator
# =======================================================
FetchContent_Declare(VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# =======================================================
# yaml-cpp
# =======================================================
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "Disable source formatting")
set(YAML_CPP_INSTALL OFF CACHE BOOL "Disable yaml-cpp install targets")
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml-cpp as static library")
FetchContent_Declare(yaml_cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml_cpp)

# =======================================================
# FreeType
# =======================================================
# FreeType can optionally use HarfBuzz for advanced text shaping. Mikoto only
# rasterizes glyphs for MSDF generation, so avoid that unnecessary dependency.
set(FT_REQUIRE_HARFBUZZ OFF CACHE BOOL "Disable HarfBuzz requirement in FreeType" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz support in FreeType" FORCE)
set(FT_WITH_HARFBUZZ OFF CACHE BOOL "Don't build with HarfBuzz" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT2_BUILD_LIBRARY ON CACHE BOOL "" FORCE)
set(FT_WITH_CMAKE ON CACHE BOOL "" FORCE)

find_package(Freetype QUIET)
if (NOT Freetype_FOUND)
    message(WARNING "Freetype was not found! Using bundled Freetype")
    set(MSDFGEN_CORE_ONLY ON)
else()
    message(STATUS "Freetype found: ${Freetype_INCLUDE_DIRS}")
    set(MSDFGEN_CORE_ONLY OFF)
endif()

FetchContent_Declare(freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG VER-2-13-2
)
FetchContent_MakeAvailable(freetype)

# =======================================================
# JoltPhysics
# =======================================================
FetchContent_Declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.3.0
        SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)

# =======================================================
# Assimp
# =======================================================
set(ASSIMP_INSTALL OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_BLEND_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "")
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "")
FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
)
FetchContent_MakeAvailable(assimp)

# =======================================================
# msdfgen
# =======================================================
set(MSDFGEN_USE_VCPKG OFF)
set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_DISABLE_SVG ON)
set(MSDFGEN_USE_SKIA OFF)
set(MSDFGEN_DISABLE_PNG ON)
set(MSDFGEN_DYNAMIC_RUNTIME OFF)
FetchContent_Declare(msdfgen
        GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
        GIT_TAG v1.12
)
FetchContent_MakeAvailable(msdfgen)

# =======================================================
# msdf-atlas-gen
# =======================================================
set(MSDF_ATLAS_BUILD_STANDALONE OFF)
set(MSDF_ATLAS_USE_VCPKG OFF)
set(MSDF_ATLAS_USE_SKIA OFF)
set(MSDF_ATLAS_DYNAMIC_RUNTIME OFF)
set(MSDF_ATLAS_MSDFGEN_EXTERNAL ON)
set(MSDF_ATLAS_INSTALL OFF)
FetchContent_Declare(msdf_atlas_gen
        GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
        GIT_TAG v1.3
)
FetchContent_MakeAvailable(msdf_atlas_gen)

# =======================================================
# spdlog
# =======================================================
FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0  # latest stable release
)
FetchContent_MakeAvailable(spdlog)

# =======================================================
# stb
# =======================================================
FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master  # stable is usually master for stb
)
FetchContent_MakeAvailable(stb)

# =======================================================
# tomlplusplus
# =======================================================
fetchcontent_declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# =======================================================
# GLM
# =======================================================
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
)

FetchContent_MakeAvailable(glm)

# =======================================================
# Taskflow
# =======================================================
set(TF_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_PROFILER OFF CACHE BOOL "" FORCE)
set(TF_BUILD_CUDA OFF CACHE BOOL "" FORCE)
set(TF_BUILD_SYCL OFF CACHE BOOL "" FORCE)
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        taskflow
        GIT_REPOSITORY https://github.com/taskflow/taskflow.git
        GIT_TAG v3.7.0
)

FetchContent_MakeAvailable(taskflow)

# =======================================================
# sol2
# =======================================================
FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG v3.3.0
)

FetchContent_MakeAvailable(sol2)

# =======================================================
# Asio
# =======================================================
FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG master
)

FetchContent_MakeAvailable(asio)

# =======================================================
# Tracy
# =======================================================
FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.13.1
)

FetchContent_MakeAvailable(tracy)

# =======================================================
# Ozz Animation
# =======================================================
set(ozz_build_tools ON CACHE BOOL "" FORCE)
set(ozz_build_gltf ON CACHE BOOL "" FORCE)
set(ozz_build_fbx ON CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_data OFF CACHE BOOL "" FORCE)
set(ozz_build_simd_ref OFF CACHE BOOL "" FORCE)
set(ozz_build_msvc_rt_dll OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        ozz-animation
        GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
        GIT_TAG master
)

FetchContent_MakeAvailable(ozz-animation)

# =======================================================
# D3D12MemoryAllocator
# =======================================================
set(BUILD_DOCUMENTATION OFF CACHE BOOL "")
FetchContent_Declare(
        D3D12MemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
        GIT_TAG v3.2.0
)

FetchContent_MakeAvailable(D3D12MemoryAllocator)

# =======================================================
# Slang (target AMD64 architectures by default)
# =======================================================
if (MSVC)
    set(SLANG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/Slang/slang-2026.17-windows-x86_64")
    message(STATUS "Setup slang for MSVC x86_64. SlangRoot is ${SLANG_ROOT}")

    set(SLANG_LIB_DIR "${SLANG_ROOT}/lib")
    set(SLANG_INCLUDE_DIR "${SLANG_ROOT}/include")

    set(SLANG_LIBS
            "${SLANG_LIB_DIR}/slang.lib"
            "${SLANG_LIB_DIR}/slang-compiler.lib"
    )

elseif(UNIX)
    set(SLANG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/Slang/slang-2026.17-linux-x86_64-glibc-2.28")
    message(STATUS "Setup slang for Unix x86_64. SlangRoot is ${SLANG_ROOT}")

    set(SLANG_LIB_DIR "${SLANG_ROOT}/lib")
    set(SLANG_INCLUDE_DIR "${SLANG_ROOT}/include")

    set(SLANG_LIBS
            "${SLANG_LIB_DIR}/libslang.so"
            "${SLANG_LIB_DIR}/libslang-compiler.so"
    )
endif()

# =======================================================
# Filewatch
# =======================================================
# Using https://github.com/ThomasMonkman/filewatch/tree/feature/fix-filesystem (currently working branch)
# It is single header
set(FILEWATCH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/Filewatch")
set(FILEWATCH_INCLUDE_DIR "${FILEWATCH_ROOT}")

# =======================================================
# ImOGuizmo
# =======================================================
# Using https://github.com/fknfilewalker/imoguizmo.git
# It is single header
set(IMOGUIZMO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/ImOGuizmo")
set(IMOGUIZMO_INCLUDE_DIR "${IMOGUIZMO_ROOT}")

# =======================================================
# imgui-node-editor
# =======================================================
# https://github.com/thedmd/imgui-node-editor.git
# Slightly modified, in Mikoto we use imgui 1.90+ which has removed some functions this library relied on
# I modified the library and added some of the missing functions. The verison used for this change was the one from master branch
# as of commit https://github.com/thedmd/imgui-node-editor/commit/e78e447900909a051817a760efe13fe83e6e1afc
set(IMGUI_NO_EDITOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui_node_editor")
set(IMGUI_NO_EDITOR_INCLUDE_DIR "${IMGUI_NO_EDITOR_ROOT}")

# =======================================================
# OpenCL-SDK
# =======================================================
set(OPENCL_SDK_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(OPENCL_SDK_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(OPENCL_SDK_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        OpenCL-SDK
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-SDK.git
        GIT_TAG        main   # You can lock to a specific release
)

FetchContent_MakeAvailable(OpenCL-SDK)

# =======================================================
# MeshOptimizer
# =======================================================
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(MESHOPT_STABLE_EXPORTS OFF CACHE BOOL "" FORCE)
set(MESHOPT_WERROR OFF CACHE BOOL "" FORCE)
set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        meshoptimizer
        GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
        GIT_TAG        v1.0
)

FetchContent_MakeAvailable(meshoptimizer)

# =======================================================
# EASTL
# =======================================================
set(EASTL_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        EASTL
        GIT_REPOSITORY https://github.com/electronicarts/EASTL.git
        GIT_TAG        master
)

FetchContent_MakeAvailable(EASTL)

# =======================================================
# TinyGLTF
# =======================================================
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_BUILDER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG        v2.9.7
)

FetchContent_MakeAvailable(tinygltf)

# =======================================================
# magic_enum
# =======================================================
FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG        v0.9.7
)

FetchContent_MakeAvailable(magic_enum)

# =======================================================
# reflect
# =======================================================
FetchContent_Declare(
        reflect
        GIT_REPOSITORY https://github.com/qlibs/reflect.git
        GIT_TAG        v1.2.6
)

FetchContent_MakeAvailable(reflect)

# =======================================================
# KTX-Software
# =======================================================
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_PY OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS_CTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_KTX2 ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_WRITE ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TRANSCODE ON CACHE BOOL "" FORCE)
FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG        v4.4.2
)

FetchContent_MakeAvailable(ktx)

# =======================================================
# FreeImage
# =======================================================
set(BUILD_JXR OFF CACHE BOOL "" FORCE)
set(BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(BUILD_LIBPNG ON CACHE BOOL "" FORCE)
set(BUILD_LIBTIFF ON CACHE BOOL "" FORCE)
set(BUILD_WEBP ON CACHE BOOL "" FORCE)
set(BUILD_LIBRAWLITE OFF CACHE BOOL "" FORCE)
set(BUILD_OPENEXR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        FreeImage
        GIT_REPOSITORY https://github.com/danoli3/FreeImage.git
        GIT_TAG        3.19.10
)

FetchContent_MakeAvailable(FreeImage)

# ======================================
# Project sources
# ======================================
file(GLOB SOURCE_FILES Sources/*.cc)

# ======================================
# ImGui sources (compiled directly into Mikoto)
# ======================================
set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

if (MSVC)
    list(APPEND IMGUI_SOURCES
            ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    )
endif ()

# ImGuizmo sources
set(IMGUIZMO_SOURCES
        ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
        ${imguizmo_SOURCE_DIR}/src/GraphEditor.cpp
        ${imguizmo_SOURCE_DIR}/src/ImCurveEdit.cpp
        ${imguizmo_SOURCE_DIR}/src/ImGradient.cpp
        ${imguizmo_SOURCE_DIR}/src/ImSequencer.cpp
)

set(IMGUI_NODES_SOURCES
        ${IMGUI_NO_EDITOR_ROOT}/imgui_node_editor.cpp
        ${IMGUI_NO_EDITOR_ROOT}/imgui_node_editor_api.cpp
        ${IMGUI_NO_EDITOR_ROOT}/crude_json.cpp
        ${IMGUI_NO_EDITOR_ROOT}/imgui_canvas.cpp
)

# SpirV-Reflect sources
set(SPIRV_REFLECT_SOURCES
        ${spirv_reflect_SOURCE_DIR}/spirv_reflect.c
)

# Miniaudio sources
set(MINIAUDIO_SOURCES
        ${miniaudio_SOURCE_DIR}/miniaudio.c
)

# Combine all sources into Mikoto
set(ALL_SOURCES
        ${SOURCE_FILES}
        ${IMGUI_SOURCES}
        ${IMGUIZMO_SOURCES}
        ${SPIRV_REFLECT_SOURCES}
        ${MINIAUDIO_SOURCES}
        ${IMGUI_NODES_SOURCES}
        ImGui/ImGuiWidget.inl
)

# ======================================
# Libraries to link
# ======================================
set(LIBRARIES
    glfw
    fmt::fmt
    EnTT::EnTT
    volk
    assimp
    yaml-cpp
    glm
    nfd
    cpptrace::cpptrace
    freetype
    Jolt
    msdfgen::msdfgen
    msdf-atlas-gen::msdf-atlas-gen
    unordered_dense::unordered_dense
    nlohmann_json::nlohmann_json
    sol2::sol2
    Tracy::TracyClient
    OpenCL::OpenCL
    ozz_animation
    ozz_animation_tools
    ozz_animation_offline
    meshoptimizer
    tinygltf
    ktx
    portable_file_dialogs
    ${SLANG_LIBS}
    EASTL
    EABase
    FreeImage
    magic_enum::magic_enum
    spirv-cross-core
    spirv-cross-glsl
    shaderc
)

# ======================================
# Debug print Cmake variables
# ======================================
get_cmake_property(_variableNames VARIABLES)
foreach(_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

add_library(${PROJECT_NAME} STATIC ${ALL_SOURCES})

# Lua needs to be installed on Linux
if(UNIX)
    find_package(Lua REQUIRED)
    list(APPEND LIBRARIES pthread OpenSSL::SSL OpenSSL::Crypto)
endif()

# For MSVC we find lua from Mikoto/Vendors folder
if (MSVC OR WIN32)
    set(LUA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/Lua/lua-5.4.2_Win64_vc17_lib")

    set(LUA_INCLUDE_DIR "${LUA_ROOT}/include")
    set(LUA_LIBRARIES "${LUA_ROOT}/lua54.lib")

    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)

    message(STATUS "Linking against Lua library: ${LUA_LIBRARIES}")
    message(STATUS "Include against Lua library: ${LUA_INCLUDE_DIR}")

    target_compile_options(Jolt PRIVATE
            /wd4865
            /WX-
            /Zc:enumTypes-
    )

    list(APPEND LIBRARIES D3D12MemoryAllocator Microsoft::DirectX-Headers)
endif()

# Windowing system
if(MIKOTO_USE_XCB_WINDOW)
    message("Using XCB for window")
    list(APPEND LIBRARIES xcb xcb-keysyms xcb-xfixes xcb-xkb)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBRARIES} ${LUA_LIBRARIES})

# ======================================
# Include directories
# ======================================
target_include_directories(${PROJECT_NAME} PUBLIC .)
target_include_directories(${PROJECT_NAME} PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imguizmo_SOURCE_DIR}/src
    ${spirv_reflect_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}
    ${fmt_SOURCE_DIR}/include
    ${glfw_SOURCE_DIR}/include
    ${entt_SOURCE_DIR}/single_include
    ${yaml_cpp_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
    ${unordered_dense_SOURCE_DIR}/include
    ${nfd_SOURCE_DIR}/src/include
    ${freetype_SOURCE_DIR}/include
    ${joltphysics_SOURCE_DIR}/Jolt
    ${assimp_SOURCE_DIR}/include
    ${msdfgen_SOURCE_DIR}/include
    ${msdf_atlas_gen_SOURCE_DIR}/include
    ${spdlog_SOURCE_DIR}/include
    ${tomlplusplus_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${glm_SOURCE_DIR}
    ${vulkanmemoryallocator_SOURCE_DIR}/include
    ${taskflow_SOURCE_DIR}
    ${sol2_SOURCE_DIR}/include
    ${asio_SOURCE_DIR}/include
    ${tracy_SOURCE_DIR}/asio/include
    ${LUA_INCLUDE_DIR}
    ${FILEWATCH_INCLUDE_DIR}
    ${IMOGUIZMO_INCLUDE_DIR}
    ${OpenCL-SDK_SOURCE_DIR}/lib/include
    ${OpenCL-SDK_SOURCE_DIR}/external/OpenCL-Headers
    ${OpenCL-SDK_SOURCE_DIR}/external/OpenCL-CLHPP/include
    ${ozz_SOURCE_DIR}/include
    ${ktx_SOURCE_DIR}
    ${meshoptimizer_SOURCE_DIR}/src
    ${tinygltf_SOURCE_DIR}
    ${IMGUI_NO_EDITOR_INCLUDE_DIR}
    ${SLANG_INCLUDE_DIR}
    ${eastl_SOURCE_DIR}/include
    ${freeimage_SOURCE_DIR}/Source
    ${magic_enum_SOURCE_DIR}/include
    ${reflect_SOURCE_DIR}
    ${spirv_cross_SOURCE_DIR}
    ${directx-headers_SOURCE_DIR}/include
    ${d3d12memoryallocator_SOURCE_DIR}/include
)

# ======================================
# Compile definitions
# ======================================
target_compile_definitions(${PROJECT_NAME} PUBLIC GLFW_INCLUDE_NONE)
target_compile_definitions(${PROJECT_NAME} PUBLIC SPDLOG_FMT_EXTERNAL)
target_compile_definitions(${PROJECT_NAME} PUBLIC USE_GLFW_KEY_CODES)
target_compile_definitions(${PROJECT_NAME} PUBLIC USE_GLFW_MOUSE_CODES)
target_compile_definitions(${PROJECT_NAME} PUBLIC USE_GLFW_INPUT)

target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES)
target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_FORCE_RADIANS)
target_compile_definitions(${PROJECT_NAME} PUBLIC GLM_ENABLE_EXPERIMENTAL)

target_compile_definitions(${PROJECT_NAME} PUBLIC MKT_USE_VULKAN_BINDLESS)
target_compile_definitions(${PROJECT_NAME} PUBLIC MKT_USE_VULKAN_DYNAMIC_RENDERING)

target_compile_definitions(${PROJECT_NAME} PUBLIC TRACY_ENABLE)

# Required by FreeImage if linked statically
target_compile_definitions(${PROJECT_NAME} PUBLIC FREEIMAGE_LIB)

# Tracy
if(MIKOTO_ENABLE_TRACY_PROFILING)
    message("Enabling Tracy profiling")
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIKOTO_ENABLE_TRACY)
endif()

# Windowing system
if(MIKOTO_USE_XCB_WINDOW)
    message("Using XCB for window")
    target_compile_definitions(${PROJECT_NAME} PUBLIC MKT_USE_XCB_WINDOW)
endif()

if(MIKOTO_USE_WIN32_WINDOW)
    message("Using Win32 for window")
    target_compile_definitions(${PROJECT_NAME} PUBLIC MKT_USE_WIN32_WINDOW)
endif()

if (MIKOTO_ALLOW_SSL)
    message("Using OpenSSL for HTTPS support")
    target_compile_definitions(${PROJECT_NAME} PUBLIC MIKOTO_OPENSSL_AVAILABLE)
endif()

# ======================================
# Custom Commands
# ======================================

# Correct paths based on your project layout
set(RESOURCES_SRC "${Mikoto_SOURCE_DIR}/Resources")
set(RESOURCES_DST "${Mikoto_BINARY_DIR}/Resources")
set(INSTALL_SCRIPT "${RESOURCES_DST}/installs.sh")

# Copy all resources into the build directory
add_custom_command(
        OUTPUT ${RESOURCES_DST}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCES_SRC} ${RESOURCES_DST}
        COMMAND chmod +x ${INSTALL_SCRIPT}
        COMMENT "Copying all resources to build directory"
)

# For installing dependencies on Linux
if(UNIX)
    add_custom_target(InstallDependencies
            DEPENDS ${RESOURCES_DST}
            COMMAND ${INSTALL_SCRIPT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running installs.sh to install Linux dependencies"
    )
endif()
