CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

PROJECT(Mikoto)

# CXX Lang Requirements
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# Silence warnings on cmake 3.11+
SET(OpenGL_GL_PREFERENCE LEGACY)

SET(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Treat Assimp Warnings as errors" FORCE)

# Libraries
SET( CURRENT_PATH ${CMAKE_CURRENT_SOURCE_DIR} )
CMAKE_PATH(GET CURRENT_PATH PARENT_PATH P_PATH)

SET(GLFW_CMAKE_DIR ${P_PATH}/Third-Party/glfw)
SET(ENTT_CMAKE_DIR ${P_PATH}/Third-Party/entt)
SET(FMT_CMAKE_DIR ${P_PATH}/Third-Party/fmt)
SET(VOLK_CMAKE_DIR ${P_PATH}/Third-Party/volk)
SET(ASSIMP_CMAKE_DIR ${P_PATH}/Third-Party/assimp)
SET(VMA_CMAKE_DIR ${P_PATH}/Third-Party/VulkanMemoryAllocator)
SET(YAML_CMAKE_DIR ${P_PATH}/Third-Party/yaml-cpp)
SET(NFD_CMAKE_DIR ${P_PATH}/Third-Party/nativefiledialog-extended)
SET(TOML_CPP ${P_PATH}/Third-Party/tomlplusplus)

# Setup Mikoto Engine source file
FILE(GLOB SOURCE_FILES src/*.cc)

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

SET(SOURCES ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES} ${SOURCE_FILES}
        Models/SystemData.hh
        Models/RenderContextData.hh
        Models/RendererDrawData.hh
        Models/MeshData.hh
        Models/ModelLoadInfo.hh
        Models/ApplicationData.hh
        Models/ShaderCreateInfo.hh
        Models/WindowProperties.hh
        Models/BufferLayout.hh
        Models/BufferElement.hh
        Models/VertexBufferCreateInfo.hh
        Models/StandardMaterialCreateData.hh
        Common/ConfigLoader.hh
        Models/ConfigOptions.hh
        Models/RenderInfo.hh
)

# External dependencies
ADD_SUBDIRECTORY(${GLFW_CMAKE_DIR} build/glfw)
ADD_SUBDIRECTORY(${FMT_CMAKE_DIR} build/fmt)
ADD_SUBDIRECTORY(${ENTT_CMAKE_DIR} build/entt)
ADD_SUBDIRECTORY(${VOLK_CMAKE_DIR} build/volk)
ADD_SUBDIRECTORY(${ASSIMP_CMAKE_DIR} build/assimp)
ADD_SUBDIRECTORY(${YAML_CMAKE_DIR} build/yaml)
ADD_SUBDIRECTORY(${NFD_CMAKE_DIR} build/nfd)

# Libraries to link against target
SET(LIBRARIES glfw fmt EnTT::EnTT volk assimp yaml-cpp nfd)

# for tests only. -ltbb was required to use std::execution::par on linux
# set(LIBRARIES glfw glew_s fmt EnTT::EnTT volk assimp yaml-cpp nfd tbb)

# Create the library
ADD_LIBRARY(${PROJECT_NAME} STATIC ${SOURCES})

# Link libraries
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${LIBRARIES})

# Specify target headers
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
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${P_PATH}/Third-Party/tomlplusplus/include)

# Avoid GLFW including OpenGL headers
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLFW_INCLUDE_NONE)

# Needed if we want to use FMT as an external library and not the
# one bundled with SPDLOG
# For more: https://github.com/gabime/spdlog/wiki/0.-FAQ
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE SPDLOG_FMT_EXTERNAL)

# For Key and Mouse Codes we use the GLFW ones
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE USE_GLFW_KEY_CODES)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE USE_GLFW_MOUSE_CODES)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE USE_GLFW_INPUT)

TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)

# The perspective projection matrix generated by GLM will use the OpenGL depth
# range of -1.0 to 1.0 by default. We need to configure it to use the Vulkan
# range of 0.0 to 1.0 using the GLM_FORCE_DEPTH_ZERO_TO_ONE definition.
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLM_FORCE_RADIANS)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)

# If you fetch pointers to all Vulkan functions in a custom way, e.g. using some loader like Volk:
# Define VMA_STATIC_VULKAN_FUNCTIONS and VMA_DYNAMIC_VULKAN_FUNCTIONS to 0.
# Pass these pointers via structure VmaVulkanFunctions.
# // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/quick_start.html#quick_start_project_setup
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE VMA_STATIC_VULKAN_FUNCTIONS=0)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE VMA_DYNAMIC_VULKAN_FUNCTIONS=0)
