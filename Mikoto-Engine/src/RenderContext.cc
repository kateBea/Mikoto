/**
 * RenderContext.cc
 * Created by kate on 7/19/2023.
 * */

// Project Headers
#include <Common/RenderingUtils.hh>

#include <Core/EventManager.hh>

#include <Renderer/RenderContext.hh>
#include <Renderer/OpenGL/OpenGLContext.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {
    auto RenderContext::Init(RenderContextSpec&& spec) -> void {
        s_Spec = std::move(spec);

        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                OpenGLContext::Init(s_Spec.WindowHandle);
                break;
            case GraphicsAPI::VULKAN_API:
                VulkanContext::Init(s_Spec.WindowHandle);
                break;
        }

        SetEventHandles();
    }

    auto RenderContext::Shutdown() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                OpenGLContext::ShutDown();
                break;
            case GraphicsAPI::VULKAN_API:
                // Wait for remaining operations
                VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

                // Delete remaining vulkan objects
                DeletionQueue::Flush();

                // Shut down the context
                VulkanContext::ShutDown();
                break;
        }
    }

    auto RenderContext::Present() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                OpenGLContext::Present();
                break;
            case GraphicsAPI::VULKAN_API:
                VulkanContext::Present();
                break;
        }
    }

    MKT_UNUSED_FUNC auto RenderContext::IsVSyncActive() -> bool {
        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                return OpenGLContext::IsVSyncActive();
            case GraphicsAPI::VULKAN_API:
                return VulkanContext::IsVSyncActive();
            default: return false;
        }
    }

    auto RenderContext::EnableVSync() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                OpenGLContext::EnableVSync();
                break;
            case GraphicsAPI::VULKAN_API:
                VulkanContext::EnableVSync();
                break;
        }
    }

    auto RenderContext::DisableVSync() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                OpenGLContext::DisableVSync();
                break;
            case GraphicsAPI::VULKAN_API:
                VulkanContext::DisableVSync();
                break;
        }
    }

    auto RenderContext::PrepareFrame() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::PrepareFrame();
                break;
        }
    }

    auto RenderContext::SubmitFrame() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::SubmitFrame();
                break;
        }
    }

    auto RenderContext::SetEventHandles() -> void {
        EventManager::Subscribe(s_Guid.Get(),
            EventType::WINDOW_RESIZE_EVENT,
            [](Event&) -> bool {
                MKT_APP_LOGGER_INFO("Handled Window Resize Event for RenderContext");
                return false;
            });
    }
}