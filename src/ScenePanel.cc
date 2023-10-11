/**
 * ScenePanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <Editor/ScenePanel.hh>
#include <Scene/SceneManager.hh>
#include <Core/CoreEvents.hh>
#include <Core/EventManager.hh>
#include <Platform/InputManager.hh>
#include <Editor/HierarchyPanel.hh>
#include <Editor/ScenePanel_OpenGLImpl.hh>
#include <Editor/ScenePanel_VulkanImpl.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    ScenePanel::ScenePanel(ScenePanelCreateInfo&& createInfo, const Path_T &iconPath)
        :   Panel{ iconPath }, m_CreateInfo{ std::move( createInfo ) }
    {
        // Set scene panel implementation
        switch (Renderer::GetActiveGraphicsAPI()) {
        case GraphicsAPI::OPENGL_API:
            m_Implementation = std::make_unique<ScenePanel_OGLImpl>();
            break;
        case GraphicsAPI::VULKAN_API:
            m_Implementation = std::make_unique<ScenePanel_VkImpl>();
            break;
        }

        // Initialize implementation
        ScenePanelData data{};
        data.ViewPortWidth = 1920;
        data.ViewPortHeight = 1080;

        m_Implementation->Init_Impl(std::move(data));
        m_Implementation->SetEditorCamera(m_CreateInfo.EditorMainCamera);
    }

    auto ScenePanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            m_Implementation->OnUpdate_Impl();
        }

        m_PanelIsHovered = m_Implementation->IsHovered();
        m_PanelIsFocused = m_Implementation->IsFocused();

        if (m_PanelIsHovered && InputManager::IsMouseKeyPressed(MouseButton::Mouse_Button_Left)) {
            EventManager::Trigger<CameraEnableRotation>();
        }
    }

    auto ScenePanelInterface::HandleGuizmos() -> void {
        if (SceneManager::IsEntitySelected()) {
            if (!SceneManager::GetCurrentlySelectedEntity().GetComponent<TagComponent>().IsVisible()) {
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
        TransformComponent& transformComponent{ SceneManager::GetCurrentlySelectedEntity().GetComponent<TransformComponent>() };
        const auto& cameraView{ m_EditorMainCamera->GetViewMatrix() };
        const auto& cameraProjection{ m_EditorMainCamera->GetProjection() };
        auto objectTransform{ transformComponent.GetTransform() };

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
