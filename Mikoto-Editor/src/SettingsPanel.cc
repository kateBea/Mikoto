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
#include <Core/System/RenderSystem.hh>
#include <GUI/ImGuiUtils.hh>

#include "GUI/IconsMaterialDesign.h"
#include "Library/String/String.hh"
#include "Panels/SettingsPanel.hh"
#include "Renderer/Core/RenderCommand.hh"
#include "Renderer/Core/RenderContext.hh"

namespace Mikoto {

    static constexpr auto GetSettingsPanelName() -> std::string_view {
        return "Settings";
    }

    SettingsPanel::SettingsPanel()
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_CONSTRUCTION, GetSettingsPanelName());

        m_Data.EditorCameraMovementSpeed = SceneCamera::GetMinMovementSpeed();
        m_Data.EditorCameraRotationSpeed = SceneCamera::GetMinRotationSpeed();

        m_Data.NearPlane = SceneCamera::GetMinNearClip();
        m_Data.FarPlane = SceneCamera::GetMaxNearClip();
    }

    SettingsPanel::SettingsPanel(const SettingsPanelCreateInfo& data)
        :   m_Data{ data.Data }
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_CONSTRUCTION, GetSettingsPanelName());
    }

    auto SettingsPanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), ImGuiWindowFlags_NoCollapse);

            constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding};

            static const std::array<std::string, 2> cameraProjectionTypesStr{
                "Orthographic", "Perspective"
            };

            SceneCamera& sceneCamera{ *m_Data.EditorCamera };
            const auto cameraCurrentProjectionType{ sceneCamera.GetProjectionType() };

            if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "SettingsPanel::OnUpdate::EditorCam" ), styleFlags, "%s", "Camera")) {
                // Perspective
                const std::string& currentProjectionTypeStr{ cameraProjectionTypesStr[cameraCurrentProjectionType] };

                // Handle type of projection
                ImGui::Spacing();
                if ( ImGui::BeginCombo( "##SettingsPanel::OnUpdate::EditorCam:Projection",currentProjectionTypeStr.c_str() ) ) {
                    UInt32_T projectionIndex{};

                    for ( const std::string& projectionType : cameraProjectionTypesStr ) {
                        const bool isSelected{ projectionType == cameraProjectionTypesStr[cameraCurrentProjectionType] };

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

                // Manage perspective settings
                if (sceneCamera.GetProjectionType() == PERSPECTIVE) {
                    // Movement
                ImGui::Spacing();
                ImGui::SliderFloat("##SettingsPanel::OnUpdate::CameraSpeed", std::addressof(m_Data.EditorCameraMovementSpeed), SceneCamera::GetMinMovementSpeed(), SceneCamera::GetMaxMovementSpeed());
                ImGui::SameLine(  );
                ImGuiUtils::HelpMarker( "Adjust camera movement speed" );

                ImGui::Spacing();
                ImGui::SliderFloat("##SettingsPanel::OnUpdate::RotationSpeed", std::addressof(m_Data.EditorCameraRotationSpeed), SceneCamera::GetMinRotationSpeed(), SceneCamera::GetMaxRotationSpeed());
                ImGui::SameLine(  );
                ImGuiUtils::HelpMarker( "Adjust camera rotation speed." );

                // Planes
                ImGui::Spacing();
                ImGui::SliderFloat("##SettingsPanel::OnUpdate::NearClip", std::addressof(m_Data.NearPlane), SceneCamera::GetMinNearClip(), SceneCamera::GetMaxNearClip());
                ImGui::SameLine(  );
                ImGuiUtils::HelpMarker( "Adjust camera near plane." );

                ImGui::Spacing();
                ImGui::SliderFloat("##SettingsPanel::OnUpdate::FarClip", std::addressof(m_Data.FarPlane), SceneCamera::GetMinFarClip(), SceneCamera::GetMaxFarClip());
                ImGui::SameLine(  );
                ImGuiUtils::HelpMarker( "Adjust camera far clip." );

                ImGui::Spacing();
                ImGui::SliderFloat("##SettingsPanel::OnUpdate::FieldOfView", std::addressof(m_Data.FieldOfView), SceneCamera::GetMinFov(), SceneCamera::GetMaxFov());
                ImGui::SameLine(  );
                ImGuiUtils::HelpMarker( "Adjust camera field of view." );

                // Rotation
                ImGui::Spacing();
                ImGui::Checkbox("Lock Rotation ( X )", std::addressof(m_Data.WantXAxisRotation));
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Lock rotation in the X axis. Cannot look from top to bottom and viceversa." );

                ImGui::Spacing();
                ImGui::Checkbox("Lock Rotation ( Y )", std::addressof(m_Data.WantYAxisRotation));
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Lock rotation in the X axis. Cannot look from left to right and viceversa." );
                }

                if (sceneCamera.GetProjectionType() == ORTHOGRAPHIC) {

                }

                ImGui::TreePop();
            }

            ImGui::Spacing();
            if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "SettingsPanel::OnUpdate::Rendering" ), styleFlags, "%s", "Rendering")) {

                ImGui::Spacing();
                if (ImGui::Checkbox("Enable Vsync", std::addressof(m_Data.VerticalSyncEnabled))) {
                    RenderSystem& renderSystem{ Engine::GetSystem<RenderSystem>() };

                    if (m_Data.VerticalSyncEnabled) {
                        renderSystem.GetContext()->EnableVSync();
                    } else {
                        renderSystem.GetContext()->DisableVSync();
                    }
                }

                ImGui::Spacing();
                if (ImGui::Checkbox("Wireframe mode", std::addressof(m_Data.RenderWireframeMode))) {
                    RenderContext& renderContext{ *Engine::GetSystem<RenderSystem>().GetContext() };

                    if (m_Data.RenderWireframeMode) {
                        renderContext.EnableVSync();
                    } else {
                        renderContext.DisableVSync();
                    }
                }

                ImGui::TreePop();
            }

            ImGui::Spacing();
            if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "SettingsPanel::OnUpdate::Color" ), styleFlags, "%s", "Clear")) {
                constexpr ImGuiColorEditFlags flags{ImGuiColorEditFlags_None | ImGuiColorEditFlags_PickerHueWheel};

                ImGui::Spacing();
                ImGui::ColorEdit4("Clear Color", value_ptr(m_Data.ClearColor), flags);

                ImGui::TreePop();
            }

            ImGui::End();
        }
    }
}