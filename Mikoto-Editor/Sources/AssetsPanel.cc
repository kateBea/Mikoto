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

#include <fmt/format.h>

#include <Panels/AssetsPanel.hh>

#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <ImGui/IconsMaterialDesign.h>

namespace Mikoto {

    auto AssetsPanel::CreateImguiTextureID() -> void {
        ImGuiBackend *backend{ ImGuiService::Get()->GetBackend() };

        if ( const ImTextureID id{ backend->ConstructImGuiTextureID( m_EditorState->PreviewMaterial ) }; id != 0 ) {
            m_DisplayTargetImGuiID = id;
        }
    }

    auto AssetsPanel::IsDisplayTextureValid() const -> bool {
        return m_DisplayTargetImGuiID != 0;
    }

    auto AssetsPanel::UpdateViewport() -> void {
        const ImVec2 dim{ ImGui::GetContentRegionAvail() };

        if ( m_ViewportWidth != dim.x || m_ViewportHeight != dim.y ) {
            m_ViewportWidth = dim.x;
            m_ViewportHeight = dim.y;
        }

        ImGui::Image( m_DisplayTargetImGuiID, ImVec2{ dim.x, dim.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 } );
    }

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

        CreateImguiTextureID();
    }

    auto AssetsPanel::OnUpdate( float ) -> void {
        if ( m_PanelIsVisible ) {
            constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };

            // Expand scene view to window bounds (no padding)
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );
            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), windowFlags );

            m_PanelIsFocused = ImGui::IsWindowFocused();
            m_PanelIsHovered = ImGui::IsWindowHovered();

            if (IsDisplayTextureValid()) {
                UpdateViewport();
            }

            if ( !IsDisplayTextureValid() ) {
                CreateImguiTextureID();
            }

            ImGui::End();

            ImGui::PopStyleVar();
        }
    }
}// namespace Mikoto