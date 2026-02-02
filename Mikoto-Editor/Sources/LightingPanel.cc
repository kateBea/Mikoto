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

#include <array>

#include <Common/String.hh>

#include <Layers/EditorLayer.hh>
#include <Panels/LightingPanel.hh>

#include <ImGui/IconsMaterialDesign.h>

#include <ImGui/ImGuiUtility.hh>

namespace Mikoto {

    LightingPanel::LightingPanel( const LightingPanelCreateInfo &info )
    : Panel{ "Lighting" }, m_EditorState{ info.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_FLASHLIGHT_ON, m_PanelName );
    }

    auto LightingPanel::OnUpdate( float timeStep ) -> void {
        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGuiUtils::DrawNode( "Background", [this] () -> void {
            DrawBackgroundSettings();
        });

        ImGui::End();
    }

    auto LightingPanel::DrawBackgroundSettings() -> void {
        static  std::array<std::string, 2> backgroundTypes{
            "Skybox", "Clear color"
        };

        ImGui::TextUnformatted( "Background" );
        const SceneBackground current{ m_EditorState->ActiveEditorScene->GetSceneBackground() };
        const SceneBackground selection{ ImGuiUtils::Combo( backgroundTypes, current ) };

        m_EditorState->ActiveEditorScene->SetSceneBackground( selection );

        TextureHandle textureHandle{ m_EditorState->ActiveEditorScene->GetSkybox() };

        switch (selection) {

            case SceneBackground::SKYBOX:
                if (!textureHandle.IsEmpty()) {
                    ImGuiUtils::InputText(StringUtil::Format( "{}", textureHandle->GetTextureUri() ), true );
                }
                break;
            case SceneBackground::CLEAR_COLOR:

                break;
            default:
                break;
        }
    }
}