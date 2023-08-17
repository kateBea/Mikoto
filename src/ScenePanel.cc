/**
 * ScenePanel.cc
 * Created by kate on 6/27/23.
 * */

// Third-Party Libraries
#include <volk.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

// Project Headers
#include <Core/Application.hh>
#include <Editor/Panels/ScenePanel.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    ScenePanel::ScenePanel(const std::shared_ptr<ScenePanelData>& data, const Path_T& iconPath)
        :   Panel{ iconPath }, m_Visible{ true }, m_Hovered{ false }, m_Focused{ false }, m_Data{ data }
    {
        m_SceneRendererVk = dynamic_cast<VulkanRenderer*>(Renderer::GetRendererAPIActive());

        // Create Sampler
        VkSamplerCreateInfo samplerCreateInfo{};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.minLod = -1000;
        samplerCreateInfo.maxLod = 1000;
        samplerCreateInfo.maxAnisotropy = 1.0f;

        if (vkCreateSampler(VulkanContext::GetPrimaryLogicalDevice(), &samplerCreateInfo, nullptr, &m_ColorAttachmentSampler) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan Renderer sampler!");

        m_DescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(m_ColorAttachmentSampler, m_SceneRendererVk->GetOffscreenColorAttachmentImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    auto ScenePanel::OnUpdate() -> void {
        if (m_Visible) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0,0});
            ImGui::Begin("Scene");
            m_Focused = ImGui::IsWindowFocused();
            m_Hovered = ImGui::IsWindowHovered();

            Application::GetPtr()->BlockImGuiLayerEvents(!m_Focused || !m_Hovered);

            auto viewPortDimensions{ ImGui::GetContentRegionAvail() };

            if (m_Data->ViewPortWidth != viewPortDimensions.x || m_Data->ViewPortHeight != viewPortDimensions.y) {
                //m_Data->SceneFrameBuffer->Resize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);
                m_Data->ViewPortWidth = viewPortDimensions.x;
                m_Data->ViewPortHeight = viewPortDimensions.y;
                m_Data->Viewport->OnViewPortResize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);
            }
#if false // TODO: working for opengl only for now (we should get this color attachment from the renderer see editor panel for more)
            ImTextureID textId{ reinterpret_cast<ImTextureID>(m_Data->SceneFrameBuffer->GetColorAttachmentId()) };
            float frameWidth{ static_cast<float>(m_Data->SceneFrameBuffer->GetFrameBufferProperties().width) };
            float frameHeight{ static_cast<float>(m_Data->SceneFrameBuffer->GetFrameBufferProperties().height) };
            ImGui::Image((ImTextureID)textId, ImVec2{ frameWidth, frameHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
#endif

            float frameWidth{ static_cast<float>(m_Data->ViewPortWidth) };
            float frameHeight{ static_cast<float>(m_Data->ViewPortHeight) };
            m_SceneRendererVk->SetViewport(0, 0, (UInt32_T)m_Data->ViewPortWidth, (UInt32_T)m_Data->ViewPortHeight);
            ImGui::Image((ImTextureID)m_DescriptorSet, ImVec2{ frameWidth, frameHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

            ImGui::End();
            ImGui::PopStyleVar();
        }
    }

    auto ScenePanel::OnEvent(Event& event) -> void {

    }
}
