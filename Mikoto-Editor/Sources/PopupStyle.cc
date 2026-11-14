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

#include <Theme/PopupStyle.hh>

namespace mikoto::editor {

    PopupStyle::PopupStyle() {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 14.0f, 14.0f } );
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2{ 10.0f, 8.0f } );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 4.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_PopupRounding, 4.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );
        ImGui::PushStyleColor( ImGuiCol_Border, ImVec4{ 0.07f, 0.07f, 0.07f, 1.0f } );
    }

    PopupStyle::~PopupStyle() {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar( 6 );
    }
}
