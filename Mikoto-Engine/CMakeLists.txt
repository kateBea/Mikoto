CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

PROJECT(MikotoEngine)

# CXX Lang Requirements
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# Silence warnings on cmake 3.11+
SET(OpenGL_GL_PREFERENCE LEGACY)

# Libraries
SET( CURRENT_PATH ${CMAKE_CURRENT_SOURCE_DIR} )
CMAKE_PATH(GET CURRENT_PATH PARENT_PATH P_PATH)

SET(GLEW_CMAKE_DIR ${P_PATH}/Third-Party/glew/build/cmake)
SET(GLFW_CMAKE_DIR ${P_PATH}/Third-Party/glfw)
SET(ENTT_CMAKE_DIR ${P_PATH}/Third-Party/entt)
SET(FMT_CMAKE_DIR ${P_PATH}/Third-Party/fmt)
SET(VOLK_CMAKE_DIR ${P_PATH}/Third-Party/volk)
SET(ASSIMP_CMAKE_DIR ${P_PATH}/Third-Party/assimp)
SET(VMA_CMAKE_DIR ${P_PATH}/Third-Party/VulkanMemoryAllocator)
SET(YAML_CMAKE_DIR ${P_PATH}/Third-Party/yaml-cpp)
SET(NFD_CMAKE_DIR ${P_PATH}/Third-Party/nativefiledialog-extended)
SET(JSON_CMAKE_DIR ${P_PATH}/Third-Party/json)

# Setup Mikoto Engine source file
FILE(GLOB_RECURSE SOURCE_FILES  "./src/*.cc")
SOURCE_GROUP(MIKOTO_SOURCES FILES ${SOURCE_FILES})

# Setup third party source files
SET(IMGUI_SOURCES
    ${P_PATH}/Third-Party/imgui/imgui.cpp
    ${P_PATH}/Third-Party/imgui/imgui_draw.cpp
    ${P_PATH}/Third-Party/imgui/imgui_demo.cpp
    ${P_PATH}/Third-Party/imgui/imgui_tables.cpp
    ${P_PATH}/Third-Party/imgui/imgui_widgets.cpp
    ${P_PATH}/Third-Party/imgui/backends/imgui_impl_glfw.cpp
    ${P_PATH}/Third-Party/imgui/backends/imgui_impl_vulkan.cpp
    ${P_PATH}/Third-Party/imgui/backends/imgui_impl_opengl3.cpp)

SET(IMGUIZMO_SOURCES
    ${P_PATH}/Third-Party/ImGuizmo/ImGuizmo.cpp
    ${P_PATH}/Third-Party/ImGuizmo/GraphEditor.cpp
    ${P_PATH}/Third-Party/ImGuizmo/ImCurveEdit.cpp
    ${P_PATH}/Third-Party/ImGuizmo/ImGradient.cpp
    ${P_PATH}/Third-Party/ImGuizmo/ImSequencer.cpp)

SET(SOURCES ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES})

# External dependencies
add_subdirectory(${GLEW_CMAKE_DIR} ./build/glew)
add_subdirectory(${GLFW_CMAKE_DIR} ./build/glfw)
add_subdirectory(${FMT_CMAKE_DIR} ./build/fmt)
add_subdirectory(${ENTT_CMAKE_DIR} ./build/entt)
add_subdirectory(${VOLK_CMAKE_DIR} ./build/volk)
add_subdirectory(${ASSIMP_CMAKE_DIR} ./build/assimp)
add_subdirectory(${YAML_CMAKE_DIR} ./build/yaml)
add_subdirectory(${NFD_CMAKE_DIR} ./build/nfd)
add_subdirectory(${JSON_CMAKE_DIR} ./build/json)

# Libraries to link against target
set(LIBRARIES glfw glew_s fmt EnTT::EnTT volk assimp yaml-cpp nfd nlohmann_json::nlohmann_json)

# for tests only. -ltbb was required to use std::execution::par on linux
# set(LIBRARIES glfw glew_s fmt EnTT::EnTT volk assimp yaml-cpp nfd tbb)

# Compiler specific flags
if (WIN32)
    # Compile commands for Windows
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(LINUX)
    # Compile commands for Linux
    add_compile_options(-fsanitize=address -D_DEBUG -Wall -Wextra -Wpedantic)

    # Sanitize
    add_link_options(-fsanitize=address)
endif()

# Header files directories

ADD_LIBRARY(${PROJECT_NAME} STATIC ${SOURCES})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${LIBRARIES})

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC  .)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/spdlog/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/glfw/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/glew/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/imgui)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/imgui/backends)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/glm/)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/stb/)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/entt/single_include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/volk)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/fmt/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/assimp/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/VulkanMemoryAllocator/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/ImGuizmo)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/yaml-cpp/include)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/nativefiledialog-extended/src/include)

# Avoid GLFW including OpenGL headers
target_compile_definitions(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

# Needed if we want to use FMT as an external library and not the
# one bundled with SPDLOG
# For more: https://github.com/gabime/spdlog/wiki/0.-FAQ
target_compile_definitions(${PROJECT_NAME} PRIVATE SPDLOG_FMT_EXTERNAL)

# For Key and Mouse Codes we use the GLFW ones
target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLFW_KEY_CODES)
target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLFW_MOUSE_CODES)
target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLFW_INPUT)

target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)

# The perspective projection matrix generated by GLM will use the OpenGL depth
# range of -1.0 to 1.0 by default. We need to configure it to use the Vulkan
# range of 0.0 to 1.0 using the GLM_FORCE_DEPTH_ZERO_TO_ONE definition.
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_FORCE_RADIANS)
target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)

# If you fetch pointers to all Vulkan functions in a custom way, e.g. using some loader like Volk:
# Define VMA_STATIC_VULKAN_FUNCTIONS and VMA_DYNAMIC_VULKAN_FUNCTIONS to 0.
# Pass these pointers via structure VmaVulkanFunctions.
# // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html#quick_start_project_setup
target_compile_definitions(${PROJECT_NAME} PRIVATE VMA_STATIC_VULKAN_FUNCTIONS=0)
target_compile_definitions(${PROJECT_NAME} PRIVATE VMA_DYNAMIC_VULKAN_FUNCTIONS=0)
