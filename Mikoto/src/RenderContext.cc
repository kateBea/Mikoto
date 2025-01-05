/**
 * RenderContext.cc
 * Created by kate on 7/19/2023.
 * */

// Project Headers
#include <Common/RenderingUtils.hh>
#include <Core/EventManager.hh>
#include <Renderer/Core/RenderContext.hh>
#include <Renderer/Core/Renderer.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanUtils.hh>

namespace Mikoto {
    auto RenderContext::Init(RenderContextData&& spec) -> void {
        s_Spec = std::move(spec);

        switch (s_Spec.TargetAPI) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::Init(s_Spec.Handle);
                break;
        }

        Renderer::Init(RendererSpec{ .Backend = s_Spec.TargetAPI });
    }


    auto RenderContext::Shutdown() -> void {
        // Renderer shutdown
        Renderer::Shutdown();

        switch (s_Spec.TargetAPI) {
            case GraphicsAPI::VULKAN_API:
                // Wait for remaining operations
                VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

                for (const auto& func : s_ShutdownCallbacks) {
                    func();
                }

                // Delete remaining vulkan objects
                DeletionQueue::Flush();

                // Shut down the context
                VulkanContext::Shutdown();
                break;
        }
    }


    auto RenderContext::PresentFrame() -> void {
        switch (s_Spec.TargetAPI) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::Present();
                break;
        }
    }


    MKT_UNUSED_FUNC auto RenderContext::IsVSyncActive() -> bool {
        switch (s_Spec.TargetAPI) {
            case GraphicsAPI::VULKAN_API:
                return VulkanContext::IsVSyncActive();
            default: return false;
        }
    }


    auto RenderContext::EnableVSync() -> void {
        switch (s_Spec.TargetAPI) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::EnableVSync();
                break;
        }
    }


    auto RenderContext::DisableVSync() -> void {
        switch ( s_Spec.TargetAPI ) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::DisableVSync();
                break;
        }
    }
    auto RenderContext::PushShutdownCallback(const std::function<void()>& func) -> void {
        s_ShutdownCallbacks.emplace_back(func);
    }


    auto RenderContext::PrepareFrame() -> void {
        switch (s_Spec.TargetAPI) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::PrepareFrame();
                break;
        }
    }


    auto RenderContext::SubmitFrame() -> void {
        switch (s_Spec.TargetAPI) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::SubmitFrame();
                break;
        }
    }
}