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

#include <GUI/ImGuiUtils.hh>
#include <GUI/BackendImplementation.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/Vulkan/VulkanCommandPool.hh>
#include <Renderer/Vulkan/VulkanFrameBuffer.hh>
#include <Renderer/Vulkan/VulkanImage.hh>

namespace Mikoto {

    class ImGuiVulkanBackend final : public BackendImplementation {
    public:
        explicit ImGuiVulkanBackend( const ImGuiBackendCreateInfo& createInfo )
            : BackendImplementation{ createInfo }, m_Extent2D{
                  .width{ static_cast<UInt32_T>( createInfo.Handle->GetWidth() ) },
                  .height{ static_cast<UInt32_T>( createInfo.Handle->GetHeight() ) }
              },
              m_Extent3D{
                  .width{ static_cast<UInt32_T>( createInfo.Handle->GetWidth() ) },
                  .height{ static_cast<UInt32_T>( createInfo.Handle->GetHeight() ) }, .depth{ 1 }
              }
        {}

        auto Init() -> bool override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

    private:
        auto InitImGuiForVulkan() -> void;
        auto InitializeImGuiRender() -> void;
        auto CreateRenderPass() -> void;
        auto CreateImages() -> void;
        auto CreateFrameBuffer() -> void;
        auto PrepareMainRenderPass( VkCommandBuffer cmd ) const -> void;
        auto InitCommandBuffers() -> void;
        auto InitializeCommands() -> void;

        auto RecordCommands( VkCommandBuffer cmd, VulkanImage& currentSwapchainImage ) const -> void;

    private:
        VkRenderPass m_ImGuiRenderPass{};
        VkDescriptorPool m_ImGuiDescriptorPool{};

        Scope_T<VulkanImage> m_ColorImage{};
        Scope_T<VulkanImage> m_DepthImage{};
        Scope_T<VulkanFrameBuffer> m_DrawFrameBuffer{};

        Scope_T<VulkanCommandPool> m_CommandPool{};
        std::vector<VkCommandBuffer> m_DrawCommandBuffers{};

        VkFormat m_ColorAttachmentFormat{};
        VkFormat m_DepthAttachmentFormat{};

        VkExtent2D m_Extent2D{ 2560, 1440 };
        VkExtent3D m_Extent3D{ 2560, 1440, 1 };
    };
}// namespace Mikoto


#endif // MIKOTO_IMGUI_VULKAN_BACKEND_HH
