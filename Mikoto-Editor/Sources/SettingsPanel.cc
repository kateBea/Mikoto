//    Copyright 2026 ケイト
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

#include <ImGui/IconsMaterialDesign.h>

#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>

#include <Core/Profiler.hh>
#include <Library/String/String.hh>
#include <Panels/SettingsPanel.hh>

namespace Mikoto {

    auto SettingsPanel::DrawCameraConfig() -> void {
        static const std::array<std::string, 2> cameraProjectionTypesStr{
            "Orthographic", "Perspective"
        };

        SceneCamera &sceneCamera{ *m_EditorState->EditorCamera };
        const ProjectionType cameraCurrentProjectionType{ sceneCamera.GetProjectionType() };

        const std::string &currentProjectionTypeStr{ cameraProjectionTypesStr[static_cast<UInt32>( cameraCurrentProjectionType )] };

        ImGui::Spacing();
        if ( ImGui::BeginCombo( "##SettingsPanel::OnUpdate::EditorCam:Projection", currentProjectionTypeStr.c_str() ) ) {
            UInt32 projectionIndex{};

            for ( const std::string &projectionType: cameraProjectionTypesStr ) {
                const bool isSelected{ projectionType == cameraProjectionTypesStr[static_cast<UInt32>( cameraCurrentProjectionType )] };

                if ( ImGui::Selectable( fmt::format( " {}", projectionType ).c_str(), isSelected ) ) {
                    sceneCamera.SetProjectionType( static_cast<ProjectionType>( projectionIndex ) );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                if ( isSelected ) {
                    ImGui::SetItemDefaultFocus();
                }

                ++projectionIndex;
            }

            ImGui::EndCombo();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        if ( sceneCamera.GetProjectionType() == ProjectionType::PERSPECTIVE ) {
            ImGui::Spacing();
            ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::CameraSpeed", m_Data.EditorCameraMovementSpeed, { 2, 100 } );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Adjust camera movement speed" );

            ImGui::Spacing();
            ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::RotationSpeed", m_Data.EditorCameraRotationSpeed, { 3, 200 } );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Adjust camera rotation speed." );

            ImGui::Spacing();
            if ( ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::Damping", m_Data.DampingFactor, { 5, 30 } ) ) { m_EditorState->EditorCamera->SetDampingFactor( m_Data.DampingFactor ); }
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Adjust camera smooth damping factor." );

            ImGui::Spacing();
            ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::NearClip", m_Data.NearPlane, { -10.0f, 100.0f } );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Adjust camera near plane." );

            ImGui::Spacing();
            ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::FarClip", m_Data.FarPlane, { 5, 200000 } );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Adjust camera far clip." );

            ImGui::Spacing();
            ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::FieldOfView", m_Data.FieldOfView, { 10, 100 } );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Adjust camera field of view." );

            ImGui::Spacing();
            ImGuiUtils::CheckBox( "Lock Rotation ( X )", m_Data.WantXAxisRotation );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Lock rotation in the X axis. Cannot look from top to bottom and viceversa." );

            ImGui::Spacing();
            ImGuiUtils::CheckBox( "Lock Rotation ( Y )", m_Data.WantYAxisRotation );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Lock rotation in the X axis. Cannot look from left to right and viceversa." );

            // Heatmap
            ImGui::Spacing();
            ImGuiUtils::CheckBox( "Show heatmap", m_EditorState->ShowHeatMap );
            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Visualize heatmap for clusters" );
        }

        if ( sceneCamera.GetProjectionType() == ProjectionType::ORTHOGRAPHIC ) {}
    }

    auto SettingsPanel::DrawInfiniteGridConfig() -> void {
        ImGui::Spacing();
        ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::GridSize", m_Data.GridSize, { 5, 100 } );
        ImGui::SameLine();
        ImGuiUtils::HelpMarker( "Adjust infinite grid size." );

        ImGui::Spacing();
        ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::GridCellSize", m_Data.GridCellSize, { 5, 100 } );
        ImGui::SameLine();
        ImGuiUtils::HelpMarker( "Adjust infinite grid cell size." );
    }

    SettingsPanel::SettingsPanel( const SettingsPanelCreateInfo &data )
        : Panel{ "Settings" }, m_EditorState( data.State ) {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_CONSTRUCTION, m_PanelName );
    }

    auto SettingsPanel::OnUpdate( float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

        ImGuiUtils::DrawNode( "Camera", [this] () -> void {
            DrawCameraConfig();
        } );

        ImGuiUtils::DrawNode( "Infinite grid", [this] () -> void {
            DrawInfiniteGridConfig();
        } );

        ImGui::End();
    }
}// namespace Mikoto