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

//
// #include <EASTL/string.h>
// #include <EASTL/string_view.h>
//
// #include <Core/Core.hh>
// #include <Core/Types.hh>
//
// #include <Memory/Allocator.hh>
//
// #include <ImGui/ImGuiUtility.hh>
//
// #include <Layers/EditorLayer.hh>
// #include <Panels/AssetsPanel.hh>
//
// namespace mikoto::editor {
//
//     AssetsPanel::AssetsPanel( const AssetsPanelDescription &description )
//         : Panel{ "Assets" }, mEditorState{ description.mState }
//     {
//         // Icons for ICON_MD for assets U+F1B2, U+F1B3, U+F6D1
//         // TODO: find the actual ICON_MD macros
//         // U+F6D1  ->  63185
//         // U+F1B2  ->  61874
//         // U+F1B3  ->  61875
//         const eastl::string icon{ gui::GetStringFromUnicode( 61875 ) };
//         mPanelHeaderName = gui::MakePanelName( string::Format("{}", icon.c_str()), mPanelName );
//     }
//
//     auto AssetsPanel::OnUpdate( float ) -> void {
//         if ( !mPanelIsVisible ) {
//             return;
//         }
//
//         constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };
//         ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), windowFlags );
//
//         mPanelIsFocused = ImGui::IsWindowFocused();
//         mPanelIsHovered = ImGui::IsWindowHovered();
//
//         ImGui::End();
//     }
// }