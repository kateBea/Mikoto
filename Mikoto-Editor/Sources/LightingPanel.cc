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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/RuntimeConsole.hh>

#include <Memory/Allocator.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Layers/EditorLayer.hh>

#include <Panels/LightingPanel.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    LightingPanel::LightingPanel( const LightingPanelCreateInfo &info )
        : Panel{ "Lighting" }, mEditorState{ info.mState } {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_LIGHTBULB, mPanelName );
    }

    auto LightingPanel::OnUpdate( float timeStep ) -> void {
        if (!mPanelIsVisible) {
            return;
        }

        ImVec2 initialSize{ ImVec2(400.0f, 300.0f) };
        ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ),
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        // Get the total width available in the current window
        f32 availableWidth{ ImGui::GetContentRegionAvail().x };

        i32 tabCount{ 3 };

        f32 itemSpacingX{ ImGui::GetStyle().ItemSpacing.x };
        f32 tabWidth{ (availableWidth / tabCount) - (itemSpacingX * 0.5f) };

        ImGui::PushStyleVar(ImGuiStyleVar_TabMinWidthBase, tabWidth);
        if ( ImGui::BeginTabBar( "LightingTabBar", ImGuiTabBarFlags_FittingPolicyResizeDown ) ) {
            if ( ImGui::BeginTabItem( "Environment" ) ) {
                ImGui::EndTabItem();
            }
            imgui::SetCursorHandOnLastItemHovered();

            if ( ImGui::BeginTabItem( "Lights" ) ) {
                ImGui::EndTabItem();
            }
            imgui::SetCursorHandOnLastItemHovered();

            if ( ImGui::BeginTabItem( "Scene" ) ) {
                ImGui::EndTabItem();
            }
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::EndTabBar();
        }

        ImGui::PopStyleVar();
        ImGui::End();
    }
}