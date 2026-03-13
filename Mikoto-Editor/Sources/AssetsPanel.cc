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

#include <Common/Common.hh>
#include <Panels/AssetsPanel.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>

namespace Mikoto {

    AssetsPanel::AssetsPanel( const AssetsPanelDescription &description )
        : Panel{ "Assets" }, m_EditorState{ description.State }
    {
        // Icons for ICON_MD for assets U+F1B2, U+F1B3, U+F6D1
        // TODO: find the actual ICON_MD macros
        // U+F6D1  ->  63185
        // U+F1B2  ->  61874
        // U+F1B3  ->  61875
        const std::string icon { ImGuiUtils::GetStringFromUnicode( 61875 ) };

        m_PanelHeaderName = ImGuiUtils::MakePanelName( fmt::format("{}", icon), m_PanelName );
    }

    auto AssetsPanel::OnUpdate( float ) -> void {
        if ( !m_PanelIsVisible ) {
            return;
        }

        constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };
        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

        m_PanelIsFocused = ImGui::IsWindowFocused();
        m_PanelIsHovered = ImGui::IsWindowHovered();

        ImGui::End();
    }
}