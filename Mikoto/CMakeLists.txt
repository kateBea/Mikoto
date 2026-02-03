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
# ======================================
cmake_policy(SET CMP0069 NEW)
cmake_policy(SET CMP0135 NEW)
cmake_policy(SET CMP0077 NEW)

project(Mikoto)

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
# Taskflow
# ======================================
set(TF_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_PROFILER OFF CACHE BOOL "" FORCE)
set(TF_BUILD_CUDA OFF CACHE BOOL "" FORCE)
set(TF_BUILD_SYCL OFF CACHE BOOL "" FORCE)
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# ======================================
# GLFW options
# ======================================
set(GLFW_INSTALL OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# ======================================
# SPIR-V-Reflect minimal options
# ======================================
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build SPIRV-Reflect as a static library")
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "Disable SPIRV-Reflect tests")
set(SPIRV_REFLECT_ENABLE_ASSERTS OFF CACHE BOOL "Disable asserts for SPIRV-Reflect")
set(SPIRV_REFLECT_ENABLE_ASAN OFF CACHE BOOL "Disable address sanitizer")
set(SPIRV_REFLECT_INSTALL OFF CACHE BOOL "Disable SPIRV-Reflect install targets")
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Build spirv-reflect executable")

# ======================================
# MSDFGEN / MSDF Atlas options
# ======================================
set(MSDF_ATLAS_BUILD_STANDALONE OFF)
set(MSDF_ATLAS_USE_VCPKG OFF)
set(MSDF_ATLAS_USE_SKIA OFF)
set(MSDF_ATLAS_DYNAMIC_RUNTIME OFF)
set(MSDF_ATLAS_MSDFGEN_EXTERNAL ON)
set(MSDF_ATLAS_INSTALL OFF)
set(BUILD_SHARED_LIBS OFF)

find_package(Freetype QUIET)

if (NOT Freetype_FOUND)
    message(WARNING "Freetype was not found! Using bundled Freetype")
    set(MSDFGEN_CORE_ONLY ON)
else()
    message(STATUS "Freetype found: ${Freetype_INCLUDE_DIRS}")
    set(MSDFGEN_CORE_ONLY OFF)
endif()

# ======================================
# Freetype options
# ======================================
# FreeType can optionally use HarfBuzz for advanced text shaping (ligatures, kerning, etc.)
# Mikoto only uses FreeType to rasterize glyphs for MSDF generation — no need for HarfBuzz.
#
# Setting FT_REQUIRE_HARFBUZZ and FT_DISABLE_HARFBUZZ disables the dependency entirely,
# preventing the "Required version higher than found version ()" warning.
set(FT_REQUIRE_HARFBUZZ OFF CACHE BOOL "Disable HarfBuzz requirement in FreeType" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable HarfBuzz support in FreeType" FORCE)
set(FT_WITH_HARFBUZZ OFF CACHE BOOL "Don't build with HarfBuzz" FORCE)

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)

# Force CMake-based build (not the legacy Windows one)
set(FT2_BUILD_LIBRARY ON CACHE BOOL "" FORCE)
set(FT_WITH_CMAKE ON CACHE BOOL "" FORCE)

set(MSDFGEN_USE_VCPKG OFF)
set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_DISABLE_SVG ON)
set(MSDFGEN_USE_SKIA OFF)
set(MSDFGEN_DISABLE_PNG ON)
set(MSDFGEN_DYNAMIC_RUNTIME OFF)

# ======================================
# File system watcher
# ======================================
set(VERBOSE OFF CACHE BOOL "Disable verbose mode" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Do not build shared libs" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build static libs only" FORCE)
set(BUILD_TEST_APP OFF CACHE BOOL "Disable test app" FORCE)
set(EFSW_INSTALL OFF CACHE BOOL "Disable install targets" FORCE)

# ======================================
# OpenCL-SDK
# ======================================
set(OPENCL_SDK_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(OPENCL_SDK_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(OPENCL_SDK_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# ======================================
# yaml-cpp minimal options
# ======================================
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "Disable source formatting")
set(YAML_CPP_INSTALL OFF CACHE BOOL "Disable yaml-cpp install targets")
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml-cpp as static library")

# ======================================
# Assimp options
# ======================================
set(ASSIMP_INSTALL OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

# Only build common importers
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_BLEND_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "")

# Disable exporters if you don't need them
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "")

# Disable tests, docs, examples
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "")
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "")

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

# fmt
FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.0.2
)
FetchContent_MakeAvailable(fmt)

# glfw
FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
)
FetchContent_MakeAvailable(glfw)


# ImGui (docking branch)
FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# ImGuizmo
FetchContent_Declare(ImGuizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG master
)
FetchContent_MakeAvailable(ImGuizmo)

# toml++ (header-only)
FetchContent_Declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# cpptrace
FetchContent_Declare(cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v0.7.3
)
FetchContent_MakeAvailable(cpptrace)

# unordered_dense (header-only)
FetchContent_Declare(unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v4.4.0
)
FetchContent_MakeAvailable(unordered_dense)

# EnTT
FetchContent_Declare(entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.0
)
FetchContent_MakeAvailable(entt)


# nlohmann_json
FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Miniaudio
FetchContent_Declare(miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.23
)
FetchContent_MakeAvailable(miniaudio)

# SpirV-Reflect
FetchContent_Declare(spirv_reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG main
)
FetchContent_MakeAvailable(spirv_reflect)

# volk
FetchContent_Declare(volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.4.304
)
FetchContent_MakeAvailable(volk)


# VulkanMemoryAllocator
FetchContent_Declare(VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# yaml-cpp
FetchContent_Declare(yaml_cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml_cpp)

# nativefiledialog-extended
FetchContent_Declare(nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG v1.1.1
)
FetchContent_MakeAvailable(nfd)

# freetype
FetchContent_Declare(freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG VER-2-13-2
)
FetchContent_MakeAvailable(freetype)

# JoltPhysics
FetchContent_Declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.3.0
        SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)

# assimp
FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
)
FetchContent_MakeAvailable(assimp)

# msdfgen
FetchContent_Declare(msdfgen
        GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
        GIT_TAG v1.12
)
FetchContent_MakeAvailable(msdfgen)

# msdf-atlas-gen
FetchContent_Declare(msdf_atlas_gen
        GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
        GIT_TAG v1.3
)
FetchContent_MakeAvailable(msdf_atlas_gen)

# spdlog
FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0  # latest stable release
)
FetchContent_MakeAvailable(spdlog)

# stb (header-only)
FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master  # stable is usually master for stb
)
FetchContent_MakeAvailable(stb)

# Tom plus plus
fetchcontent_declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
)

FetchContent_MakeAvailable(glm)

# Taskflow
FetchContent_Declare(
        taskflow
        GIT_REPOSITORY https://github.com/taskflow/taskflow.git
        GIT_TAG v3.7.0
)

FetchContent_MakeAvailable(taskflow)

# Sol2
FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG v3.3.0
)

FetchContent_MakeAvailable(sol2)

FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG master
)

FetchContent_MakeAvailable(asio)

# Tracy
FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(tracy)

#Filewatch
# Using https://github.com/ThomasMonkman/filewatch/tree/feature/fix-filesystem (currently working branch)
# It is single header
set(FILEWATCH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/Filewatch")
set(FILEWATCH_INCLUDE_DIR "${FILEWATCH_ROOT}")

# ImOGuizmo
# Using https://github.com/fknfilewalker/imoguizmo.git
# It is single header
set(IMOGUIZMO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/ImOGuizmo")
set(IMOGUIZMO_INCLUDE_DIR "${IMOGUIZMO_ROOT}")

# -------------------------------------------------------
# OpenCL-SDK
# -------------------------------------------------------
FetchContent_Declare(
        OpenCL-SDK
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-SDK.git
        GIT_TAG        main   # You can lock to a specific release
)

FetchContent_MakeAvailable(OpenCL-SDK)

# harfbuzz (not needed if you don't do complex text layout)
#FetchContent_Declare(
#        harfbuzz
#        GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
#        GIT_TAG main
#)
#FetchContent_MakeAvailable(harfbuzz)

# On linux OpenSSL can be fetched and build with Mikoto
# for HTTPS support
if (UNIX AND NOT OpenSSL_FOUND)
    FetchContent_Declare(
            openssl
            URL https://www.openssl.org/source/openssl-3.6.0.tar.gz
    )

    FetchContent_MakeAvailable(openssl)

    # ================
    set(OPENSSL_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl-src)
    set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include)
    set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/config)
    ExternalProject_Add(
            OpenSSL
            SOURCE_DIR ${OPENSSL_SOURCE_DIR}
            GIT_REPOSITORY https://github.com/openssl/openssl.git
            GIT_TAG OpenSSL_1_1_1n
            CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/config --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}
            BUILD_COMMAND make
            INSTALL_COMMAND make install
            BUILD_BYPRODUCTS
            ${OPENSSL_INSTALL_DIR}/lib/libssl.a
            ${OPENSSL_INSTALL_DIR}/lib/libcrypto.a
            UPDATE_COMMAND ""               # <— prevent automatic updates/rebuilds every time
            BUILD_ALWAYS 0                  # <— do not always rebuild
    )


    # We cannot use find_library because ExternalProject_Add() is performed at build time.
    # And to please the property INTERFACE_INCLUDE_DIRECTORIES,
    # we make the include directory in advance.
    file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

    # Make sure the directories exist
    file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

    add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
    set_property(TARGET OpenSSL::SSL PROPERTY IMPORTED_LOCATION
            ${OPENSSL_INSTALL_DIR}/lib/libssl.a)
    set_property(TARGET OpenSSL::SSL PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
    add_dependencies(OpenSSL::SSL OpenSSL)

    add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
    set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_LOCATION
            ${OPENSSL_INSTALL_DIR}/lib/libcrypto.a)
    set_property(TARGET OpenSSL::Crypto PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
    add_dependencies(OpenSSL::Crypto OpenSSL)
endif ()

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

# ImGuizmo sources
set(IMGUIZMO_SOURCES
        ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
        ${imguizmo_SOURCE_DIR}/GraphEditor.cpp
        ${imguizmo_SOURCE_DIR}/ImCurveEdit.cpp
        ${imguizmo_SOURCE_DIR}/ImGradient.cpp
        ${imguizmo_SOURCE_DIR}/ImSequencer.cpp
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
)

# ======================================
# Debug print Cmake variables
# ======================================
#get_cmake_property(_variableNames VARIABLES)
#    foreach(_variableName ${_variableNames})
#        message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

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
        ${imguizmo_SOURCE_DIR}
        ${spirv_reflect_SOURCE_DIR}
        ${miniaudio_SOURCE_DIR}
        ${fmt_SOURCE_DIR}/include
        ${glfw_SOURCE_DIR}/include
        ${entt_SOURCE_DIR}/single_include
        ${yaml_cpp_SOURCE_DIR}/include
        ${nlohmann_json_SOURCE_DIR}/include
        ${unordered_dense_SOURCE_DIR}/include
        ${nfd_SOURCE_DIR}/src
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
