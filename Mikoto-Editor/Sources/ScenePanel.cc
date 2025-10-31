/**
 * ScenePanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "imgui.h"
#include "glm/gtc/type_ptr.hpp"

// Important to include after imgui
#include <ImGuizmo.h>
#include <imgui.h>

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <Common/Common.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Library/String/String.hh>
#include <Panels/ScenePanel.hh>
#include <Scene/Component.hh>

namespace Mikoto {

    static constexpr auto GetSceneName() -> std::string_view { return "Scene"; }

    auto ShowGizmoToolbarOverlay(bool* open) -> void {
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove;

        const float PAD = 20.0f;

        // Get the position and size of the Scene window
        ImVec2 scenePos = ImGui::GetWindowPos();

        // Position overlay relative to the Scene window’s top-left corner
        ImVec2 window_pos = ImVec2(scenePos.x + PAD, scenePos.y + 2 * PAD);

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID); // match same viewport as Scene

        ImGui::SetNextWindowBgAlpha(0.35f); // semi-transparent background

        if (ImGui::Begin("##GizmoToolbar", open, window_flags))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));

            ImGui::BeginGroup();

            // These could be ImageButtons with icons instead
            if (ImGui::Button(ICON_MD_TRANSFORM)) { /* Set translate gizmo */ }
            ImGui::SameLine();
            if (ImGui::Button(ICON_MD_ROTATE_LEFT)) { /* Set rotate gizmo */ }
            ImGui::SameLine();
            if (ImGui::Button(ICON_MD_SCALE)) { /* Set scale gizmo */ }

            ImGui::EndGroup();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();


            ImGui::BeginGroup();

            if (ImGui::Button(ICON_MD_PLAY_ARROW)) { /* Play */ }
            ImGui::SameLine();
            if (ImGui::Button(ICON_MD_PAUSE)) { /* Pause */ }
            ImGui::SameLine();
            if (ImGui::Button(ICON_MD_STOP)) { /* Stop */ }

            ImGui::EndGroup();
            ImGui::PopStyleVar(2);
        }
        ImGui::End();
    }


    ScenePanel::ScenePanel( const ScenePanelCreateInfo &createInfo )
        : m_EditorState{ createInfo.State }, m_ViewPortWidth( createInfo.Width ), m_ViewPortHeight( createInfo.Height ) {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_IMAGE, GetSceneName() );

        ImGuiBackend *backend{ ImGuiService::Get()->GetBackend() };
        m_DisplayTargetImGuiID = backend->ConstructImGuiTextureID( m_EditorState->FinalComposition );
    }

    auto ScenePanel::OnUpdate( MKT_UNUSED_VAR float ts ) -> void {
        if ( m_PanelIsVisible ) {
            constexpr ImGuiWindowFlags windowFlags{};

            // Expand scene view to window bounds (no padding)
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );
            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

            DrawScenePlayButtons();

            m_PanelIsFocused = ImGui::IsWindowFocused();
            m_PanelIsHovered = ImGui::IsWindowHovered();

            UpdateViewport();

            static bool open{ true };
            ShowGizmoToolbarOverlay(&open);

            SetupManipulation();
            DrawManipulationGuizmos();

            ImGui::End();

            ImGui::PopStyleVar();
        }
    }

    auto ScenePanel::SetManipulation( GuizmoType mode ) -> void {
        m_GuizmoType = mode;
    }

    auto ScenePanel::GetViewportWidth() const -> float {
        return m_ViewPortWidth;
    }

    auto ScenePanel::GetViewportHeight() const -> float {
        return m_ViewPortHeight;
    }

    auto ScenePanel::UpdateViewport() -> void {
        const ImVec2 dim{ ImGui::GetContentRegionAvail() };

        if ( m_ViewPortWidth != dim.x || m_ViewPortHeight != dim.y ) {
            m_ViewPortWidth = dim.x;
            m_ViewPortHeight = dim.y;
        }

        ImGui::Image( m_DisplayTargetImGuiID, ImVec2{ dim.x, dim.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 } );
    }
    auto ScenePanel::SetupManipulation() const -> void {
        Entity *currentSelection{ m_EditorState->SelectedEntity };
        if ( currentSelection != nullptr && currentSelection->IsValid() ) {
            if ( !currentSelection->GetComponent<TagComponent>().IsActive() ) {
                return;
            }

            ImGuizmo::SetOrthographic( m_EditorState->EditorCamera->IsOrthographic() );
            ImGuizmo::SetDrawlist();

            const ImVec2 windowPosition{ ImGui::GetWindowPos() };
            const ImVec2 windowDimensions{ ImGui::GetWindowSize() };
            ImGuizmo::SetRect( windowPosition.x, windowPosition.y, windowDimensions.x, windowDimensions.y );
        }
    }

    auto ScenePanel::DrawManipulationGuizmos() -> void {
        Entity *currentSelection{ m_EditorState->SelectedEntity };
        if ( currentSelection == nullptr || !currentSelection->IsValid() ) { return; }

        TransformComponent &transformComponent{ currentSelection->GetComponent<TransformComponent>() };

        const glm::mat4 &cameraView{ m_EditorState->EditorCamera->GetViewMatrix() };
        const glm::mat4 &cameraProjection{ m_EditorState->EditorCamera->GetProjection() };
        glm::mat4 objectTransform{ transformComponent.GetTransform() };
        glm::vec3 oldTranslation{ transformComponent.GetTranslation() };
        glm::vec3 oldRotation{ transformComponent.GetRotation() };
        glm::vec3 oldScale{ transformComponent.GetScale() };

        switch ( m_GuizmoType ) {
            case GuizmoType::TRANSLATION:
                ImGuizmo::Manipulate( glm::value_ptr( cameraView ), glm::value_ptr( cameraProjection ), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::MODE::LOCAL, glm::value_ptr( objectTransform ) );
                break;
            case GuizmoType::ROTATION:
                ImGuizmo::Manipulate( glm::value_ptr( cameraView ), glm::value_ptr( cameraProjection ), ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, glm::value_ptr( objectTransform ) );
                break;
            case GuizmoType::SCALE:
                ImGuizmo::Manipulate( glm::value_ptr( cameraView ), glm::value_ptr( cameraProjection ), ImGuizmo::OPERATION::SCALE, ImGuizmo::MODE::LOCAL, glm::value_ptr( objectTransform ) );
                break;
        }

        if ( ImGuizmo::IsUsing() ) {
            transformComponent.SetTransform( objectTransform );

            // Apply the transformation to the children
            // For now Guizmos only change translation so thats the only thing we handle in the children

            const glm::vec3 offsetTranslation{ transformComponent.GetTranslation() - oldTranslation };
            const glm::vec3 offsetRotation{ transformComponent.GetRotation() - oldRotation };
            const glm::vec3 offsetScale{ transformComponent.GetScale() - oldScale };

            //propagate changes
        }
    }

    auto ScenePanel::DrawScenePlayButtons() const -> void {

    }
}// namespace Mikoto
