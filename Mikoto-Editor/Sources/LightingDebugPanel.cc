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
// #include <imgui.h>
//
// #include <ImGui/IconsMaterialDesign.h>
//
// #include <ImGui/ImGuiUtility.hh>
// #include <Layers/EditorLayer.hh>
// #include <Panels/LightingDebugPanel.hh>
//
// namespace mikoto::editor {
//
//     LightingDebugPanel::LightingDebugPanel( const LightingDebugPanelCreateInfo &info )
//         : Panel{ "Lighting Debug" }, mEditorState{ info.mState } {
//         mPanelHeaderName = gui::MakePanelName( ICON_MD_TABLE_CHART, mPanelName );
//     }
//
//     auto LightingDebugPanel::OnUpdate( float timeStep ) -> void {
//         if (!mPanelIsVisible) {
//             return;
//         }
//
//         constexpr ImGuiWindowFlags flags{ ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize };
//         ImGui::Begin( mPanelHeaderName.c_str(), &mPanelIsVisible, flags);
//
//
//         ImGui::End();
//     }
// }