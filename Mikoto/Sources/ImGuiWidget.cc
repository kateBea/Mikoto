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

    auto widget::MakeIconTitle( eastl::string_view panelIcon, eastl::string_view panelName ) -> eastl::string {
        return string::Format( "{} {}", panelIcon.data(), panelName.data() );
    }

    auto widget::MakeHelpPopUp( eastl::string_view description, eastl::string_view placeHolder ) -> void {
        ImGui::TextDisabled( "%s", placeHolder.data() );

        if (ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort ) && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos( ImGui::GetFontSize() * 35.0f );

            ImGui::TextUnformatted( description.data() );

            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
}// namespace mikoto