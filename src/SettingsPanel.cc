//
// Created by kate on 6/27/23.
//

#include "imgui.h"

#include "glm/gtc/type_ptr.hpp"

#include "Core/Application.hh"
#include "Editor/Panels/SettingsPanel.hh"
#include "Renderer/RenderCommand.hh"
#include "Renderer/RenderContext.hh"


namespace Mikoto {

    SettingsPanel::SettingsPanel(const std::shared_ptr<SettingsPanelData> &data, const Path_T &iconPath)
        :   Panel{ iconPath }, m_Visible{ true }, m_Hovered{ false }, m_Focused{ false }, m_Data{ data }
    {}

    auto SettingsPanel::OnUpdate() -> void {
        if (m_Visible) {
            ImGui::Begin("Settings");
            ImGui::ColorEdit3("Clear Color", glm::value_ptr(m_Data->ClearColor));

            ImGui::Separator();
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

            ImGui::End();
        }
    }

    auto SettingsPanel::OnEvent(Event &event) -> void {

    }
}