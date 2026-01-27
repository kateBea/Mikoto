//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_IMGUI_VULKAN_BACKEND_HH
#define MIKOTO_IMGUI_VULKAN_BACKEND_HH

#include <any>
#include <memory>
#include <vector>

#include <volk.h>
#include <ankerl/unordered_dense.h>

#include <ImGui/ImGuiService.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanFramebuffer.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace Mikoto {

    class ImGuiVulkanBackend final : public ImGuiBackend {
    public:
        explicit ImGuiVulkanBackend( const ImGuiBackendCreateInfo& createInfo )
            : ImGuiBackend{ createInfo },
        m_Extent2D{
            .width{ static_cast<UInt32>( createInfo.Handle->GetWidth() ) },
            .height{ static_cast<UInt32>( createInfo.Handle->GetHeight() ) } },
        m_Extent3D{
            .width{ static_cast<UInt32>( createInfo.Handle->GetWidth() ) },
            .height{ static_cast<UInt32>( createInfo.Handle->GetHeight() ) },
            .depth{ 1 } } {}

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        MKT_NODISCARD auto GetFinalComposition() -> TextureHandle override;

        MKT_NODISCARD auto ConstructImGuiTextureID( const Texture* texture ) -> ImTextureID override;
        MKT_NODISCARD auto ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID override;

    private:
        auto InitImGuiForVulkan() -> void;
        auto CreateRenderPass() -> void;
        auto CreateImages() -> void;
        auto CreateFrameBuffer() -> void;

        auto RecordRenderPassCommands(CommandListHandle cmdList ) -> void;
        auto RecordDynamicRenderCommands(CommandListHandle cmdList ) -> void;

        auto SetupViewportAndScissors( CommandListHandle cmdList ) -> void;
        auto RecordCommands( CommandListHandle cmdList  ) -> void;

    private:
#if defined( MKT_USE_VULKAN_DYNAMIC_RENDERING )
        const bool m_UseDynamicRendering{ true };
#else
        const bool m_UseDynamicRendering{ false };
#endif

        VkRenderPass m_ImGuiRenderPass{};
        VkDescriptorPool m_ImGuiDescriptorPool{};

        TextureHandle m_ColorImage{};
        TextureHandle m_DepthImage{};
        FramebufferHandle m_DrawFrameBuffer{};

        VkExtent2D m_Extent2D{ 2560, 1440 };
        VkExtent3D m_Extent3D{ 2560, 1440, 1 };

        struct ImGuiTextIDInfo {
            VkDescriptorSet descriptorSet{};
        };
        ankerl::unordered_dense::map<const Texture*, ImGuiTextIDInfo> m_ImGuiSets{};
    };
}// namespace Mikoto


#endif // MIKOTO_IMGUI_VULKAN_BACKEND_HH
