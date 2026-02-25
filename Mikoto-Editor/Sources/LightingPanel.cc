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

#include <ImGui/IconsMaterialDesign.h>

#include <Common/String.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Panels/LightingPanel.hh>

#include <Core/RuntimeConsole.hh>

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

        ImGuiTabBarFlags tabFlags{ ImGuiTabBarFlags_None };
        if ( ImGui::BeginTabBar( "LightingTabBar", tabFlags ) ) {
            if ( ImGui::BeginTabItem( "Scene" ) ) {
                DrawSceneSettings();
                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Shadows" ) ) {
                DrawShadowsSettings();
                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Lights" ) ) {
                DrawLightsSettings();
                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Environment" ) ) {
                DrawEnvironmentSettings();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    auto LightingPanel::DrawShadowsSettings() -> void {

    }

    auto LightingPanel::DrawLightsSettings() -> void {
    }

    auto LightingPanel::DrawEnvironmentSettings() -> void {
    }

    auto LightingPanel::DrawSceneSettings() -> void {
        static  std::array<std::string, 2> backgroundTypes{
            "Skybox", "Clear color"
        };

        ImGui::SeparatorText( "Sky light" );

        ImGui::Text( "Skybox Texture" );
        ImGui::SameLine();

        const SceneBackground current{ m_EditorState->ActiveEditorScene->GetSceneBackground() };
        const SceneBackground selection{ ImGuiUtils::Combo( backgroundTypes, current ) };

        m_EditorState->ActiveEditorScene->SetSceneBackground( selection );

        TextureHandle textureHandle{ m_EditorState->ActiveEditorScene->GetSkybox() };

        switch (selection) {

            case SceneBackground::SKYBOX:
                if (!textureHandle.IsEmpty()) {
                    ImGuiUtils::InputText(StringUtil::Format( "{}", textureHandle->GetTextureUri() ), true );
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("HDR_LOAD_LIGHT_PANEL") }) {
                        std::string hdrPath{ *static_cast<std::string*>( payload->Data ) };
                        RuntimeConsole::Get()->Debug( StringUtil::Format("You dropped texture from HDR_LOAD_LIGHT_PANEL {}", hdrPath ) );
                    }
                    ImGui::EndDragDropTarget();
                }

                break;
            case SceneBackground::CLEAR_COLOR:

                break;
            default:
                break;
        }

        ImGui::Text( "Default ambient" );
        ImGui::SameLine();

        static float ambientIntensity{ 1.0f };
        ImGuiUtils::Slider( "##AmbientSlider", ambientIntensity, { 0.0f, 10.0f } );
        ImGui::Spacing();

        ImGui::Text( "Default reflection" );
        ImGui::SameLine();
        ImGui::Spacing();
    }
}