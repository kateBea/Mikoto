/**
 * ScenePanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "glm/gtc/type_ptr.hpp"
#include "imgui.h"

// Important to include after imgui
#include <volk.h>
#include "ImGuizmo.h"

// Project Headers
#include <STL/String/String.hh>

#include "Core/CoreEvents.hh"
#include "Core/EventManager.hh"
#include "Panels/HierarchyPanel.hh"
#include "Panels/ScenePanel.hh"
#include "Platform/Input/InputManager.hh"
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Scene/SceneManager.hh"


#include "GUI/IconsMaterialDesign.h"

// Third-Party Libraries
#include "backends/imgui_impl_vulkan.h"
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/RenderingUtils.hh"
#include "Renderer/Vulkan/DeletionQueue.hh"
#include "Renderer/Vulkan/VulkanRenderer.hh"

namespace Mikoto {

    class ScenePanel_VkImpl : public ScenePanelInterface {
    private:
        auto Init_Impl( const ScenePanelData& data) -> void override {
            m_Data = data;
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

            m_ColorAttachmentDescriptorSet = ImGui_ImplVulkan_AddTexture(m_ColorAttachmentSampler, m_SceneRenderer->GetFinalImage().GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            DeletionQueue::Push([descSet = m_ColorAttachmentDescriptorSet]() -> void {
                ImGui_ImplVulkan_RemoveTexture(descSet);
            });
        }

        auto OnUpdate_Impl() -> void override {
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
        }

    private:
        VkSampler m_ColorAttachmentSampler{};
        VkDescriptorSet m_ColorAttachmentDescriptorSet{};
        VulkanRenderer* m_SceneRenderer{};

    };


    static constexpr auto GetSceneName() -> std::string_view {
        return "Scene";
    }

    ScenePanel::ScenePanel(ScenePanelCreateInfo&& createInfo)
        :   Panel{}, m_CreateInfo{ createInfo }
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_IMAGE, GetSceneName());

        // Set scene panel implementation
        switch (Renderer::GetActiveGraphicsAPI()) {
        case GraphicsAPI::VULKAN_API:
            m_Implementation = std::make_unique<ScenePanel_VkImpl>();
            break;
        }

        // Initialize implementation
        ScenePanelData data{};
        data.ViewPortWidth = 1920;
        data.ViewPortHeight = 1080;

        m_Implementation->Init_Impl(data);
        m_Implementation->SetEditorCamera(m_CreateInfo.EditorMainCamera);
    }

    auto ScenePanel::OnUpdate(MKT_UNUSED_VAR float ts) -> void {
        if (m_PanelIsVisible) {
            constexpr ImGuiWindowFlags windowFlags{};

            // Expand scene view to window bounds (no padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f,0.0f });
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), windowFlags);

            //DrawScenePlayButtons();

            m_PanelIsFocused = ImGui::IsWindowFocused();
            m_PanelIsHovered = ImGui::IsWindowHovered();

            m_Implementation->OnUpdate_Impl();

            ImGui::End();

            ImGui::PopStyleVar();
        }

        if (m_PanelIsHovered && InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Right)) {
            EventManager::Trigger<CameraEnableRotation>();
        }
    }

    auto ScenePanel::DrawScenePlayButtons() -> void {
        if (ImGui::Button(fmt::format("{}", ICON_MD_PLAY_ARROW).c_str())) {}
        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }


        ImGui::SameLine();

        if (ImGui::Button(fmt::format("{}", ICON_MD_STOP).c_str())) {}
        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }
    }

    auto ScenePanelInterface::HandleGuizmos() -> void {
        auto currentSelection{ SceneManager::GetCurrentSelection() };
        if (currentSelection.has_value()) {
            if (!currentSelection->get().GetComponent<TagComponent>().IsVisible()) {
                return;
            }

            ImGuizmo::SetOrthographic(m_EditorMainCamera->IsOrthographic());
            ImGuizmo::SetDrawlist();

            const auto windowPosition{ ImGui::GetWindowPos() };
            const auto windowDimensions{ ImGui::GetWindowSize() };
            ImGuizmo::SetRect(windowPosition.x, windowPosition.y, windowDimensions.x, windowDimensions.y);

            HandleManipulationMode();
        }
    }

    auto ScenePanelInterface::HandleManipulationMode() -> void {
        auto& currentSelectionContext{ SceneManager::GetCurrentSelection()->get() };
        TransformComponent& transformComponent{ currentSelectionContext.GetComponent<TransformComponent>() };

        const auto& cameraView{ m_EditorMainCamera->GetViewMatrix() };
        const auto& cameraProjection{ m_EditorMainCamera->GetProjection() };
        auto objectTransform{ transformComponent.GetTransform() };

        if (currentSelectionContext.HasComponent<RenderComponent>() &&
                currentSelectionContext.GetComponent<RenderComponent>().GetObjectData().MeshData.Data != nullptr) {
            // Show guizmos when there's content to manipulate
            // There may bo contents, for example, when we create a
            // new renderable but the actual model is not loaded

            switch (m_ActiveManipulationMode) {
                case ManipulationMode::TRANSLATION:
                    ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(objectTransform));
                    break;
                case ManipulationMode::ROTATION:
                    ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(objectTransform));
                    break;
                case ManipulationMode::SCALE:
                    ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), ImGuizmo::OPERATION::SCALE, ImGuizmo::MODE::LOCAL, glm::value_ptr(objectTransform));
                    break;
            }

            if (ImGuizmo::IsUsing()) {
                transformComponent.SetTransform(objectTransform);
            }
        }
    }
}
