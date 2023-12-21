cmake_minimum_required(VERSION 3.18)

set (PROJECT_NAME Mikoto)
project(${PROJECT_NAME})

# CXX Lang Requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Silence warnings on cmake 3.11+
set(OpenGL_GL_PREFERENCE LEGACY)

# Libraries
set(GLEW_CMAKE_DIR Third-Party/glew/build/cmake)
set(GLFW_CMAKE_DIR Third-Party/glfw)
set(ENTT_CMAKE_DIR Third-Party/entt)
set(FMT_CMAKE_DIR Third-Party/fmt)
set(VOLK_CMAKE_DIR Third-Party/volk)
set(ASSIMP_CMAKE_DIR Third-Party/assimp)
set(VMA_CMAKE_DIR Third-Party/VulkanMemoryAllocator)
set(YAML_CMAKE_DIR Third-Party/yaml-cpp)
set(NFD_CMAKE_DIR Third-Party/nativefiledialog-extended)
set(JSON_CMAKE_DIR Third-Party/json)

# Projects source files
set(CORE_SOURCES
        Mikoto-Engine/src/Logger.cc
        Mikoto-Engine/src/ImGuiManager.cc
        Mikoto-Engine/src/OpenGLContext.cc
        Mikoto-Engine/src/InputManager.cc Mikoto-Editor/src/EditorLayer.cc Mikoto-Engine/src/Timer.cc
        Mikoto-Engine/src/VulkanUtils.cc
        Mikoto-Engine/src/OpenGLUtils.cc
        Mikoto-Engine/src/Serializer.cc
        Mikoto-Runtime/src/GameLayer.cc
        Mikoto-Runtime/GameLayer.hh
        Mikoto-Engine/src/AssetsManager.cc
)

set(PLATFORM_SOURCES
        Mikoto-Engine/src/Application.cc
        Mikoto-Engine/src/GlfwWindow.cc
        )

set(RENDERER_SOURCES
        Mikoto-Engine/src/VulkanVertexBuffer.cc
        Mikoto-Engine/src/OpenGLVertexArray.cc
        Mikoto-Engine/src/OpenGLVertexBuffer.cc
        Mikoto-Engine/src/OpenGLIndexBuffer.cc
        Mikoto-Engine/src/Renderer.cc
        Mikoto-Engine/src/VertexBuffer.cc
        Mikoto-Engine/src/IndexBuffer.cc
        Mikoto-Engine/src/OpenGLShader.cc
        Mikoto-Engine/src/OpenGLRenderer.cc
        Mikoto-Engine/src/RendererAPI.cc
        Mikoto-Engine/src/OrthographicCamera.cc
        Mikoto-Engine/src/Texture2D.cc
        Mikoto-Engine/src/OpenGLTexture2D.cc
        Mikoto-Engine/src/RenderCommand.cc
        Mikoto-Engine/src/Shader.cc
        Mikoto-Engine/src/FrameBuffer.cc
        Mikoto-Engine/src/OpenGLFrameBuffer.cc
        Mikoto-Engine/src/VulkanContext.cc
        Mikoto-Engine/src/VulkanRenderer.cc
        Mikoto-Engine/src/Mesh.cc
        Mikoto-Engine/src/Model.cc
        Mikoto-Engine/src/Material.cc
        Mikoto-Engine/src/VulkanStandardMaterial.cc
        Mikoto-Engine/src/VulkanFrameBuffer.cc
        Mikoto-Engine/src/VulkanIndexBuffer.cc
        Mikoto-Engine/src/VulkanShader.cc
        Mikoto-Engine/src/VulkanCommandPool.cc
        Mikoto-Engine/src/VulkanTexture2D.cc
        Mikoto-Engine/src/VulkanPipeline.cc
        Mikoto-Engine/src/VulkanSwapChain.cc Mikoto-Engine/src/RenderContext.cc Mikoto-Engine/src/OpenGLDefaultMaterial.cc
        Mikoto-Engine/src/VulkanImage.cc
        Mikoto-Engine/src/VulkanImage.cc
        Mikoto-Engine/src/VulkanBuffer.cc
        Mikoto-Engine/src/DescriptorManager.cc
        Mikoto-Engine/src/PhysicallyBasedMaterial.cc
        Mikoto-Engine/src/VulkanPBRMaterial.cc)

set(IMGUI_SOURCES
        Third-Party/imgui/imgui.cpp
        Third-Party/imgui/backends/imgui_impl_glfw.cpp
        Third-Party/imgui/imgui_widgets.cpp
        Third-Party/imgui/imgui_draw.cpp
        Third-Party/imgui/imgui_tables.cpp
        Third-Party/imgui/imgui_demo.cpp
        Third-Party/imgui/backends/imgui_impl_opengl3.cpp
        Third-Party/imgui/backends/imgui_impl_vulkan.cpp)

set(EDITOR_SOURCES
        Mikoto-Editor/src/Editor.cc Mikoto-Editor/src/HierarchyPanel.cc Mikoto-Editor/src/InspectorPanel.cc Mikoto-Editor/src/SettingsPanel.cc Mikoto-Editor/src/ScenePanel.cc Mikoto-Editor/src/StatsPanel.cc
        Mikoto-Editor/src/ConsolePanel.cc
        Mikoto-Editor/src/ConsoleManager.cc
        Mikoto-Engine/src/RendererPanel.cc
        Mikoto-Editor/src/AssetsManagerPanel.cc
        Mikoto-Editor/src/MaterialEditorPanel.cc)

set(SCENE_SOURCES
        Mikoto-Engine/src/Scene.cc Mikoto-Engine/src/SceneCamera.cc
        Mikoto-Editor/src/EditorCamera.cc
        Mikoto-Engine/src/SceneManager.cc
        Mikoto-Engine/Scene/SceneManager.hh)

set(IMGUIZMO_SOURCES
        Third-Party/ImGuizmo/ImGuizmo.cpp
        Third-Party/ImGuizmo/GraphEditor.cpp
        Third-Party/ImGuizmo/ImCurveEdit.cpp
        Third-Party/ImGuizmo/ImGradient.cpp
        Third-Party/ImGuizmo/ImSequencer.cpp)

set(SOURCES
        ${CORE_SOURCES}
        ${PLATFORM_SOURCES}
        ${IMGUI_SOURCES}
        ${RENDERER_SOURCES}
        ${EDITOR_SOURCES}
        ${SCENE_SOURCES}
        ${IMGUIZMO_SOURCES}
        Mikoto-Engine/src/Engine.cc Mikoto-Engine/src/stb_image.cc
        Mikoto-Engine/src/vk_mem_alloc.h.cc
        Mikoto-Engine/src/ImGuiVulkanBackend.cc
        Mikoto-Engine/src/ImGuiOpenGLBackend.cc
        Mikoto-Engine/src/EventManager.cc
        Mikoto-Editor/src/ContentBrowserPanel.cc
        Mikoto-Editor/Panels/ContentBrowserPanel.hh
        Mikoto-Runtime/src/Sandbox.cc)

# External dependencies
add_subdirectory(${GLEW_CMAKE_DIR})
add_subdirectory(${GLFW_CMAKE_DIR})
add_subdirectory(${FMT_CMAKE_DIR})
add_subdirectory(${ENTT_CMAKE_DIR})
add_subdirectory(${VOLK_CMAKE_DIR})
add_subdirectory(${ASSIMP_CMAKE_DIR})
add_subdirectory(${YAML_CMAKE_DIR})
add_subdirectory(${NFD_CMAKE_DIR})
add_subdirectory(${JSON_CMAKE_DIR})

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
include_directories(engine/)

include_directories(Mikoto-Editor)
include_directories(Mikoto-Engine)
include_directories(Mikoto-Runtime)

include_directories(Third-Party/spdlog/include)
include_directories(Third-Party/glfw/include)
include_directories(Third-Party/glew/include)
include_directories(Third-Party/imgui)
include_directories(Third-Party/imgui/backends)
include_directories(Third-Party/glm/)
include_directories(Third-Party/stb/)
include_directories(Third-Party/entt/single_include)
include_directories(Third-Party/volk)
include_directories(Third-Party/assimp/include)
include_directories(Third-Party/VulkanMemoryAllocator/include)
include_directories(Third-Party/ImGuizmo)
include_directories(Third-Party/yaml-cpp/include)
include_directories(Third-Party/nativefiledialog-extended/src/include)

add_executable(${PROJECT_NAME} Mikoto-Engine/src/EntryPoint.cc ${SOURCES})
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
