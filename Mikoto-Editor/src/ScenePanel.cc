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
#include "ImGuizmo.h"

// Project Headers
#include "Common/StringUtils.hh"

#include "Core/CoreEvents.hh"
#include "Core/EventManager.hh"
#include "Panels/HierarchyPanel.hh"
#include "Panels/ScenePanel.hh"
#include "Panels/ScenePanelOpenGLImpl.hh"
#include "Panels/ScenePanelVulkanImpl.hh"
#include "Platform/InputManager.hh"
#include "Renderer/Vulkan/VulkanContext.hh"
#include "Scene/SceneManager.hh"


#include "GUI/IconsFontAwesome5.h"
#include "GUI/IconsMaterialDesign.h"
#include "GUI/IconsMaterialDesignIcons.h"

namespace Mikoto {
    static constexpr auto GetSceneName() -> std::string_view {
        return "Scene";
    }

    ScenePanel::ScenePanel(ScenePanelCreateInfo&& createInfo)
        :   Panel{}, m_CreateInfo{ std::move( createInfo ) }
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_IMAGE, GetSceneName());

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
        auto& currentSelectionContext{ SceneManager::GetCurrentlySelectedEntity() };
        TransformComponent& transformComponent{ SceneManager::GetCurrentlySelectedEntity().GetComponent<TransformComponent>() };

        const auto& cameraView{ m_EditorMainCamera->GetViewMatrix() };
        const auto& cameraProjection{ m_EditorMainCamera->GetProjection() };
        auto objectTransform{ transformComponent.GetTransform() };

        if (currentSelectionContext.HasComponent<RenderComponent>() &&
                currentSelectionContext.GetComponent<RenderComponent>().GetObjectData().ObjectModel != nullptr) {
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
