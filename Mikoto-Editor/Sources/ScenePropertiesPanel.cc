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
// #include <ImGui/ImGuiUtility.hh>
// #include <ImGui/IconsMaterialDesign.h>
//
// #include <Layers/EditorLayer.hh>
//
// #include <Physics/PhysicsWorld.hh>
//
// #include <Panels/ScenePropertiesPanel.hh>
//
// namespace mikoto::editor {
//
//     using namespace mikoto::physics;
//
//     ScenePropertiesPanel::ScenePropertiesPanel( const ScenePropertiesPanelCreateInfo &info )
//         : Panel{ "Scene Properties" }, mEditorState{ info.State }
//     {
//         mPanelHeaderName = gui::MakePanelName( ICON_MD_DATA_OBJECT, mPanelName );
//     }
//
//     auto ScenePropertiesPanel::OnUpdate( float timeStep ) -> void {
//         if (!mPanelIsVisible) {
//             return;
//         }
//
//         ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );
//
//         gui::DrawNode( "Background", [this]() -> void {
//             gui::UnindentScoped und{};
//
//             ImGui::Separator();
//             ImGui::Text( "Skybox" );
//         } );
//
//         gui::DrawNode( "Scene Stats", [this]() -> void {
//             gui::UnindentScoped und{};
//
//             // TODO
//         } );
//
//         gui::DrawNode( "Metadata", [this]() {
//             gui::UnindentScoped und{};
//
//             // TODO
//         } );
//
//         gui::DrawNode( "Physics world", [this]() -> void {
//             gui::UnindentScoped und{};
//
//             auto scene{ mEditorState->mActiveEditorScene };
//             auto *physics{ scene->GetPhysicsWorld() };
//
//             static std::array<std::string, 4> values{
//                 "Earth", "Moon", "Mars", "Jupiter"
//             };
//
//             GravityBody current{ physics->GetGravityBody() };
//             current = gui::Combo( values, current );
//
//             physics->SetGravityBody( current );
//         } );
//
//         ImGui::End();
//     }
// }