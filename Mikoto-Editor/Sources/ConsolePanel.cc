//
// Created by kate on 10/12/23.
//

#include "Panels/ConsolePanel.hh"

#include <GUI/Icons/IconsFontAwesome5.h>
#include <GUI/Icons/IconsMaterialDesign.h>
#include <GUI/Icons/IconsMaterialDesignIcons.h>
#include <imgui.h>

#include <Common/Common.hh>
#include <Core/System/GUISystem.hh>
#include <Library/String/String.hh>
#include <Tools/ConsoleManager.hh>
#include <memory>
#include <string_view>

namespace Mikoto {
    static constexpr auto GetConsolePanelName() -> std::string_view {
        return "Console";
    }

    ConsolePanel::ConsolePanel() {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_TERMINAL, GetConsolePanelName());
    }

    auto ConsolePanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), ImGuiWindowFlags_NoCollapse);

            if (ImGui::Button(fmt::format("{} Clear", ICON_MD_DELETE).c_str())) {
                ConsoleManager::ClearMessages();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::SameLine();

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

            ImGui::Spacing();
            ImGui::Spacing();

            // TODO: refresh when there's new messages instead of every single loop
            DisplayMessages();

            ImGui::End();
        }
    }

    auto ConsolePanel::DisplayMessages() -> void {
        const auto& messages{ ConsoleManager::GetMessages() };

        GUISystem& guiSystem{ Engine::GetSystem<GUISystem>() };

        ImGui::PushFont(guiSystem.GetFonts()[GUISystem::GUIFonts::IMGUI_MANAGER_FONT_JET_BRAINS_17]);

        for (const auto& [level, message] : messages) {
            switch (level) {
                case ConsoleLogLevel::CONSOLE_WARNING:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 153, 51, 255));
                    break;
                case ConsoleLogLevel::CONSOLE_DEBUG:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(36, 192, 19, 255));
                    break;
                case ConsoleLogLevel::CONSOLE_INFO:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 255, 255));
                    break;
                case ConsoleLogLevel::CONSOLE_ERROR:
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 50, 0, 255));
                    break;
            }

            ImGui::TextUnformatted(message.c_str());

            ImGui::Spacing();

            ImGui::Separator();

            ImGui::Spacing();
            ImGui::PopStyleColor();
        }

        ImGui::PopFont();
    }
}