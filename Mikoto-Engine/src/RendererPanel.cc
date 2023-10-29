//
// Created by kate on 10/13/23.
//

#include "imgui.h"
#include "imgui_internal.h"


#include "../Common/StringUtils.hh"
#include "Common/RenderingUtils.hh"
#include "GUI/ImGuiManager.hh"
#include "Renderer/Renderer.hh"

#include "GUI/IconsFontAwesome5.h"
#include "GUI/IconsMaterialDesign.h"
#include "GUI/IconsMaterialDesignIcons.h"
#include "Panels/RendererPanel.hh"

namespace Mikoto {
    static constexpr auto GetRendererPanel() -> std::string_view {
        return "Renderer";
    }

    RendererPanel::RendererPanel() {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_SETTINGS_DISPLAY, GetRendererPanel());
    }

    auto RendererPanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            static constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar };

            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), windowFlags);


             ImGui::End();
        }
    }
}