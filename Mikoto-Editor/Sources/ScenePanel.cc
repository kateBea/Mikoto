//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

// Important to include after imgui
#include <ImGuizmo.h>
#include <ImOGuizmo.hh>

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <Common/Common.hh>
#include <Math/Math.hh>
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
        if (const ImTextureID id{ backend->ConstructImGuiTextureID( m_EditorState->FinalComposition ) }; id != 0) {
            m_ColorImageID = id;
        }
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
            //ShowUtilitiesOverlay();

            SetupManipulation();
            DrawManipulationGuizmos();
            DrawOrientationAxis();
        }

        // Try validating the image id again in case the texture was recreated
        if (!IsDisplayTextureValid()) {
            CreateImguiTextureID();
            CreateWireframeImguiTextureID();
        }

        ImGui::End();
    }

    auto ScenePanel::SetManipulation( GuizmoType mode ) -> void {
        m_GuizmoType = mode;
    }

    auto ScenePanel::GetWidth() const -> float {
        return m_ViewPortWidth;
    }

    auto ScenePanel::GetHeight() const -> float {
        return m_ViewPortHeight;
    }

    auto ScenePanel::DrawOrientationAxis() -> void {
        ImVec2 winPos{ ImGui::GetWindowPos() };

        const float widgetSize{ 100.0f };
        const float gizmoX{ winPos.x };
        const float gizmoY{ winPos.y + m_ViewPortHeight - widgetSize };

        ImOGuizmo::SetRect(gizmoX, gizmoY, widgetSize);

        glm::mat4 view{ m_EditorState->EditorCamera->GetViewMatrix() };
        glm::mat4 proj{ glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f) };

        ImOGuizmo::DrawGizmo(glm::value_ptr(view), glm::value_ptr(proj));
    }

    auto ScenePanel::ShowUtilitiesOverlay() -> void {
    }

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
            if (!currentSelection->GetComponent<TagComponent>().IsActive()) {
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