/**
 * SettingsPanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <Utility/StringUtils.hh>

#include <Editor/SettingsPanel.hh>
#include <Renderer/RenderCommand.hh>
#include <Renderer/RenderContext.hh>

#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

namespace Mikoto {
    static constexpr auto GetSettingsPanelName() -> std::string_view {
        return "Settings";
    }

    SettingsPanel::SettingsPanel()
        :   Panel{}
    {
        m_PanelHeaderName = MakePanelName(ICON_MD_CONSTRUCTION, GetSettingsPanelName());

        m_Data.EditorCameraMovementSpeed = EditorCamera::GetMinMovementSpeed();
        m_Data.EditorCameraRotationSpeed = EditorCamera::GetMinRotationSpeed();

        for (Size_T count{}; count < REQUIRED_IDS; ++count) {
            m_Guids.emplace_back();
        }
    }

    auto SettingsPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible));

            static constexpr ImGuiTreeNodeFlags styleFlags{ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding};

            if (ImGui::TreeNodeEx((void *) m_Guids[0].Get(), styleFlags, "%s", "Editor Camera")) {
                // Movement
                ImGui::SliderFloat("Movement Speed", std::addressof(m_Data.EditorCameraMovementSpeed), EditorCamera::GetMinMovementSpeed(), EditorCamera::GetMaxMovementSpeed());
                ImGui::SliderFloat("Rotation Speed", std::addressof(m_Data.EditorCameraRotationSpeed), EditorCamera::GetMinRotationSpeed(), EditorCamera::GetMaxRotationSpeed());

                // Planes
                ImGui::SliderFloat("Near Plane", std::addressof(m_Data.NearPlane), EditorCamera::GetMinNearClip(), EditorCamera::GetMaxNearClip());
                ImGui::SliderFloat("Far Plane", std::addressof(m_Data.FarPlane), EditorCamera::GetMinFarClip(), EditorCamera::GetMaxFarClip());
                ImGui::SliderFloat("Field of view", std::addressof(m_Data.FieldOfView), EditorCamera::GetMinFov(), EditorCamera::GetMaxFov());

                // Rotation
                ImGui::Checkbox("Lock X Rotation", std::addressof(m_Data.WantXAxisRotation));
                ImGui::Checkbox("Lock Y Rotation", std::addressof(m_Data.WantYAxisRotation));
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx((void *) m_Guids[1].Get(), styleFlags, "%s", "Rendering")) {
                if (ImGui::Checkbox("Lock Framerate (VSync)", std::addressof(m_Data.VerticalSyncEnabled))) {
                    if (m_Data.VerticalSyncEnabled)
                        RenderContext::EnableVSync();
                    else
                        RenderContext::DisableVSync();
                }

                if (ImGui::Checkbox("Wireframe mode", std::addressof(m_Data.RenderWireframeMode))) {
                    if (m_Data.RenderWireframeMode)
                        // Should be done by the renderer
                        RenderCommand::EnableWireframeMode();
                    else
                        RenderCommand::DisableWireframeMode();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx((void *) m_Guids[2].Get(), styleFlags, "%s", "Color")) {
                static constexpr ImGuiColorEditFlags flags{ImGuiColorEditFlags_None | ImGuiColorEditFlags_PickerHueWheel};
                ImGui::ColorEdit4("Clear Color", glm::value_ptr(m_Data.ClearColor), flags);
                ImGui::TreePop();
            }

            ImGui::End();
        }
    }
}