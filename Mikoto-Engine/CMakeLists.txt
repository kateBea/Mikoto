cmake_minimum_required(VERSION 3.18)

set (PROJECT_NAME MikotoEngine)
project(${PROJECT_NAME})

# CXX Lang Requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Silence warnings on cmake 3.11+
set(OpenGL_GL_PREFERENCE LEGACY)

# Libraries

set( cur_path ${CMAKE_CURRENT_SOURCE_DIR} )
cmake_path(GET cur_path PARENT_PATH parent_path)

set(GLEW_CMAKE_DIR ${parent_path}/Third-Party/glew/build/cmake)
set(GLFW_CMAKE_DIR ${parent_path}/Third-Party/glfw)
set(ENTT_CMAKE_DIR ${parent_path}/Third-Party/entt)
set(FMT_CMAKE_DIR ${parent_path}/Third-Party/fmt)
set(VOLK_CMAKE_DIR ${parent_path}/Third-Party/volk)
set(ASSIMP_CMAKE_DIR ${parent_path}/Third-Party/assimp)
set(VMA_CMAKE_DIR ${parent_path}/Third-Party/VulkanMemoryAllocator)
set(YAML_CMAKE_DIR ${parent_path}/Third-Party/yaml-cpp)
set(NFD_CMAKE_DIR ${parent_path}/Third-Party/nativefiledialog-extended)
set(JSON_CMAKE_DIR ${parent_path}/Third-Party/json)

set(CORE_SOURCES
        src/Logger.cc
        src/ImGuiManager.cc
        src/OpenGLContext.cc
        src/InputManager.cc
        src/Timer.cc
        src/VulkanUtils.cc
        src/OpenGLUtils.cc
        src/Serializer.cc
        src/AssetsManager.cc
        src/Window.cc
)

set(PLATFORM_SOURCES
        src/XPWindow.cc
)

set(RENDERER_SOURCES
        src/VulkanVertexBuffer.cc
        src/OpenGLVertexArray.cc
        src/OpenGLVertexBuffer.cc
        src/OpenGLIndexBuffer.cc
        src/Renderer.cc
        src/VertexBuffer.cc
        src/IndexBuffer.cc
        src/OpenGLShader.cc
        src/ConsoleManager.cc
        src/OpenGLRenderer.cc
        src/RendererAPI.cc
        src/OrthographicCamera.cc
        src/Texture2D.cc
        src/OpenGLTexture2D.cc
        src/RenderCommand.cc
        src/Shader.cc
        src/FrameBuffer.cc
        src/OpenGLFrameBuffer.cc
        src/VulkanContext.cc
        src/VulkanRenderer.cc
        src/Mesh.cc
        src/Model.cc
        src/Material.cc
        src/VulkanStandardMaterial.cc
        src/VulkanFrameBuffer.cc
        src/VulkanIndexBuffer.cc
        src/VulkanShader.cc
        src/VulkanCommandPool.cc
        src/VulkanTexture2D.cc
        src/VulkanPipeline.cc
        src/VulkanSwapChain.cc
        src/RenderContext.cc
        src/OpenGLDefaultMaterial.cc
        src/VulkanImage.cc
        src/VulkanImage.cc
        src/VulkanBuffer.cc
        src/DescriptorManager.cc
        src/PhysicallyBasedMaterial.cc
        src/VulkanPBRMaterial.cc
)

set(IMGUI_SOURCES
        ${parent_path}/Third-Party/imgui/imgui.cpp
        ${parent_path}/Third-Party/imgui/backends/imgui_impl_glfw.cpp
        ${parent_path}/Third-Party/imgui/imgui_widgets.cpp
        ${parent_path}/Third-Party/imgui/imgui_draw.cpp
        ${parent_path}/Third-Party/imgui/imgui_tables.cpp
        ${parent_path}/Third-Party/imgui/imgui_demo.cpp
        ${parent_path}/Third-Party/imgui/backends/imgui_impl_opengl3.cpp
        ${parent_path}/Third-Party/imgui/backends/imgui_impl_vulkan.cpp
)

set(SCENE_SOURCES
        src/Scene.cc
        src/SceneCamera.cc
        src/EditorCamera.cc
        src/SceneManager.cc
)

set(IMGUIZMO_SOURCES
        ${parent_path}/Third-Party/ImGuizmo/ImGuizmo.cpp
        ${parent_path}/Third-Party/ImGuizmo/GraphEditor.cpp
        ${parent_path}/Third-Party/ImGuizmo/ImCurveEdit.cpp
        ${parent_path}/Third-Party/ImGuizmo/ImGradient.cpp
        ${parent_path}/Third-Party/ImGuizmo/ImSequencer.cpp)

set(SOURCES
        ${CORE_SOURCES}
        ${PLATFORM_SOURCES}
        ${IMGUI_SOURCES}
        ${RENDERER_SOURCES}
        ${EDITOR_SOURCES}
        ${SCENE_SOURCES}
        ${IMGUIZMO_SOURCES}
        src/stb_image.cc
        src/vk_mem_alloc.h.cc
        src/ImGuiVulkanBackend.cc
        src/ImGuiOpenGLBackend.cc
        src/EventManager.cc
)

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

# Compiler flags
if (MSVC)
    # Compile commands for Windows

else()
    # Compile commands for Linux
    add_compile_options(-fsanitize=address -D_DEBUG -Wall -Wextra -Wpedantic)
    # Sanitize
    add_link_options(-fsanitize=address)
endif()

# Header files directories
include_directories(.)

include_directories(${parent_path}/Third-Party/spdlog/include)
include_directories(${parent_path}/Third-Party/glfw/include)
include_directories(${parent_path}/Third-Party/glew/include)
include_directories(${parent_path}/Third-Party/imgui)
include_directories(${parent_path}/Third-Party/imgui/backends)
include_directories(${parent_path}/Third-Party/glm/)
include_directories(${parent_path}/Third-Party/stb/)
include_directories(${parent_path}/Third-Party/entt/single_include)
include_directories(${parent_path}/Third-Party/volk)
include_directories(${parent_path}/Third-Party/fmt/include)
include_directories(${parent_path}/Third-Party/assimp/include)
include_directories(${parent_path}/Third-Party/VulkanMemoryAllocator/include)
include_directories(${parent_path}/Third-Party/ImGuizmo)
include_directories(${parent_path}/Third-Party/yaml-cpp/include)
include_directories(${parent_path}/Third-Party/nativefiledialog-extended/src/include)

add_library(${PROJECT_NAME} STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})

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
