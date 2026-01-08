/**
 * SettingsPanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "glm/gtc/type_ptr.hpp"
#include "imgui.h"

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>

#include "Library/String/String.hh"
#include "Panels/SettingsPanel.hh"

namespace Mikoto {

    SettingsPanel::SettingsPanel( const SettingsPanelCreateInfo &data )
        : Panel{ "Settings" }, m_EditorState( data.State ) {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_CONSTRUCTION, m_PanelName );
    }

    auto SettingsPanel::OnUpdate( float timeStep ) -> void {
        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

        constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                 ImGuiTreeNodeFlags_Framed |
                                                 ImGuiTreeNodeFlags_SpanAvailWidth |
                                                 ImGuiTreeNodeFlags_FramePadding };

        static const std::array<std::string, 2> cameraProjectionTypesStr{
            "Orthographic", "Perspective"
        };

        SceneCamera &sceneCamera{ *m_EditorState->EditorCamera };
        const ProjectionType cameraCurrentProjectionType{ sceneCamera.GetProjectionType() };

        if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "SettingsPanel::OnUpdate::EditorCam" ), styleFlags, "%s", "Camera" )) {
            // Perspective
            const std::string &currentProjectionTypeStr{ cameraProjectionTypesStr[static_cast<UInt32>( cameraCurrentProjectionType )] };

            // Handle type of projection
            ImGui::Spacing();
            if (ImGui::BeginCombo( "##SettingsPanel::OnUpdate::EditorCam:Projection", currentProjectionTypeStr.c_str() )) {
                UInt32 projectionIndex{};

                for (const std::string &projectionType: cameraProjectionTypesStr) {
                    const bool isSelected{ projectionType == cameraProjectionTypesStr[static_cast<UInt32>( cameraCurrentProjectionType )] };

                    if (ImGui::Selectable( fmt::format( " {}", projectionType ).c_str(), isSelected )) { sceneCamera.SetProjectionType( static_cast<ProjectionType>( projectionIndex ) ); }

                    if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

                    if (isSelected) { ImGui::SetItemDefaultFocus(); }

                    ++projectionIndex;
                }

                ImGui::EndCombo();
            }

            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

            // Manage perspective settings
            if (sceneCamera.GetProjectionType() == ProjectionType::PERSPECTIVE) {
                // Movement
                ImGui::Spacing();
                ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::CameraSpeed", m_Data.EditorCameraMovementSpeed, { 2, 100 } );
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Adjust camera movement speed" );

                ImGui::Spacing();
                ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::RotationSpeed", m_Data.EditorCameraRotationSpeed, { 3, 200 } );
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Adjust camera rotation speed." );

                ImGui::Spacing();
                if (ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::Damping", m_Data.DampingFactor, { 5, 30 } )) { m_EditorState->EditorCamera->SetDampingFactor( m_Data.DampingFactor ); }
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Adjust camera smooth damping factor." );

                // Planes
                ImGui::Spacing();
                ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::NearClip", m_Data.NearPlane, { 0.1f, 2.0f } );
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

                // Rotation
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
                ImGuiUtils::CheckBox( "Show heatmap", m_EditorState->HeatMapVisualizer );
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Visualize heatmap for clusters" );

                // Outline
                ImGui::Spacing();
                ImGuiUtils::Slider( "##SettingsPanel::OnUpdate::Outline", m_Data.Outline, { 0.5f, 2.0f } );
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Adjust selection outline width." );

                ImGui::Spacing();
                if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "##SettingsPanel::OnUpdate::OutlineColor" ), styleFlags, "%s", "Outline" )) {
                    constexpr ImGuiColorEditFlags flags{ ImGuiColorEditFlags_None | ImGuiColorEditFlags_PickerHueWheel };

                    ImGui::Spacing();
                    ImGui::ColorEdit4( "Outline Color", value_ptr( m_Data.OutlineColor ), flags );

                    ImGui::TreePop();
                }
            }

            if (sceneCamera.GetProjectionType() == ProjectionType::ORTHOGRAPHIC) {}

            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "SettingsPanel::OnUpdate::Rendering" ), styleFlags, "%s", "Rendering" )) {
            ImGui::Spacing();
            if (ImGuiUtils::CheckBox( "Vertical Sync", m_Data.VerticalSyncEnabled )) {}

            ImGui::Spacing();
            ImGuiUtils::CheckBox( "Wireframe render", m_Data.RenderWireframeMode );

            ImGui::TreePop();
        }

        ImGui::Spacing();
        if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "SettingsPanel::OnUpdate::Color" ), styleFlags, "%s", "Clear" )) {
            constexpr ImGuiColorEditFlags flags{ ImGuiColorEditFlags_None | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB };

            ImGui::Spacing();
            ImGui::ColorEdit4( "Clear Color", value_ptr( m_Data.ClearColor ), flags );

            ImGui::TreePop();
        }

        ImGui::End();

    }
}// namespace Mikoto