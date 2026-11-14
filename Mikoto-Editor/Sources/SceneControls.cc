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

#include <imgui.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/LocalizationService.hh>

#include <Scene/Scene.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/SceneControls.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    static constexpr std::array<core::cstr, 3> kSceneControlIcons{ ICON_MD_PLAY_ARROW, ICON_MD_BOLT, ICON_MD_STOP };

    auto GetSceneControlsWidth( const SceneControlVisibility& visibility ) -> float {
        const std::array<bool, 3> visible{ visibility.mPlay, visibility.mSimulate, visibility.mStop };
        float width{};
        core::usize count{};

        for ( core::usize index{}; index < visible.size(); ++index ) {
            if ( visible[index] ) {
                width += ImGui::CalcTextSize( kSceneControlIcons[index] ).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ++count;
            }
        }

        return count > 0 ? width + ImGui::GetStyle().ItemSpacing.x * static_cast<float>( count - 1 ) : 0.0f;
    }

    auto DrawSceneControls( EditorState& state, const SceneControlVisibility& visibility ) -> void {
        const std::array<core::cstr, 3> labelsDesc{
            MKT_LOC( "viewport_play" ).c_str(),
            MKT_LOC( "viewport_simulate" ).c_str(),
            MKT_LOC( "viewport_stop" ).c_str()
        };
        const bool hasScene{ state.HasWorkspace() && state.mActiveScene };
        const bool running{ state.mRuntimeActive };
        const bool simulating{ hasScene && state.mActiveScene->IsSimulating() };
        constexpr std::array<ImVec4, 3> colors{
            ImVec4{ 0.35f, 0.82f, 0.46f, 1.0f },
            ImVec4{ 0.30f, 0.64f, 0.96f, 1.0f },
            ImVec4{ 0.95f, 0.32f, 0.35f, 1.0f }
        };
        const auto button{ [&]( core::usize index ) {
            ImGui::PushStyleColor( ImGuiCol_Text, colors[index] );
            const bool pressed{ ImGui::Button( kSceneControlIcons[index] ) };
            ImGui::PopStyleColor();
            return pressed;
        } };
        ImGui::BeginDisabled( !hasScene || running || simulating );

        if ( visibility.mPlay ) {
            if ( button( 0 ) ) {
                state.mRequestPlay();
            }

            imgui::SetCursorHandOnLastItemHovered();
            imgui::ImGuiScopedStyleVar popupPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 8.0f, 8.0f } };
            imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f } };

            imgui::widget::MakeHelpPopUpDelay( labelsDesc[0], "", 1.5f );
        }

        if ( visibility.mSimulate ) {
            if ( visibility.mPlay ) {
                ImGui::SameLine();
            }

            if ( button( 1 ) ) {
                state.mActiveScene->SetState( scene::SceneState::eSimulating );
            }

            imgui::SetCursorHandOnLastItemHovered();
            imgui::ImGuiScopedStyleVar popupPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 8.0f, 8.0f } };
            imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f } };

            imgui::widget::MakeHelpPopUpDelay( labelsDesc[1], "", 1.5f );
        }

        ImGui::EndDisabled();

        if ( visibility.mStop ) {
            if ( visibility.mPlay || visibility.mSimulate ) {
                ImGui::SameLine();
            }

            ImGui::BeginDisabled( !running && !simulating );

            if ( button( 2 ) ) {
                if ( running ) {
                    state.mRequestStop();
                }
                else if ( hasScene ) {
                    state.mActiveScene->SetState( scene::SceneState::eIdle );
                }
            }

            imgui::SetCursorHandOnLastItemHovered();
            {
                imgui::ImGuiScopedStyleVar popupPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 8.0f, 8.0f } };
                imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f } };

                imgui::widget::MakeHelpPopUpDelay( labelsDesc[2], "", 1.5f );
            }

            ImGui::EndDisabled();
        }
    }
}
