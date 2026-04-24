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
// #include <ranges>
//
// #include <EASTL/algorithm.h>
//
// #include <Core/Core.hh>
// #include <Core/Types.hh>
//
// #include <Memory/Allocator.hh>
//
// #include <ImGui/ImGuiService.hh>
// #include <ImGui/ImGuiUtility.hh>
// #include <ImGui/IconsMaterialDesign.h>
//
// #include <Layers/EditorLayer.hh>
// #include <Panels/PassVisualizerPanel.hh>
//
// namespace mikoto::editor {
//
//     static auto DrawTexturePreview(TextureHandle composition, float maxWidth, float maxHeight) -> void {
//         // No flipping, the final image is already in the correct viewport coordinates
//         // In the case of vulkan this is also taken into account when setting up the viewport
//         ImTextureID id{ gui::ImGuiService::Get()->GetTextureID( composition) };
//         if (!id) {
//             return;
//         }
//
//         // Query texture size
//         ImVec2 texSize{ static_cast<float>( composition->GetWidth() ), static_cast<float>( composition->GetHeight() ) };
//         if (texSize.x == 0 || texSize.y == 0) {
//             return;
//         }
//
//         const float aspect{ texSize.x / texSize.y };
//         ImVec2 displaySize{};
//
//         if (texSize.x > texSize.y) {
//             displaySize.x = eastl::min(maxWidth, texSize.x);
//             displaySize.y = displaySize.x / aspect;
//
//             if (displaySize.y > maxHeight) {
//                 displaySize.y = maxHeight;
//                 displaySize.x = displaySize.y * aspect;
//             }
//         } else {
//             displaySize.y = eastl::min(maxHeight, texSize.y);
//             displaySize.x = displaySize.y * aspect;
//
//             if (displaySize.x > maxWidth) {
//                 displaySize.x = maxWidth;
//                 displaySize.y = displaySize.x / aspect;
//             }
//         }
//
//         ImGui::Image(id, displaySize);
//     }
//
//     PassVisualizerPanel::PassVisualizerPanel( const PassVisualizerDescription &description )
//         : Panel{ "Preview" }, mEditorState{ description.mState } {
//         mPanelHeaderName = gui::MakePanelName( string::Format("{}", ICON_MD_WINDOW), mPanelName );
//
//         mPassesCompositions.try_emplace( "1. Triangle", mEditorState->mMainRenderer->GetTexture( "HelloTriangle_ColorTarget" ) );
//         mPassesCompositions.try_emplace( "2. Texture2D", mEditorState->mMainRenderer->GetTexture( "HelloTexture_ColorTarget" ) );
//         mPassesCompositions.try_emplace( "3. BRDF LUT", mEditorState->mMainRenderer->GetTexture( "BRDFLutPass_ColorTarget" ) );
//         mPassesCompositions.try_emplace( "5. ShadowMap", mEditorState->mMainRenderer->GetTexture( "DirectionalShadowMapPass_ColorTarget" ) );
//         mPassesCompositions.try_emplace( "6. Bloom", mEditorState->mMainRenderer->GetTexture( "BloomBlend_ColorTarget" ) );
//         mPassesCompositions.try_emplace( "7. Gradient", mEditorState->mMainRenderer->GetTexture( "ColorGradient_ColorTarget" ) );
//         mPassesCompositions.try_emplace( "8. Chroma", mEditorState->mMainRenderer->GetTexture( "ChromaticAberration_ColorTarget" ) );
//     }
//
//     auto PassVisualizerPanel::OnUpdate( float ) -> void {
//         if ( !mPanelIsVisible ) {
//             return;
//         }
//
//         constexpr ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };
//
//         // Expand scene view to window bounds (no padding)
//         gui::ImGuiScopedStyleVar winPad( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );
//
//         ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), windowFlags );
//
//         mPanelIsFocused = ImGui::IsWindowFocused();
//         mPanelIsHovered = ImGui::IsWindowHovered();
//
//         if (mEditorState && !mPassesCompositions.empty()) {
//             ImGui::BeginChild("PassList", ImVec2(150, 0), true);
//             for ( auto &name: mPassesCompositions | std::views::keys ) {
//                 if (ImGui::Selectable(name.c_str(), name == mActivePassName)) {
//                     mActivePassName = name;
//                 }
//             }
//
//             ImGui::EndChild();
//             ImGui::SameLine();
//
//             // Main preview
//             ImGui::BeginGroup();
//             if (!mActivePassName.empty()) {
//                 auto it{ mPassesCompositions.find(mActivePassName) };
//                 if (it != mPassesCompositions.end()) {
//                     ImVec2 availSize = ImGui::GetContentRegionAvail();
//                     DrawTexturePreview(it->second, availSize.x, availSize.y);
//                 }
//             } else {
//                 ImGui::TextDisabled("Select a pass to preview...");
//             }
//             ImGui::EndGroup();
//         } else {
//             ImGui::TextDisabled("No passes available for preview.");
//         }
//
//         ImGui::End();
//     }
// }// namespace Mikoto
