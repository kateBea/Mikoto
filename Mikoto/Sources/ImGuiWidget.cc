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

#include <imgui.h>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <ImGui/ImGuiWidget.hh>

namespace mikoto::gui {

    using namespace mikoto::core;

    auto widget::MakeIconTitle( eastl::string_view panelIcon, eastl::string_view panelName ) -> eastl::string {
        return string::Format( "{} {}", panelIcon.data(), panelName.data() );
    }

    auto widget::MakeHelpPopUp( eastl::string_view description, eastl::string_view placeHolder ) -> void {
        if (placeHolder.size() != 0) {
            ImGui::TextDisabled( "%s", placeHolder.data() );
        }

        if (ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort ) && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );

            ImGui::TextUnformatted( description.data() );

            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    auto widget::MakeHelpPopUpDelay(eastl::string_view description, eastl::string_view placeHolder, float duration) -> void {
        if (!placeHolder.empty()) {
            ImGui::TextDisabled("%.*s", (int)placeHolder.size(), placeHolder.data());
        }

        // These track timer state
        static ImGuiID lastItemId{ 0 };
        static f64 hoverStartTime{ 0.0 };

        ImGuiID currentItemId{ ImGui::GetItemID() };

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            f64 currentTime{ ImGui::GetTime() };

            // First time hovering or change element
            if (lastItemId != currentItemId || hoverStartTime == 0.0) {
                lastItemId = currentItemId;
                hoverStartTime = currentTime;
            }

            // How much time hovered
            f64 elapsed{ currentTime - hoverStartTime };

            // Only show tooltip text if elapsed lower active time (duration)
            if (elapsed < as<f64>( duration ) ) {
                if (ImGui::BeginTooltip()) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);

                    ImGui::TextUnformatted(description.data(), description.data() + description.size());

                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }
        } else {
            if (lastItemId == currentItemId) {
                hoverStartTime = 0.0;
            }
        }
    }

}// namespace mikoto