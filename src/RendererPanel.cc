//
// Created by kate on 10/13/23.
//

#include <imgui.h>
#include <imgui_internal.h>


#include <Utility/StringUtils.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <ImGui/ImGuiManager.hh>

#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>
#include <Editor/RendererPanel.hh>

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