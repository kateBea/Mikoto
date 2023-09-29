/**
 * ScenePanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <Core/Application.hh>
#include <Editor/Panels/ScenePanel.hh>
#include <Editor/Panels/HierarchyPanel.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Editor/Panels/ScenePanel_VulkanImpl.hh>
#include <Editor/Panels/ScenePanel_OpenGLImpl.hh>

namespace Mikoto {

    ScenePanel::ScenePanel(ScenePanelCreateInfo&& createInfo, const Path_T &iconPath)
        :   Panel{ iconPath }, m_CreateInfo{ std::move( createInfo ) }
    {
        m_PanelIsVisible = true;
        m_PanelIsHovered = false;
        m_PanelIsFocused = false;

        // Set scene panel implementation
        switch (Renderer::GetActiveGraphicsAPI()) {
        case GraphicsAPI::OPENGL_API:
            m_Implementation = std::make_shared<ScenePanel_OGLImpl>();
            break;
        case GraphicsAPI::VULKAN_API:
            m_Implementation = std::make_shared<ScenePanel_VkImpl>();
            break;
        }

        // Initialize implementation
        ScenePanelData data{};
        data.Viewport = std::make_unique<Scene>();
        data.ViewPortWidth = 1920;
        data.ViewPortHeight = 1080;

        m_Implementation->Init_Impl(std::move(data));
        m_Implementation->SetSceneCamera(m_CreateInfo.EditorMainCamera);

        CreateCallbacks();
    }

    auto ScenePanel::OnUpdate() -> void {
        if (m_PanelIsVisible)
            m_Implementation->OnUpdate_Impl();

        m_PanelIsHovered = m_Implementation->IsHovered();
        m_PanelIsFocused = m_Implementation->IsFocused();
    }

    auto ScenePanel::OnEvent(Event& event) -> void {
        m_Implementation->OnEvent_Impl(event);
    }

    auto ScenePanel::CreateCallbacks() -> void {
        m_Callbacks.EntitySelectionCallback = [&](HierarchyPanel* hierarchy) -> void {
            this->m_Implementation->SetContextSelection(hierarchy->GetContextSelection());
        };

        m_Callbacks.EntityDeselectionCallback = [&](HierarchyPanel* hierarchy) -> void {
            // For now, it does basically the same as when we deselect, might not be the case
            // in the future, for now we simply want to deselect the current game object
            this->m_Implementation->SetContextSelection(hierarchy->GetContextSelection());
        };
    }

    auto ScenePanelInterface::HandleGuizmos() -> void {
        if (m_CurrentContextSelection.IsValid()) {
            MKT_CORE_LOGGER_INFO("We got valid context selection for Guizmos :)");

            ImGuizmo::SetOrthographic(m_EditorMainCamera->IsOrthographic());
            ImGuizmo::SetDrawlist();

            const auto windowPosition{ ImGui::GetWindowPos() };
            const auto windowDimensions{ ImGui::GetWindowSize() };
            ImGuizmo::SetRect(windowPosition.x, windowPosition.y, windowDimensions.x, windowDimensions.y);

            TransformComponent& transformComponent{ m_CurrentContextSelection.GetComponent<TransformComponent>() };
            const auto& cameraView{ m_EditorMainCamera->GetViewMatrix() };
            const auto& cameraProjection{ m_EditorMainCamera->GetProjection() };
            auto objectTransform{ transformComponent.GetTransform() };

            ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr(objectTransform));

            if (ImGuizmo::IsUsing()) {
                transformComponent.SetTransform(objectTransform);
            }
        }
    }
}
