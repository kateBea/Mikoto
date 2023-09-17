cmake_minimum_required(VERSION 3.21)
set (PROJECT_NAME Mikoto)
project(${PROJECT_NAME})

# CXX Lang Requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Libraries
set(GLEW_CMAKE_DIR vendor/glew/build/cmake)
set(GLFW_CMAKE_DIR vendor/glfw)
set(ENTT_CMAKE_DIR vendor/entt)
set(FMT_CMAKE_DIR vendor/fmt)
set(VOLK_CMAKE_DIR vendor/volk)
set(ASSIMP_CMAKE_DIR vendor/assimp)
set(VMA_CMAKE_DIR vendor/VulkanMemoryAllocator)

# Projects source files
set(CORE_SOURCES
        src/Logger.cc
        src/LayerStack.cc
        src/ImGuiLayer.cc
        src/OpenGLContext.cc
        src/InputManager.cc src/EditorLayer.cc src/Timer.cc
        src/VulkanUtils.cc
        src/OpenGLUtils.cc
        src/GUIManager.cc
        src/LayerManager.cc
        src/JobSystem.cc)

set(PLATFORM_SOURCES
        src/Application.cc
        src/MainWindow.cc
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
        src/OpenGLRenderer.cc
        src/RendererAPI.cc
        src/OrthographicCamera.cc
        src/Texture2D.cc
        src/OpenGLTexture2D.cc
        src/RenderCommand.cc
        src/OrthographicCameraController.cc
        src/Shader.cc
        src/FrameBuffer.cc
        src/OpenGLFrameBuffer.cc
        src/Camera.cc
        src/VulkanContext.cc
        src/VulkanRenderer.cc
        src/Mesh.cc
        src/Model.cc
        src/Texture.cc
        src/Material.cc
        src/VulkanStandardMaterial.cc
        src/VulkanFrameBuffer.cc
        src/VulkanIndexBuffer.cc
        src/VulkanShader.cc
        src/VulkanCommandPool.cc
        src/VulkanTexture2D.cc
        src/VulkanDescriptorPool.cc
        src/VulkanPipeline.cc
        src/VulkanSwapChain.cc src/RenderContext.cc src/OpenGLDefaultMaterial.cc
        src/VulkanCommandBuffer.cc
        src/VulkanDescriptorSet.cc
        src/VulkanRenderPass.cc
        src/VulkanImage.cc
        src/VulkanImage.cc
        src/VulkanBuffer.cc)

set(IMGUI_SOURCES
        vendor/imgui/imgui.cpp
        vendor/imgui/backends/imgui_impl_glfw.cpp
        vendor/imgui/imgui_widgets.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/imgui_tables.cpp
        vendor/imgui/imgui_demo.cpp
        vendor/imgui/backends/imgui_impl_opengl3.cpp
        vendor/imgui/backends/imgui_impl_vulkan.cpp)

set(EDITOR_SOURCES
        src/Editor.cc src/HierarchyPanel.cc src/InspectorPanel.cc src/SettingsPanel.cc src/ScenePanel.cc src/StatsPanel.cc)

set(SCENE_SOURCES
        src/Scene.cc src/SceneCamera.cc
        src/EditorCamera.cc)

set(IMGUIZMO_SOURCES
        vendor/ImGuizmo/ImGuizmo.cpp
        vendor/ImGuizmo/GraphEditor.cpp
        vendor/ImGuizmo/ImCurveEdit.cpp
        vendor/ImGuizmo/ImGradient.cpp
        vendor/ImGuizmo/ImSequencer.cpp)

set(SOURCES
        ${CORE_SOURCES}
        ${PLATFORM_SOURCES}
        ${IMGUI_SOURCES}
        ${RENDERER_SOURCES}
        ${EDITOR_SOURCES}
        ${SCENE_SOURCES}
        ${IMGUIZMO_SOURCES}
        src/Engine.cc src/stb_image.cc
        src/vk_mem_alloc.h.cc
        src/GUIManager.cc
        src/ImGuiVulkanBackend.cc
        src/ImGuiOpenGLBackend.cc)

# External dependencies
add_subdirectory(${GLEW_CMAKE_DIR})
add_subdirectory(${GLFW_CMAKE_DIR})
add_subdirectory(${FMT_CMAKE_DIR})
add_subdirectory(${ENTT_CMAKE_DIR})
add_subdirectory(${VOLK_CMAKE_DIR})
add_subdirectory(${ASSIMP_CMAKE_DIR})

# Libraries to link against target
set(LIBRARIES glfw glew_s fmt EnTT::EnTT volk assimp)

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
include_directories(vendor/spdlog/include)
include_directories(vendor/glfw/include)
include_directories(vendor/glew/include)
include_directories(vendor/imgui)
include_directories(vendor/imgui/backends)
include_directories(vendor/glm/)
include_directories(vendor/stb/)
include_directories(vendor/entt/single_include)
include_directories(vendor/volk)
include_directories(vendor/assimp/include)
include_directories(vendor/VulkanMemoryAllocator/include)
include_directories(vendor/ImGuizmo)

add_executable(${PROJECT_NAME} src/EntryPoint.cc ${SOURCES})
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
