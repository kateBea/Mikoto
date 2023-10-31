/**
 * ScenePanel_VulkanImpl.hh
 * Created by kate on 8/26/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_VULKAN_IMPL_HH
#define MIKOTO_SCENE_PANEL_VULKAN_IMPL_HH

// Third-Party Libraries
#include "ImGuizmo.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/VulkanUtils.hh"
#include "Common/RenderingUtils.hh"
#include "Core/Event.hh"
#include "Renderer/Renderer.hh"
#include "Renderer/Vulkan/DeletionQueue.hh"
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Renderer/Vulkan/VulkanRenderer.hh"
#include "Scene/SceneManager.hh"
#include "ScenePanel.hh"

namespace Mikoto {
    class ScenePanel_VkImpl : public ScenePanelInterface {
    private:
        auto Init_Impl(ScenePanelData&& data) -> void override {
            m_Data = std::move(data);
            m_SceneRenderer = dynamic_cast<VulkanRenderer*>(Renderer::GetActiveGraphicsAPIPtr());
            m_ActiveManipulationMode = ManipulationMode::TRANSLATION;

            // Create Sampler
            VkSamplerCreateInfo samplerCreateInfo{ VulkanUtils::Initializers::SamplerCreateInfo() };
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

            if (vkCreateSampler(VulkanContext::GetPrimaryLogicalDevice(), &samplerCreateInfo, nullptr, &m_ColorAttachmentSampler) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to create Vulkan sampler!");
            }

            DeletionQueue::Push([sampler = m_ColorAttachmentSampler]() -> void {
                vkDestroySampler(VulkanContext::GetPrimaryLogicalDevice(), sampler, nullptr);
            });

            m_ColorAttachmentDescriptorSet = ImGui_ImplVulkan_AddTexture(m_ColorAttachmentSampler, m_SceneRenderer->GetOffscreenColorAttachmentImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            DeletionQueue::Push([descSet = m_ColorAttachmentDescriptorSet]() -> void {
                ImGui_ImplVulkan_RemoveTexture(descSet);
            });
        }

        auto OnUpdate_Impl() -> void override {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

            auto viewPortDimensions{ ImGui::GetContentRegionAvail() };
            auto& currentlyActiveScene{ SceneManager::GetActiveScene() };


            // If the window size has changed, we need to resize the scene viewport
            if (m_Data.ViewPortWidth != viewPortDimensions.x || m_Data.ViewPortHeight != viewPortDimensions.y) {

                m_Data.ViewPortWidth = viewPortDimensions.x;
                m_Data.ViewPortHeight = viewPortDimensions.y;
                currentlyActiveScene.OnViewPortResize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);
                //m_SceneRendererVk->OnFramebufferResize((UInt32_T)m_Data->ViewPortWidth, (UInt32_T)m_Data->ViewPortHeight);
            }

            float frameWidth{ static_cast<float>(m_Data.ViewPortWidth) };
            float frameHeight{ static_cast<float>(m_Data.ViewPortHeight) };
            ImGui::Image((ImTextureID) m_ColorAttachmentDescriptorSet, ImVec2{ frameWidth, frameHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

            HandleGuizmos();

            ImGui::PopStyleVar();
        }

    private:
        VkSampler m_ColorAttachmentSampler{};
        VkDescriptorSet m_ColorAttachmentDescriptorSet{};
        VulkanRenderer* m_SceneRenderer{};

    };
}

#endif // MIKOTO_SCENE_PANEL_VULKAN_IMPL_HH
