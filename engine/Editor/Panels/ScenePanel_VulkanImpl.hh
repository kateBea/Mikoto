/**
 * ScenePanel_VulkanImpl.hh
 * Created by kate on 8/26/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_VULKAN_IMPL_HH
#define MIKOTO_SCENE_PANEL_VULKAN_IMPL_HH

// Third-Party Libraries
#include <volk.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

// Project Headers
#include <Core/Application.hh>
#include <Core/Events/Event.hh>
#include <Renderer/Renderer.hh>
#include <Editor/Panels/Panel.hh>
#include <Editor/Panels/ScenePanel.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {
    class ScenePanel_VkImpl : public ScenePanelInterface {
    private:
        auto Init_Impl(std::shared_ptr<ScenePanelData> data) -> void override {
            m_Data = std::move(data);
            m_SceneRenderer = dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr());

            // Create Sampler
            VkSamplerCreateInfo samplerCreateInfo{};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

            samplerCreateInfo.maxAnisotropy = 1.0f;
            samplerCreateInfo.mipLodBias = 0.0f;
            samplerCreateInfo.minLod = 0.0f;
            samplerCreateInfo.maxLod = 1.0f;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

            if (vkCreateSampler(VulkanContext::GetPrimaryLogicalDevice(), &samplerCreateInfo, nullptr, &m_ColorAttachmentSampler) != VK_SUCCESS)
                throw std::runtime_error("Failed to create Vulkan Renderer sampler!");

            m_DescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(m_ColorAttachmentSampler,
                                                                            m_SceneRenderer->GetOffscreenColorAttachmentImage(),
                                                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        auto OnUpdate_Impl() -> void override {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0,0});
            ImGui::Begin("Scene");
            m_Focused = ImGui::IsWindowFocused();
            m_Hovered = ImGui::IsWindowHovered();

            // TODO: review
            Application::GetPtr()->BlockImGuiLayerEvents(!m_Focused || !m_Hovered);
            auto viewPortDimensions{ ImGui::GetContentRegionAvail() };

            if (m_Data->ViewPortWidth != viewPortDimensions.x || m_Data->ViewPortHeight != viewPortDimensions.y) {

                m_Data->ViewPortWidth = viewPortDimensions.x;
                m_Data->ViewPortHeight = viewPortDimensions.y;
                m_Data->Viewport->OnViewPortResize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);
                //m_SceneRendererVk->OnFramebufferResize((UInt32_T)m_Data->ViewPortWidth, (UInt32_T)m_Data->ViewPortHeight);
            }

            float frameWidth{ static_cast<float>(m_Data->ViewPortWidth) };
            float frameHeight{ static_cast<float>(m_Data->ViewPortHeight) };
            ImGui::Image((ImTextureID)m_DescriptorSet, ImVec2{ frameWidth, frameHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

            ImGui::End();
            ImGui::PopStyleVar();
        }

        auto OnEvent_Impl(Event& event) -> void override {

        }

    private:
        VkSampler m_ColorAttachmentSampler{};
        VkDescriptorSet m_DescriptorSet{};
        VulkanRenderer* m_SceneRenderer{};
    };
}

#endif // MIKOTO_SCENE_PANEL_VULKAN_IMPL_HH
