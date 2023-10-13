//
// Created by kate on 10/12/23.
//

#include <memory>
#include <string_view>

#include <imgui.h>

#include <Utility/Common.hh>
#include <Utility/StringUtils.hh>
#include <Editor/ConsolePanel.hh>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesignIcons.h>
#include <ImGui/ImGuiManager.hh>

namespace Mikoto {
    static constexpr auto GetConsolePanelName() -> std::string_view {
        return "Console";
    }

    ConsolePanel::ConsolePanel() {
        m_PanelHeaderName = MakePanelName(ICON_MD_DESKTOP_MAC, GetConsolePanelName());
    }

    auto ConsolePanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible));

            const float cursorPosX{ ImGui::GetCursorPosX() };
            m_SearchFilter.Draw("###ContentBrowserFilter", ImGui::GetContentRegionAvail().x);
            if (!m_SearchFilter.IsActive()) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(cursorPosX + ImGui::GetFontSize() * 0.5f);

                // TODO: grab the color from text color and lower alpha value
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,128));
                ImGui::TextUnformatted(fmt::format("{} Search...", ICON_MD_SEARCH).c_str());
                ImGui::PopStyleColor();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            ImGui::Text(R"(
            Console Commands:
            Developers can define various console commands that can manipulate game parameters, trigger specific events, or toggle features.

            Script Execution:
            Allow for executing scripts or script-like commands directly from the console. This enables rapid prototyping and testing of game logic.
            )");
            ImGui::PopStyleColor();

            ImGui::End();
        }
    }
}