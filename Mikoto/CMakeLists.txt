CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

PROJECT(Mikoto)

# CXX Lang Requirements
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW CMake Options
SET(GLFW_INSTALL OFF CACHE BOOL "")
SET(GLFW_BUILD_DOCS OFF CACHE BOOL "")
SET(GLFW_BUILD_TESTS OFF CACHE BOOL "")
SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
SET(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
SET(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
SET(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# MSDFGEN CMake Options
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

# Assimp CMake Options
SET(ASSIMP_INSTALL OFF CACHE BOOL "")
SET(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
SET(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Treat Assimp Warnings as errors" FORCE)

# EnkiTS CMake Options
SET(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "")
SET(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "")
SET(ENKITS_BUILD_SHARED OFF CACHE BOOL "")
SET(ENKITS_INSTALL OFF CACHE BOOL "")
SET(ENKITS_SANITIZE ON CACHE BOOL "")

# Setup the root directory where the dependencies are located
# All required Dependencies are located in the root folder
# This CmakeList file however is located in Mikoto (the core engine) folder
SET(CURRENT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
CMAKE_PATH(GET CURRENT_PATH PARENT_PATH MIKOTO_ROOT_DIRECTORY)

# External dependencies
SET(GLFW_CMAKE_DIR      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/glfw)
SET(ENTT_CMAKE_DIR      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/entt)
SET(FMT_CMAKE_DIR       ${MIKOTO_ROOT_DIRECTORY}/Third-Party/fmt)
SET(VOLK_CMAKE_DIR      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/volk)
SET(ASSIMP_CMAKE_DIR    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/assimp)
SET(VMA_CMAKE_DIR       ${MIKOTO_ROOT_DIRECTORY}/Third-Party/VulkanMemoryAllocator)
SET(YAML_CMAKE_DIR      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/yaml-cpp)
SET(NFD_CMAKE_DIR       ${MIKOTO_ROOT_DIRECTORY}/Third-Party/nativefiledialog-extended)
SET(TOML_CPP            ${MIKOTO_ROOT_DIRECTORY}/Third-Party/tomlplusplus)
SET(CPP_TRACE           ${MIKOTO_ROOT_DIRECTORY}/Third-Party/cpptrace)
SET(FREE_TYPE           ${MIKOTO_ROOT_DIRECTORY}/Third-Party/freetype)
SET(JOLT_PHYSICS        ${MIKOTO_ROOT_DIRECTORY}/Third-Party/JoltPhysics/Build)
SET(MSDFGEN             ${MIKOTO_ROOT_DIRECTORY}/Third-Party/msdfgen)
SET(MSDF_ATLAS_GEN      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/msdf-atlas-gen)
SET(SPIRV_REFLECT      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/spirv-reflect)
SET(ENKI_TS      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/enkiTS)
SET(MINIAUDIO      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/miniaudio)
SET(UNORDERED_DENSE      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/unordered_dense)
SET(JSON      ${MIKOTO_ROOT_DIRECTORY}/Third-Party/json)

# Setup Mikoto Engine source file
FILE(GLOB SOURCE_FILES Sources/*.cc)

# Setup third party source files

# Imgui and Imgui backend
SET(IMGUI_SOURCES
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/imgui.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/imgui_draw.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/imgui_demo.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/imgui_tables.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/imgui_widgets.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/backends/imgui_impl_glfw.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/backends/imgui_impl_vulkan.cpp)

# ImGui - ImGuizmo
SET(IMGUIZMO_SOURCES
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/ImGuizmo/ImGuizmo.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/ImGuizmo/GraphEditor.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/ImGuizmo/ImCurveEdit.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/ImGuizmo/ImGradient.cpp
    ${MIKOTO_ROOT_DIRECTORY}/Third-Party/ImGuizmo/ImSequencer.cpp)

# SpirV Reflect
SET(SPIRV_REFLECT_SOURCES
        ${MIKOTO_ROOT_DIRECTORY}/Third-Party/spirv-reflect/spirv_reflect.c)

# SpirV Reflect
SET(MINIAUDIO
        ${MIKOTO_ROOT_DIRECTORY}/Third-Party/miniaudio/miniaudio.c)

# Mikoto compile source files
SET(SOURCES ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES} ${SOURCE_FILES} ${SPIRV_REFLECT_SOURCES}
        ${MINIAUDIO})

# External dependencies
ADD_SUBDIRECTORY(${GLFW_CMAKE_DIR} build/glfw)
ADD_SUBDIRECTORY(${FMT_CMAKE_DIR} build/fmt)
ADD_SUBDIRECTORY(${ENTT_CMAKE_DIR} build/entt)
ADD_SUBDIRECTORY(${VOLK_CMAKE_DIR} build/volk)
ADD_SUBDIRECTORY(${ASSIMP_CMAKE_DIR} build/assimp)
ADD_SUBDIRECTORY(${YAML_CMAKE_DIR} build/yaml)
ADD_SUBDIRECTORY(${NFD_CMAKE_DIR} build/nfd)
ADD_SUBDIRECTORY(${CPP_TRACE} build/cpptrace)
ADD_SUBDIRECTORY(${FREE_TYPE} build/freetype)
ADD_SUBDIRECTORY(${JOLT_PHYSICS} build/joltphysics)
ADD_SUBDIRECTORY(${MSDFGEN} build/msdfgen)
ADD_SUBDIRECTORY(${MSDF_ATLAS_GEN} build/msdf_atlas_gen)
ADD_SUBDIRECTORY(${ENKI_TS} build/enkiTS)
ADD_SUBDIRECTORY(${UNORDERED_DENSE} build/unordered_dense)
ADD_SUBDIRECTORY(${JSON} build/json)

# Todo pull other libs wih fetchcontent
#include(FetchContent)
#
#FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
#FetchContent_MakeAvailable(json)
#
#target_link_libraries(foo PRIVATE nlohmann_json::nlohmann_json)

# Libraries to link against target
SET(LIBRARIES
    glfw
    fmt
    EnTT::EnTT
    volk
    assimp
    yaml-cpp
    nfd
    cpptrace::cpptrace
    freetype
    Jolt
    msdfgen::msdfgen
    msdf-atlas-gen::msdf-atlas-gen
    enkiTS
    unordered_dense::unordered_dense
        nlohmann_json::nlohmann_json
)

# Required pthread and lm for linux
# https://github.com/mackron/miniaudio for details on potential required libraries
IF (UNIX)
    SET(LIBRARIES ${LIBRARIES} tbb pthread m dl atomic)
ENDIF ()

# Create the library
ADD_LIBRARY(${PROJECT_NAME} STATIC ${SOURCES})

# Link libraries
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${LIBRARIES})

# Specify target headers
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC  .)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/cpptrace/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/spdlog/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/spdlog/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/glfw/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/glew/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/imgui/backends)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/glm/)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/stb/)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/entt/single_include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/volk)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/fmt/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/assimp/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/VulkanMemoryAllocator/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/ImGuizmo)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/yaml-cpp/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/nativefiledialog-extended/src/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/tomlplusplus/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/JoltPhysics/Jolt)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/freetype/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/msdfgen)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/msdf-atlas-gen)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/spirv-reflect)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/enkiTS/src)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/miniaudio)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/concurrentqueue)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/unordered_dense/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${MIKOTO_ROOT_DIRECTORY}/Third-Party/json/include)

# Avoid GLFW including OpenGL headers
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

# Needed if we want to use FMT as an external library and not the
# one bundled with SPDLOG
# For more: https://github.com/gabime/spdlog/wiki/0.-FAQ
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE SPDLOG_FMT_EXTERNAL)

# For Key and Mouse Codes we use the GLFW ones
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE USE_GLFW_INPUT)

# Avoid symbols conflict when using Volk
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)

# The perspective projection matrix generated by GLM will use the OpenGL depth
# range of -1.0 to 1.0 by default. We need to configure it to use the Vulkan
# range of 0.0 to 1.0 using the GLM_FORCE_DEPTH_ZERO_TO_ONE definition.
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLM_FORCE_RADIANS)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)

TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)


# If you fetch pointers to all Vulkan functions in a custom way, e.g. using some loader like Volk:
# Define VMA_STATIC_VULKAN_FUNCTIONS and VMA_DYNAMIC_VULKAN_FUNCTIONS to 0.
# Pass these pointers via structure VmaVulkanFunctions.
# // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html#quick_start_project_setup
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE VMA_STATIC_VULKAN_FUNCTIONS=0)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE VMA_DYNAMIC_VULKAN_FUNCTIONS=0)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE MIKOTO_USE_AUDIO_ENGINE_INTERFACE)
