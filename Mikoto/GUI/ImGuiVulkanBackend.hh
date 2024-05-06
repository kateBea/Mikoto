/**
 * ImGuiVulkanBackend.hh
 * Created by kate on 9/14/23.
 * */

#ifndef MIKOTO_IMGUI_VULKAN_BACKEND_HH
#define MIKOTO_IMGUI_VULKAN_BACKEND_HH

// C++ Standard Library
#include <any>
#include <memory>
#include <vector>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <vulkan/vulkan_core.h>

#include <GUI/ImGuiUtils.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanImage.hh>

namespace Mikoto {
    class ImGuiVulkanBackend : public BackendImplementation {
    public:
        auto Init(std::any functionName) -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

    private:
        auto PrepareForRender() -> void;
        auto CreateRenderPass() -> void;
        auto CreateImages() -> void;
        auto CreateFrameBuffer() -> void;
        auto BuildCommandBuffers() -> void;
        auto RecordImGuiCommandBuffers( VkCommandBuffer cmd ) -> void;

        auto DrawImGui( VkCommandBuffer cmd, VkImage currentSwapChainImage ) -> void;
    private:
        VkRenderPass m_ImGuiRenderPass{};
        VkDescriptorPool m_ImGuiDescriptorPool{};

        VulkanImage m_ColorImage{};
        VulkanImage m_DepthImage{};
        VulkanFrameBuffer m_DrawFrameBuffer{};

        VkFormat m_ColorAttachmentFormat{};
        VkFormat m_DepthAttachmentFormat{};

        VkExtent2D m_Extent2D{ 2560, 1440 };
        VkExtent3D m_Extent3D{ 2560, 1440, 1 };
    };
}


#endif // MIKOTO_IMGUI_VULKAN_BACKEND_HH
