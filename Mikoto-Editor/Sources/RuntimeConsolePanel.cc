//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_string.h>

#include <imgui.h>
#include <ImGui/IconsMaterialDesign.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Memory/Allocator.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiUtility.hh>

#include <Layers/EditorLayer.hh>

#include <Panels/RuntimeConsolePanel.hh>

namespace mikoto::editor {

    using namespace mikoto::gui;

    RuntimeConsolePanel::RuntimeConsolePanel(const RuntimeConsolePanelCreateInfo& info)
        : Panel{ "Console" }, mState{ info.mState } {
        mPanelHeaderName = widget::MakeIconTitle(ICON_MD_TERMINAL, mPanelName);
    }

    auto RuntimeConsolePanel::OnUpdate(float timeStep) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mPanelIsVisible) {
            return;
        }

        ImGui::Begin(mPanelHeaderName.c_str(), MKT_ADDRESSOF(mPanelIsVisible), ImGuiWindowFlags_NoCollapse);

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

        for (const auto& line : RuntimeConsole::Get()->GetLogs()) {
            if (line.find("[INFO]") != eastl::string::npos && !showInfo) continue;
            if (line.find("[WARN]") != eastl::string::npos && !showWarn) continue;
            if (line.find("[ERROR]") != eastl::string::npos && !showError) continue;
            if (line.find("[DEBUG]") != eastl::string::npos && !showDebug) continue;

            ImVec4 color{ ImGui::GetStyleColorVec4(ImGuiCol_Text) };
            if (line.find("[ERROR]") != eastl::string::npos) {
                color = { 1.0f, 0.3f, 0.3f, 1.0f };
            }
            else if (line.find("[WARN]") != eastl::string::npos) {
                color = { 1.0f, 0.8f, 0.3f, 1.0f };
            }
            else if (line.find("[DEBUG]") != eastl::string::npos) {
                color = { 0.5f, 0.8f, 1.0f, 1.0f };
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }

        if (mScrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
        }

        mScrollToBottom = false;

        ImGui::EndChild();
        ImGui::Separator();

        eastl::array<char, 256> buffer{};
        if (ImGui::InputText("##ConsoleInput", buffer.data(), buffer.size() + 1, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if ( eastl::string input{ buffer.data() }; !input.empty()) {
                RuntimeConsole::Get()->ExecuteCommand( input );

                buffer[0] = '\0';
                mScrollToBottom = true;
            }
        }

        ImGui::End();
    }
} // namespace Mikoto