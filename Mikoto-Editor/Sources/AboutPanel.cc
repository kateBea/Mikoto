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

#include <Core/LocalizationService.hh>

#include <Filesystem/FileSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Panels/AboutPanel.hh>

namespace mikoto::editor {
    AboutPanel::AboutPanel( const AboutPanelCreateInfo& info )
        : Panel{ "About Mikoto", "About Mikoto", info.mGroup, info.mIsToggleable } {
        mPanelIsVisible = false;
    }

    auto AboutPanel::OnRender( core::f32 ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 560, 400 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted( MKT_LOC( "about_copyright" ).c_str() );
        ImGui::TextWrapped( "%s", MKT_LOC( "about_c_20_engine_with_lua_and_c_scripting_see_the" ).c_str() );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "about_repository_and_credits", "Repository and credits" ).c_str() ) ) {
            filesystem::OpenInBrowser( "https://github.com/kateBea/Mikoto" );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "about_release_notes", "Release notes" ).c_str() ) ) {
            filesystem::OpenInBrowser( "https://github.com/kateBea/Mikoto/releases" );
        }

        ImGui::End();
    }
}// namespace mikoto::editor
