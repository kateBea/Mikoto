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

#include <ImGui/ImGuiService.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanFramebuffer.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace Mikoto {

    class ImGuiVulkanBackend final : public ImGuiBackend {
    public:
        explicit ImGuiVulkanBackend( const ImGuiBackendCreateInfo& createInfo )
            : ImGuiBackend{ createInfo }, m_Extent2D{
                  .width{ static_cast<UInt32>( createInfo.Handle->GetWidth() ) },
                  .height{ static_cast<UInt32>( createInfo.Handle->GetHeight() ) }
              },
              m_Extent3D{
                  .width{ static_cast<UInt32>( createInfo.Handle->GetWidth() ) },
                  .height{ static_cast<UInt32>( createInfo.Handle->GetHeight() ) }, .depth{ 1 }
              }
        {}

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

    private:
        auto InitImGuiForVulkan() -> void;
        auto CreateRenderPass() -> void;
        auto CreateImages() -> void;
        auto CreateFrameBuffer() -> void;

        auto RecordRenderPassCommands(CommandListHandle cmdList ) -> void;
        auto RecordCommands( TextureHandle swapChainDrawTarget, CommandListHandle cmdList  ) -> void;

    private:

        VkRenderPass m_ImGuiRenderPass{};
        VkDescriptorPool m_ImGuiDescriptorPool{};

        TextureHandle m_ColorImage{};
        TextureHandle m_DepthImage{};
        FramebufferHandle m_DrawFrameBuffer{};

        VkExtent2D m_Extent2D{ 2560, 1440 };
        VkExtent3D m_Extent3D{ 2560, 1440, 1 };
    };
}


#endif // MIKOTO_IMGUI_VULKAN_BACKEND_HH
