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
#include <Core/Application.hh>
#include <Editor/Panels/SettingsPanel.hh>
#include <Renderer/RenderCommand.hh>
#include <Renderer/RenderContext.hh>


namespace Mikoto {
    SettingsPanel::SettingsPanel(const std::shared_ptr<SettingsPanelData> &data, const Path_T &iconPath)
        :   Panel{ iconPath }, m_Data{ data }
    {
        m_PanelIsVisible = true;
        m_PanelIsHovered = false;
        m_PanelIsFocused = false;
    }

    auto SettingsPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin("Settings");
            const ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                ImGuiTreeNodeFlags_AllowItemOverlap |
                                                ImGuiTreeNodeFlags_Framed |
                                                ImGuiTreeNodeFlags_SpanAvailWidth |
                                                ImGuiTreeNodeFlags_FramePadding };

            if (ImGui::TreeNodeEx((void*)123213, styleFlags, "%s", "Editor Camera")) {
                ImGui::SliderFloat("Movement Speed", &(m_Data->EditorCameraMovementSpeed), 2.0f, 10.0f);
                ImGui::SliderFloat("Rotation Speed", &(m_Data->EditorCameraRotationSpeed), 2.0f, 10.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx((void*)123215, styleFlags, "%s", "Rendering")) {
                if (ImGui::Checkbox("VSync Enabled", &m_Data->VerticalSyncEnabled)) {
                    if (m_Data->VerticalSyncEnabled)
                        RenderContext::EnableVSync();
                    else
                        RenderContext::DisableVSync();
                }

                if (ImGui::Checkbox("Wireframe mode", &m_Data->RenderWireframeMode)) {
                    if (m_Data->RenderWireframeMode)
                        RenderCommand::EnableWireframeMode();
                    else
                        RenderCommand::DisableWireframeMode();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx((void*)123216, styleFlags, "%s", "Color")) {
                const ImGuiColorEditFlags flags{ ImGuiColorEditFlags_None | ImGuiColorEditFlags_PickerHueWheel };
                ImGui::ColorEdit4("Clear Color", glm::value_ptr(m_Data->ClearColor), flags);
                ImGui::TreePop();
            }

            ImGui::End();
        }
    }

    auto SettingsPanel::OnEvent(Event &event) -> void {

    }
}