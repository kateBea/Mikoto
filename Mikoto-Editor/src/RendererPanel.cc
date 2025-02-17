//
// Created by kate on 10/13/23.
//

#include "imgui.h"
#include "Panels/RendererPanel.hh"

#include "GUI/IconsMaterialDesign.h"
#include "GUI/ImGuiManager.hh"

namespace Mikoto {
    // TODO: move to single place and reuse in other panels the same way
    static constexpr auto GetRendererPanel() -> std::string_view {
        return "Renderer";
    }

    RendererPanel::RendererPanel() {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_SETTINGS_DISPLAY, GetRendererPanel());
    }

    auto RendererPanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            static constexpr ImGuiWindowFlags windowFlags{
                ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar };

            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), windowFlags);

            ImGui::End();
        }
    }
}