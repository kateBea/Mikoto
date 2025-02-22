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
#include <Library/String/String.hh>

#include "Core/Events/CoreEvents.hh"
#include "Core/System/EventSystem.hh"
#include "Panels/HierarchyPanel.hh"
#include "Panels/ScenePanel.hh"
#include "Renderer/Vulkan/VulkanContext.hh"


#include "GUI/IconsMaterialDesign.h"

// Third-Party Libraries
#include "backends/imgui_impl_vulkan.h"
#include "volk.h"

// Project Headers
#include <Core/Input/MouseCodes.hh>
#include <Core/System/InputSystem.hh>
#include <GUI/ImGuiManager.hh>
#include <Scene/Scene/Scene.hh>

#include "Common/Common.hh"
#include "Renderer/Vulkan/VulkanDeletionQueue.hh"
#include "Renderer/Vulkan/VulkanRenderer.hh"

namespace Mikoto {

    class ScenePanel_VkImpl final : public ScenePanelApi {
    public:
        explicit ScenePanel_VkImpl( const SceneApiCreateInfo& createInfo)
            : ScenePanelApi{ createInfo }
        {}

        auto Init() -> void override {
            m_ActiveManipulationMode = GuizmoManipulationMode::TRANSLATION;

            // Create a Sampler for the texture we will display in the viewport
            VkSamplerCreateInfo samplerCreateInfo{ VulkanHelpers::Initializers::SamplerCreateInfo() };
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

            if (vkCreateSampler(VulkanContext::Get().GetDevice().GetLogicalDevice(), &samplerCreateInfo, nullptr, &m_ColorAttachmentSampler) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR("Failed to create Vulkan sampler!");
            }

            VulkanDeletionQueue::Push([sampler = m_ColorAttachmentSampler]() -> void {
                vkDestroySampler(VulkanContext::Get().GetDevice().GetLogicalDevice(), sampler, nullptr);
            });

            // Create the Descriptor set for the texture displayed in the ImGuiWindow scene

            const VulkanRenderer* vulkanSceneRenderer{ dynamic_cast<const VulkanRenderer*>( m_Renderer ) };

            m_ColorAttachmentDescriptorSet =
                ImGui_ImplVulkan_AddTexture(m_ColorAttachmentSampler, vulkanSceneRenderer->GetFinalImage().GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            ImGuiManager::AddShutdownCallback( [ds = m_ColorAttachmentDescriptorSet]() -> void {
                ImGui_ImplVulkan_RemoveTexture(ds);
                } );
        }

        auto OnUpdate() -> void override {
            const ImVec2 viewPortDimensions{ ImGui::GetContentRegionAvail() };

            // If the window size has changed, we need to resize the scene viewport
            if ( m_ViewPortWidth != viewPortDimensions.x || m_ViewPortHeight != viewPortDimensions.y ) {
                m_ViewPortWidth = viewPortDimensions.x;
                m_ViewPortHeight = viewPortDimensions.y;
                m_TargetScene->OnViewPortResize( viewPortDimensions.x, viewPortDimensions.y );
            }

            ImGui::Image( reinterpret_cast<ImTextureID>( m_ColorAttachmentDescriptorSet ),
                ImVec2{ m_ViewPortWidth, m_ViewPortHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

            SetupGuizmos();

            HandleManipulationMode();
        }

    private:
        VkSampler m_ColorAttachmentSampler{};
        VkDescriptorSet m_ColorAttachmentDescriptorSet{};
    };


    static constexpr auto GetSceneName() -> std::string_view {
        return "Scene";
    }

    ScenePanel::ScenePanel(const ScenePanelCreateInfo& createInfo)
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_IMAGE, GetSceneName());

        // Initialize implementation
        SceneApiCreateInfo sceneApiCreateInfo{
            .ViewportWidth{ createInfo.Width },
            .ViewportHeight{ createInfo.Height },
            .TargetScene{ createInfo.TargetScene },
            .Renderer{ createInfo.Renderer },
            .EditorMainCamera{ createInfo.EditorMainCamera },
            .GetActiveEntityCallback{ createInfo.GetActiveEntityCallback },
        };

        // Set scene panel implementation
        m_Implementation = CreateScope<ScenePanel_VkImpl>(sceneApiCreateInfo);

        if (m_Implementation != nullptr) {
            m_Implementation->Init();
        } else {
            MKT_APP_LOGGER_ERROR( "ScenePanel::ScenePanel - Failed to create Scene Panel ImGui implementation." );
        }
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

            m_Implementation->OnUpdate();

            ImGui::End();

            ImGui::PopStyleVar();
        }
    }

    auto ScenePanelApi::SetupGuizmos() const -> void {
        Entity* currentSelection{ m_GetActiveEntityCallback() };
        if (currentSelection != nullptr && currentSelection->IsValid()) {
            if (!currentSelection->GetComponent<TagComponent>().IsVisible()) {
                return;
            }

            ImGuizmo::SetOrthographic(m_EditorMainCamera->IsOrthographic());
            ImGuizmo::SetDrawlist();

            const ImVec2 windowPosition{ ImGui::GetWindowPos() };
            const ImVec2 windowDimensions{ ImGui::GetWindowSize() };
            ImGuizmo::SetRect(windowPosition.x, windowPosition.y, windowDimensions.x, windowDimensions.y);
        }
    }

    auto ScenePanelApi::HandleManipulationMode() const -> void {
        Entity* currentSelection{ m_GetActiveEntityCallback() };
        if (currentSelection == nullptr || !currentSelection->IsValid()) {
            return;
        }

        TransformComponent& transformComponent{ currentSelection->GetComponent<TransformComponent>() };

        const glm::mat4& cameraView{ m_EditorMainCamera->GetViewMatrix() };
        const glm::mat4& cameraProjection{ m_EditorMainCamera->GetProjection() };
        glm::mat4 objectTransform{ transformComponent.GetTransform() };
        glm::vec3 oldTranslation{ transformComponent.GetTranslation() };

        switch (m_ActiveManipulationMode) {
            case GuizmoManipulationMode::TRANSLATION:
                ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(objectTransform));
            break;
            case GuizmoManipulationMode::ROTATION:
                ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(objectTransform));
            break;
            case GuizmoManipulationMode::SCALE:
                ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), ImGuizmo::OPERATION::SCALE, ImGuizmo::MODE::LOCAL, glm::value_ptr(objectTransform));
            break;
        }

        if (ImGuizmo::IsUsing()) {
            transformComponent.SetTransform(objectTransform);

            // Apply the transformation to the children
            // For now Guizmos only change translation so thats the only thing we handle in the children

            glm::vec3 offsetTranslation{ transformComponent.GetTranslation() - oldTranslation };
            auto& hierarchy{ m_TargetScene->GetHierarchy() };
            hierarchy.ForAllChildren( [offsetTranslation]( Entity* child ) -> void {
                TransformComponent& childTransform{ child->GetComponent<TransformComponent>() };

                childTransform.SetTranslation( childTransform.GetTranslation() + offsetTranslation );

            } , [&](Entity* target) -> bool {
                return target->GetComponent<TagComponent>().GetGUID() ==
                    currentSelection->GetComponent<TagComponent>().GetGUID();
            });
        }
    }
}
