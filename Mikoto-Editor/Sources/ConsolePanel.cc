//
// Created by kate on 10/12/23.
//

#include <ImGui/IconsMaterialDesign.h>
#include <imgui.h>

#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Panels/ConsolePanel.hh>

namespace Mikoto {

    ConsolePanel::ConsolePanel(const ConsolePanelCreateInfo& info)
        : Panel{ "Console" }, m_State{ info.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName(ICON_MD_TERMINAL, m_PanelName);
    }

    auto ConsolePanel::OnUpdate(float timeStep) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible), ImGuiWindowFlags_NoCollapse);

        // Filtering options
        static bool showInfo{ true }, showWarn{ true }, showError{ true }, showDebug{ true };

        ImGui::Checkbox("Info", &showInfo);
        ImGui::SameLine();
        ImGui::Checkbox("Warn", &showWarn);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &showError);
        ImGui::SameLine();
        ImGui::Checkbox("Debug", &showDebug);

        ImGui::Separator();

        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false,
                          ImGuiWindowFlags_HorizontalScrollbar);

        const auto& logs{ RuntimeConsole::Get()->GetLogs() };
        for (const auto& line : logs) {
            if (line.find("[INFO]") != std::string::npos && !showInfo) continue;
            if (line.find("[WARN]") != std::string::npos && !showWarn) continue;
            if (line.find("[ERROR]") != std::string::npos && !showError) continue;
            if (line.find("[DEBUG]") != std::string::npos && !showDebug) continue;

            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (line.find("[ERROR]") != std::string::npos) {
                color = { 1.0f, 0.3f, 0.3f, 1.0f };
            }
            else if (line.find("[WARN]") != std::string::npos) {
                color = { 1.0f, 0.8f, 0.3f, 1.0f };
            }
            else if (line.find("[DEBUG]") != std::string::npos) {
                color = { 0.5f, 0.8f, 1.0f, 1.0f };
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }

        if (m_ScrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
        }

        m_ScrollToBottom = false;

        ImGui::EndChild();
        ImGui::Separator();

        // Input text field
        static char inputBuffer[256]{ "" };

        if (ImGui::InputText("##ConsoleInput", inputBuffer, sizeof(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string input{ inputBuffer };
            if (!input.empty()) {
                RuntimeConsole::Get()->ExecuteCommand(input);
                inputBuffer[0] = '\0';
                m_ScrollToBottom = true;
            }
        }

        ImGui::End();
    }

} // namespace Mikoto