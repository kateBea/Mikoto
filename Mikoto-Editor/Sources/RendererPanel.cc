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

        ImGui::TextUnformatted( StringUtil::Format( "Pass count: {}", passList.size() ).c_str() );
        ImGui::Spacing();

        constexpr  UInt32 columnCount{ 6 };
        if ( ImGui::BeginTable( "RendererPanel_PassTable", columnCount,
                                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp ) ) {
            ImGui::TableSetupColumn( "Name" );
            ImGui::TableSetupColumn( "Reads" );
            ImGui::TableSetupColumn( "Writes" );
            ImGui::TableSetupColumn( "Policy" );
            ImGui::TableSetupColumn( "Status" );
            ImGui::TableSetupColumn( "Executed" );
            ImGui::TableHeadersRow();

            for ( const auto& pass: passList | std::views::values ) {
                ImGui::TableNextRow();

                // Column 0 – Pass Name
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( pass.Name.c_str() );

                // Column 1 – Reads
                ImGui::TableSetColumnIndex( 1 );
                if ( pass.Reads.empty() ) {
                    ImGui::TextDisabled( "None" );
                } else {
                    for ( const auto& r: pass.Reads ) {
                        ImGui::BulletText( "%s", r.Name.c_str() );
                    }
                }

                // Column 2 – Writes
                ImGui::TableSetColumnIndex( 2 );
                if ( pass.Writes.empty() ) {
                    ImGui::TextDisabled( "None" );
                } else {
                    for ( const auto& w: pass.Writes ) {
                        ImGui::BulletText( "%s", w.Name.c_str() );
                    }
                }

                // Column 3 – Execution Policy
                ImGui::TableSetColumnIndex( 3 );
                switch ( pass.ExecutionPolicy ) {
                    case FramePassExecutionPolicy::PER_FRAME:
                        ImGui::TextUnformatted( "Per Frame" );
                        break;
                    case FramePassExecutionPolicy::ONCE:
                        ImGui::TextUnformatted( "Once" );
                        break;
                    case FramePassExecutionPolicy::ON_CHANGE:
                        ImGui::TextUnformatted( "On change" );
                        break;
                }

                // Column 4 – Status
                ImGui::TableSetColumnIndex( 4 );
                switch ( pass.Status ) {
                    case FramePassNodeStatus::ACTIVE:
                        ImGui::TextColored( ImVec4( 0.3f, 1.0f, 0.3f, 1.0f ), "Active" );
                        break;
                    case FramePassNodeStatus::SLEEPING:
                        ImGui::TextColored( ImVec4( 1.0f, 0.25f, 0.25f, 1.0f ), "Sleeping" );
                        break;
                }

                // Column 5 – Execution State
                ImGui::TableSetColumnIndex( 5 );
                if ( pass.HasExecuted )
                    ImGui::TextColored( ImVec4( 0.4f, 0.8f, 1.0f, 1.0f ), "Yes" );
                else
                    ImGui::TextDisabled( "No" );
            }

            ImGui::EndTable();
        }
    }

    auto RendererPanel::DrawRendererConfig() -> void {
        ImGuiUtils::CheckBox( "##RendererPanel::DrawRendererConfig::Checkbox", m_IsWireframeEnabled );
        ImGui::SameLine();
        ImGui::TextUnformatted( "Render wireframe" );
    }
}// namespace Mikoto