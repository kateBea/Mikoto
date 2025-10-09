cmake_minimum_required(VERSION 3.22)

# ======================================
# CMake Policy Settings
# ======================================
# CMP0069: INTERPROCEDURAL_OPTIMIZATION (IPO / LTO)
# --------------------------------------
# When enabled, this policy ensures that CMake enforces interprocedural
# optimization (also known as link-time optimization, or LTO) properly.
# Some external dependencies (like FreeType or Assimp) declare the
# INTERPROCEDURAL_OPTIMIZATION property, but older policies would silently
# ignore it. Setting CMP0069 to NEW ensures consistent LTO behavior and
# silences related warnings.
#
# Reference: https://cmake.org/cmake/help/latest/policy/CMP0069.html
#
# CMP0135: DOWNLOAD_EXTRACT_TIMESTAMP
# --------------------------------------
# When using FetchContent_Declare() with a URL, this policy controls the
# timestamps applied to extracted files.
#   - OLD behavior: keeps timestamps from the original archive.
#   - NEW behavior: sets timestamps to the extraction time.
# Setting CMP0135 to NEW improves build reproducibility and avoids
# unnecessary rebuilds when dependencies are updated.
#
# Reference: https://cmake.org/cmake/help/latest/policy/CMP0135.html
#
# Both are safe and recommended to set to NEW for modern CMake projects.
# ======================================
cmake_policy(SET CMP0069 NEW)
cmake_policy(SET CMP0135 NEW)

project(Mikoto)

# ======================================
# Language requirements
# ======================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ======================================
# GLFW options
# ======================================
set(GLFW_INSTALL OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

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

set(MSDFGEN_CORE_ONLY ON)
set(MSDFGEN_USE_VCPKG OFF)
set(MSDFGEN_BUILD_STANDALONE OFF)
set(MSDFGEN_DISABLE_SVG ON)
set(MSDFGEN_DISABLE_PNG ON)
set(MSDFGEN_DYNAMIC_RUNTIME OFF)

# ======================================
# Assimp options
# ======================================
set(ASSIMP_INSTALL OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

# ======================================
# EnkiTS options
# ======================================
set(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "")
set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "")
set(ENKITS_BUILD_SHARED OFF CACHE BOOL "")
set(ENKITS_INSTALL OFF CACHE BOOL "")
set(ENKITS_SANITIZE ON CACHE BOOL "")

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

# ======================================
# External dependencies via FetchContent
# ======================================
include(FetchContent)

# fmt
fetchcontent_declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.0.2
)
fetchcontent_makeavailable(fmt)

# glfw
fetchcontent_declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
)
fetchcontent_makeavailable(glfw)

# ImGui (docking branch)
fetchcontent_declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
)
fetchcontent_makeavailable(imgui)

# ImGuizmo
fetchcontent_declare(ImGuizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG master
)
fetchcontent_makeavailable(ImGuizmo)

# toml++ (header-only)
fetchcontent_declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)
fetchcontent_makeavailable(tomlplusplus)

# cpptrace
fetchcontent_declare(cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG v0.7.3
)
fetchcontent_makeavailable(cpptrace)

# unordered_dense (header-only)
fetchcontent_declare(unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v4.4.0
)
fetchcontent_makeavailable(unordered_dense)

# EnTT
fetchcontent_declare(entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.0
)
fetchcontent_makeavailable(entt)

# nlohmann_json
fetchcontent_declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
fetchcontent_makeavailable(nlohmann_json)

# Miniaudio
fetchcontent_declare(miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.23
)
fetchcontent_makeavailable(miniaudio)


# SpirV-Reflect
fetchcontent_declare(spirv_reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG main
)
fetchcontent_makeavailable(spirv_reflect)

add_library(spirv_reflect_lib ${spirv_reflect_SOURCE_DIR}/spirv_reflect.c)
target_include_directories(spirv_reflect_lib PUBLIC ${spirv_reflect_SOURCE_DIR})

# volk
fetchcontent_declare(volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG 1.4.304
)
fetchcontent_makeavailable(volk)

# VulkanMemoryAllocator
fetchcontent_declare(VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.3.0
)
fetchcontent_makeavailable(VulkanMemoryAllocator)

# yaml-cpp
fetchcontent_declare(yaml_cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
)
fetchcontent_makeavailable(yaml_cpp)

# nativefiledialog-extended
fetchcontent_declare(nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG v1.1.1
)
fetchcontent_makeavailable(nfd)

# freetype
fetchcontent_declare(freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG VER-2-13-2
)
fetchcontent_makeavailable(freetype)

# JoltPhysics
fetchcontent_declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.3.0
        SOURCE_SUBDIR Build
)
fetchcontent_makeavailable(JoltPhysics)

# assimp
fetchcontent_declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
)
fetchcontent_makeavailable(assimp)

# msdfgen
fetchcontent_declare(msdfgen
        GIT_REPOSITORY https://github.com/Chlumsky/msdfgen.git
        GIT_TAG v1.12
)
fetchcontent_makeavailable(msdfgen)

# msdf-atlas-gen
fetchcontent_declare(msdf_atlas_gen
        GIT_REPOSITORY https://github.com/Chlumsky/msdf-atlas-gen.git
        GIT_TAG v1.3
)
fetchcontent_makeavailable(msdf_atlas_gen)

# spdlog
fetchcontent_declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0  # latest stable release
)
fetchcontent_makeavailable(spdlog)

# stb (header-only)
fetchcontent_declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master  # stable is usually master for stb
)
fetchcontent_makeavailable(stb)

# Tom plus plus
fetchcontent_declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)
fetchcontent_makeavailable(tomlplusplus)

# GLM
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
)

FetchContent_MakeAvailable(glm)

# Vulkan Memory Allocator (VMA)
FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG master
)

FetchContent_MakeAvailable(VulkanMemoryAllocator)

# Taskflow
FetchContent_Declare(
        taskflow
        GIT_REPOSITORY https://github.com/taskflow/taskflow.git
        GIT_TAG v3.7.0
)

FetchContent_MakeAvailable(taskflow)

# harfbuzz (not needed if you don't do complex text layout)
#FetchContent_Declare(
#        harfbuzz
#        GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
#        GIT_TAG main
#)
#FetchContent_MakeAvailable(harfbuzz)

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

#get_cmake_property(_variableNames VARIABLES)
#    foreach(_variableName ${_variableNames})
#        message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

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
)

if(UNIX)
    list(APPEND LIBRARIES tbb pthread m dl atomic)
endif()

# ======================================
# Create Mikoto library
# ======================================
add_library(${PROJECT_NAME} STATIC ${ALL_SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBRARIES})

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
)

# ======================================
# Compile definitions
# ======================================
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(${PROJECT_NAME} PRIVATE SPDLOG_FMT_EXTERNAL)
target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLFW_KEY_CODES)
target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLFW_MOUSE_CODES)
target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLFW_INPUT)

target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_RADIANS)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIKOTO_USE_AUDIO_ENGINE_INTERFACE)
