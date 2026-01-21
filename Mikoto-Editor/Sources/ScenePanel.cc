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

    static auto InferManipulationMode( ImGuiUtils::GuizmoManipulationMode manipulation ) -> GuizmoType {
        switch (manipulation) {
            case ImGuiUtils::GuizmoManipulationMode::TRANSLATION:
                return GuizmoType::TRANSLATION;
            case ImGuiUtils::GuizmoManipulationMode::ROTATION:
                return GuizmoType::ROTATION;
            case ImGuiUtils::GuizmoManipulationMode::SCALE:
                return GuizmoType::SCALE;

            default: ;
        }

        return GuizmoType::TRANSLATION;
    }

    auto ScenePanel::CreateImguiTextureID() -> void {
        ImGuiBackend *backend{ ImGuiService::Get()->GetBackend() };

        if (const ImTextureID id{ backend->ConstructImGuiTextureID( m_EditorState->FinalComposition ) }; id != 0) { m_ColorImageID = id; }
    }

    auto ScenePanel::CreateWireframeImguiTextureID() -> void {
        ImGuiBackend *backend{ ImGuiService::Get()->GetBackend() };
        const ImTextureID id{ backend->ConstructImGuiTextureID( m_EditorState->WireframeComposition ) };

        if ( id != 0) {
            m_WireframeImageID = id;
        }
    }

    ScenePanel::ScenePanel( const ScenePanelCreateInfo &createInfo )
        : Panel{ "Scene" },  m_EditorState{ createInfo.State }, m_ViewPortWidth( createInfo.Width ), m_ViewPortHeight( createInfo.Height ) {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_IMAGE, m_PanelName );

        CreateImguiTextureID();
    }

    auto ScenePanel::ShowStatsOverlay(float timeStep) -> void {
        if (!m_PanelIsFocused) {
            return;
        }

        static int location{ -1 };
        ImGuiIO &io = ImGui::GetIO();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if (location >= 0) {
            const float PAD = 10.0f;
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos;// Use work area to avoid menu-bar/task-bar, if any!
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos, window_pos_pivot;
            window_pos.x = ( location & 1 ) ? ( work_pos.x + work_size.x - PAD ) : ( work_pos.x + PAD );
            window_pos.y = ( location & 2 ) ? ( work_pos.y + work_size.y - PAD ) : ( work_pos.y + PAD );
            window_pos_pivot.x = ( location & 1 ) ? 1.0f : 0.0f;
            window_pos_pivot.y = ( location & 2 ) ? 1.0f : 0.0f;
            ImGui::SetNextWindowPos( window_pos, ImGuiCond_Always, window_pos_pivot );
            ImGui::SetNextWindowViewport( viewport->ID );
            window_flags |= ImGuiWindowFlags_NoMove;
        } else if (location == -2) {
            // Center window
            ImGui::SetNextWindowPos( ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
            window_flags |= ImGuiWindowFlags_NoMove;
        }
        ImGui::SetNextWindowBgAlpha( 0.6f );// Transparent background
        if (ImGui::Begin( "Performance Overlay", nullptr, window_flags )) {
            ImGui::Separator();
            if (ImGui::IsMousePosValid()) {
                ImGui::Text( "Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y );
            }
            else {
                ImGui::Text( "Mouse Position: <invalid>" );
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text( "FPS: %.1f", 1.0f / timeStep );

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text( "TimeStep: %.1f ms", timeStep * 1000.0f);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text( "Total lights: %d", m_EditorState->ActiveEditorScene->GetLightCount());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text( "Total active lights: %d", m_EditorState->ActiveEditorScene->GetActiveLightCount());

            if (ImGui::BeginPopupContextWindow()) {
                if (ImGui::MenuItem( "Custom", nullptr, location == -1 )) location = -1;
                if (ImGui::MenuItem( "Center", nullptr, location == -2 )) location = -2;
                if (ImGui::MenuItem( "Top-left", nullptr, location == 0 )) location = 0;
                if (ImGui::MenuItem( "Top-right", nullptr, location == 1 )) location = 1;
                if (ImGui::MenuItem( "Bottom-left", nullptr, location == 2 )) location = 2;
                if (ImGui::MenuItem( "Bottom-right", nullptr, location == 3 )) location = 3;
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    auto ScenePanel::OnUpdate( float ts ) -> void {
        if (!m_PanelIsVisible) {
            return;
        }

        constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };

        // Expand scene view to window bounds (no padding)
        ImGuiUtils::ImGuiScopedStyleVar winPad ( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );

        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

        m_PanelIsFocused = ImGui::IsWindowFocused();
        m_PanelIsHovered = ImGui::IsWindowHovered();

        if (IsDisplayTextureValid()) {
            UpdateViewport();

            //DrawSceneToolbar();
            //ShowStatsOverlay(ts);

            SetupManipulation();
            DrawManipulationGuizmos();
        }

        // Try validating the image id again in case the texture was recreated
        if (!IsDisplayTextureValid()) {
            CreateImguiTextureID();
            CreateWireframeImguiTextureID();
        }

        ImGui::End();
    }

    auto ScenePanel::SetManipulation( GuizmoType mode ) -> void { m_GuizmoType = mode; }

    auto ScenePanel::GetWidth() const -> float { return m_ViewPortWidth; }

    auto ScenePanel::GetHeight() const -> float { return m_ViewPortHeight; }

    auto ScenePanel::IsDisplayTextureValid() const -> bool {
        return m_ColorImageID != 0 || m_WireframeImageID != 0;
    }

    auto ScenePanel::UpdateViewport() -> void {
        const ImVec2 dim{ ImGui::GetContentRegionAvail() };

        if (m_ViewPortWidth != dim.x || m_ViewPortHeight != dim.y) {
            m_ViewPortWidth = dim.x;
            m_ViewPortHeight = dim.y;
        }

        // No flipping, the final image is already in the correct viewport coordinates
        // In the case of vulkan this is also taken into account when setting up the vierwport
        if (!m_EditorState->ShowWireframe) {
            ImGui::Image( m_ColorImageID, ImVec2{ dim.x, dim.y } );
        } else {
            ImGui::Image( m_WireframeImageID, ImVec2{ dim.x, dim.y } );
        }
    }

    auto ScenePanel::SetupManipulation() const -> void {
        Entity *currentSelection{ m_EditorState->SelectedEntity };
        if (currentSelection != nullptr && currentSelection->IsValid()) {
            if (!currentSelection->GetComponent<TagComponent>().IsActive()) { return; }

            ImGuizmo::SetOrthographic( m_EditorState->EditorCamera->IsOrthographic() );
            ImGuizmo::SetDrawlist();

            const ImVec2 windowPosition{ ImGui::GetWindowPos() };
            const ImVec2 windowDimensions{ ImGui::GetWindowSize() };
            ImGuizmo::SetRect( windowPosition.x, windowPosition.y, windowDimensions.x, windowDimensions.y );
        }
    }

    auto ScenePanel::DrawManipulationGuizmos() -> void {
        Entity *currentSelection{ m_EditorState->SelectedEntity };
        if (currentSelection == nullptr || !currentSelection->IsValid() || !currentSelection->GetComponent<TagComponent>().IsActive()) {
            return;
        }

        TransformComponent &transformComponent{ currentSelection->GetComponent<TransformComponent>() };

        const glm::mat4 &cameraView{ m_EditorState->EditorCamera->GetViewMatrix() };
        const glm::mat4 &cameraProjection{ m_EditorState->EditorCamera->GetProjection() };

        glm::mat4 objectTransform{ transformComponent.GetTransform() };

        m_GuizmoType = InferManipulationMode( m_EditorState->Manipulation );

        switch (m_GuizmoType) {
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

        if (ImGuizmo::IsUsing()) {
            transformComponent.SetTransform( objectTransform );

            // Apply the transformation to the children
            // For now Guizmos only change translation so thats the only thing we handle in the children
            RelationComponent &relation{ m_EditorState->SelectedEntity->GetComponent<RelationComponent>() };
            for (auto &childID: relation.GetChildren()) {
                Entity *child{ m_EditorState->ActiveEditorScene->FindByID( childID ) };
                if (child) {
                    // TODO: World transform = ParentWorld * LocalTransform
                }
            }

            //propagate changes
        }
    }

    auto ScenePanel::DrawSceneToolbar() const -> void {
        // Static so user dragging persists
        static bool firstFrame{ true };
        static ImVec2 toolbarPos{};

        const ImVec2 windowPos{ ImGui::GetWindowPos() };
        const ImVec2 windowSize{ ImGui::GetWindowSize() };

        // Initial center positioning
        if (firstFrame) {
            toolbarPos = ImVec2{
                windowPos.x + windowSize.x * 0.5f - 70.0f,
                windowPos.y + 20.0f
            };
            firstFrame = false;
        }

        ImGui::SetNextWindowPos( toolbarPos, ImGuiCond_Always );
        ImGui::SetNextWindowBgAlpha( 0.20f );// transparent inner

        // Styles
        ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.20f } );// inner transparent
        ImGui::PushStyleColor( ImGuiCol_Border, ImVec4{ 0.0f, 0.0f, 0.0f, 1.00f } );  // opaque border

        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 6.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1.3f );
        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, 4.0f );

        ImGuiWindowFlags flags{
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking
        };

        if (ImGui::Begin( "SceneToolsOverlay", nullptr, flags )) {
            // Drag anywhere inside window
            if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging( ImGuiMouseButton_Left )) {
                const ImVec2 delta{ ImGui::GetIO().MouseDelta };
                toolbarPos.x += delta.x;
                toolbarPos.y += delta.y;
            }

            auto makeTool = [&]( const char *icon, GuizmoType type ) {
                const bool active{
                    m_EditorState->Manipulation ==
                    static_cast<ImGuiUtils::GuizmoManipulationMode>( type )
                };

                const ImVec2 btnSize{ 28.0f, 28.0f };
                const ImVec2 iconPadding{ 2.0f, 2.0f };

                if (active) ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0.75f, 0.75f, 0.75f, 0.85f } );

                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, iconPadding );

                if (ImGui::Button( icon, btnSize )) {
                    m_EditorState->Manipulation =
                            static_cast<ImGuiUtils::GuizmoManipulationMode>( type );
                }

                ImGui::PopStyleVar();

                if (active) ImGui::PopStyleColor();

                ImGui::SameLine();
            };

            // Extra spacing on first button
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 6.0f, 0.0f } );
            makeTool( ICON_MD_OPEN_WITH, GuizmoType::TRANSLATION );
            ImGui::PopStyleVar();

            makeTool( ICON_MD_ROTATE_RIGHT, GuizmoType::ROTATION );
            makeTool( ICON_MD_OPEN_IN_FULL, GuizmoType::SCALE );

            ImGui::NewLine();
        }

        ImGui::End();

        ImGui::PopStyleVar( 3 );
        ImGui::PopStyleColor( 2 );
    }


}// namespace Mikoto