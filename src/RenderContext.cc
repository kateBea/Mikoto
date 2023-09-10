/**
 * RenderContext.cc
 * Created by kate on 7/19/2023.
 * */

// Project Headers
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/RenderContext.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/OpenGL/OpenGLContext.hh>

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
    }

    auto RenderContext::ShutDown() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::OPENGL_API:
                OpenGLContext::ShutDown();
                break;
            case GraphicsAPI::VULKAN_API:
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
}