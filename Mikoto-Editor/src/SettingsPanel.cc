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
#include "STL/String/String.hh"

#include "Panels/SettingsPanel.hh"
#include "Renderer/Core/RenderCommand.hh"
#include "Renderer/Core/RenderContext.hh"
#include "GUI/IconsMaterialDesign.h"

namespace Mikoto {
    static constexpr auto GetSettingsPanelName() -> std::string_view {
        return "Settings";
    }

    SettingsPanel::SettingsPanel()
        :   Panel{}
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_CONSTRUCTION, GetSettingsPanelName());

        m_Data.EditorCameraMovementSpeed = SceneCamera::GetMinMovementSpeed();
        m_Data.EditorCameraRotationSpeed = SceneCamera::GetMinRotationSpeed();

        m_Data.NearPlane = SceneCamera::GetMinNearClip();
        m_Data.FarPlane = 2500.0f;
    }

    auto SettingsPanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), ImGuiWindowFlags_NoCollapse);

            static constexpr ImGuiTreeNodeFlags styleFlags{ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding};

            if (ImGui::TreeNodeEx((void *) 21332323, styleFlags, "%s", "Editor Camera")) {
                // Movement
                ImGui::SliderFloat("Movement Speed", std::addressof(m_Data.EditorCameraMovementSpeed), SceneCamera::GetMinMovementSpeed(), SceneCamera::GetMaxMovementSpeed());
                ImGui::SliderFloat("Rotation Speed", std::addressof(m_Data.EditorCameraRotationSpeed), SceneCamera::GetMinRotationSpeed(), SceneCamera::GetMaxRotationSpeed());

                // Planes
                ImGui::SliderFloat("Near Plane", std::addressof(m_Data.NearPlane), SceneCamera::GetMinNearClip(), SceneCamera::GetMaxNearClip());
                ImGui::SliderFloat("Far Plane", std::addressof(m_Data.FarPlane), SceneCamera::GetMinFarClip(), SceneCamera::GetMaxFarClip());
                ImGui::SliderFloat("Field of view", std::addressof(m_Data.FieldOfView), SceneCamera::GetMinFov(), SceneCamera::GetMaxFov());

                // Rotation
                ImGui::Checkbox("Lock X Rotation", std::addressof(m_Data.WantXAxisRotation));
                ImGui::Checkbox("Lock Y Rotation", std::addressof(m_Data.WantYAxisRotation));
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx((void *) 1231213, styleFlags, "%s", "Rendering")) {
                // TODO: manage through events?? since for Vulkan the swapchain has to be recreated
                if (ImGui::Checkbox("Lock Framerate (VSync)", std::addressof(m_Data.VerticalSyncEnabled))) {
                    if (m_Data.VerticalSyncEnabled)
                        RenderContext::EnableVSync();
                    else
                        RenderContext::DisableVSync();
                }

                if (ImGui::Checkbox("Wireframe mode", std::addressof(m_Data.RenderWireframeMode))) {
                    // TODO: manage through events??
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx((void *) 3225252, styleFlags, "%s", "Color")) {
                static constexpr ImGuiColorEditFlags flags{ImGuiColorEditFlags_None | ImGuiColorEditFlags_PickerHueWheel};
                ImGui::ColorEdit4("Clear Color", glm::value_ptr(m_Data.ClearColor), flags);
                ImGui::TreePop();
            }

            ImGui::End();
        }
    }
}