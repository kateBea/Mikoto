/**
 * RenderContext.cc
 * Created by kate on 7/19/2023.
 * */

// Project Headers
#include <Common/RenderingUtils.hh>
#include <Core/EventManager.hh>
#include <GUI/ImGuiManager.hh>
#include <Renderer/RenderContext.hh>
#include <Renderer/Vulkan/DeletionQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {
    auto RenderContext::Init(RenderContextSpec&& spec) -> void {
        s_Spec = std::move(spec);

        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::Init(s_Spec.WindowHandle);
                break;
        }

        Renderer::Init(RendererSpec{ .Backend = s_Spec.Backend });
        ImGuiManager::Init(s_Spec.WindowHandle);
    }


    auto RenderContext::Shutdown() -> void {
        // Renderer shutdown
        Renderer::Shutdown();

        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                // Wait for remaining operations
                VulkanUtils::WaitOnDevice(VulkanContext::GetPrimaryLogicalDevice());

                // Delete remaining vulkan objects
                DeletionQueue::Flush();

                // Imgui manager shutdown. Imgui is shut down after flushing the delete queue
                // because some descriptor sets allocated from imgui may need the device to
                // still be alive amongst some other structures
                ImGuiManager::Shutdown();

                // Shut down the context
                VulkanContext::Shutdown();
                break;
        }
    }


    auto RenderContext::Present() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::Present();
                break;
        }
    }


    MKT_UNUSED_FUNC auto RenderContext::IsVSyncActive() -> bool {
        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                return VulkanContext::IsVSyncActive();
            default: return false;
        }
    }


    auto RenderContext::EnableVSync() -> void {
        switch (s_Spec.Backend) {
            case GraphicsAPI::VULKAN_API:
                VulkanContext::EnableVSync();
                break;
        }
    }


    auto RenderContext::DisableVSync() -> void {
        switch (s_Spec.Backend) {
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
}