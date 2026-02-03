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

#include <ImGui/IconsMaterialDesign.h>

#include <ImGui/ImGuiUtility.hh>
#include <Panels/RendererPanel.hh>
#include <GraphNodes/GraphEditor.hh>

#include "Common/String.hh"
#include "Layers/EditorLayer.hh"

namespace Mikoto {

    RendererPanel::RendererPanel( const RendererPanelCreateInfo &info )
        : Panel{ "Renderer" }, m_EditorState{ info.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_POWER_SETTINGS_NEW, m_PanelName );
    }

    auto RendererPanel::OnUpdate( float timeStep ) -> void {
        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGuiUtils::DrawNode( "Passes", [this] () -> void {
            DrawPassInfo();

            //static bool open{ true };
            //ShowExampleAppCustomNodeGraph(&open);
        });

        ImGuiUtils::DrawNode( "Renderer", [this] () -> void {
            DrawRendererConfig();
        });

        ImGui::End();
    }

    auto RendererPanel::IsWireframeEnabled() const -> bool {
        return m_IsWireframeEnabled;
    }

    auto RendererPanel::DrawPassInfo() -> void {
        const auto& passList{ m_EditorState->EditorSceneRenderer->GetPassList() };
        ImGui::TextUnformatted( StringUtil::Format("Pass count: {}", passList.size() ).c_str() );
        ImGui::TextUnformatted( StringUtil::Format("Pass List --------").c_str() );

        for (const auto& pass : passList) {
            ImGui::TextUnformatted( StringUtil::Format("{}", pass.first).c_str() );
        }

    }

    auto RendererPanel::DrawRendererConfig() -> void {
        ImGuiUtils::CheckBox( "##RendererPanel::DrawRendererConfig::Checkbox", m_IsWireframeEnabled );
        ImGui::SameLine();
        ImGui::TextUnformatted( "Render wireframe" );
    }
}// namespace Mikoto