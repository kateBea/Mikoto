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

            if (ImGui::TreeNodeEx( reinterpret_cast<const void *>( "SettingsPanel::OnUpdate::EditorCam" ), styleFlags, "%s", "Camera")) {
                // Movement
                ImGui::Spacing();
                ImGui::SliderFloat("Movement Speed", std::addressof(m_Data.EditorCameraMovementSpeed), SceneCamera::GetMinMovementSpeed(), SceneCamera::GetMaxMovementSpeed());

                ImGui::Spacing();
                ImGui::SliderFloat("Rotation Speed", std::addressof(m_Data.EditorCameraRotationSpeed), SceneCamera::GetMinRotationSpeed(), SceneCamera::GetMaxRotationSpeed());

                // Planes
                ImGui::Spacing();
                ImGui::SliderFloat("Near Plane", std::addressof(m_Data.NearPlane), SceneCamera::GetMinNearClip(), SceneCamera::GetMaxNearClip());

                ImGui::Spacing();
                ImGui::SliderFloat("Far Plane", std::addressof(m_Data.FarPlane), SceneCamera::GetMinFarClip(), SceneCamera::GetMaxFarClip());

                ImGui::Spacing();
                ImGui::SliderFloat("Field of view", std::addressof(m_Data.FieldOfView), SceneCamera::GetMinFov(), SceneCamera::GetMaxFov());

                // Rotation
                ImGui::Spacing();
                ImGui::Checkbox("Lock Rotation ( X )", std::addressof(m_Data.WantXAxisRotation));
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Lock rotation in the X axis. Cannot look from top to bottom and viceversa." );

                ImGui::Spacing();
                ImGui::Checkbox("Lock Rotation ( Y )", std::addressof(m_Data.WantYAxisRotation));
                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Lock rotation in the X axis. Cannot look from left to right and viceversa." );

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