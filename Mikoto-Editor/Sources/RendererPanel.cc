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
// #include <ImGui/IconsMaterialDesign.h>
//
// #include <../../Mikoto/ImGui/GraphEditor.hh>
// #include <Core/RuntimeConsole.hh>
// #include <Core/String.hh>
// #include <ImGui/ImGuiUtility.hh>
// #include <Layers/EditorLayer.hh>
// #include <Panels/RendererPanel.hh>
// #include <Renderer/Core/RenderSystem.hh>
// #include <array>
// #include <memory>
//
// #include "Core/TimeService.hh"
//
// namespace mikoto {
//
//     static auto ShowTextureDetails( const Texture* texture ) -> void {
//         const UInt32 width  = texture ? texture->GetWidth()  : 0;
//         const UInt32 height = texture ? texture->GetHeight() : 0;
//
//         ImGui::TextUnformatted("Dimensions");
//         ImGui::SameLine(90.0f);
//         if (width == 0 || height == 0) {
//             ImGui::TextUnformatted("N / A");
//         } else {
//             ImGui::TextUnformatted(fmt::format("[{}, {}]", width, height).c_str());
//         }
//
//         ImGui::TextUnformatted("Type");
//         ImGui::SameLine(90.0f);
//         if (texture) {
//             if (auto cube{ dynamic_cast<const TextureCube*>(texture)}; cube || texture->IsHDR() ) {
//                 ImGui::TextUnformatted("HDR");
//             }
//
//             if (auto flatTexture{ dynamic_cast<const Texture2D*>(texture)} ) {
//                 ImGui::TextUnformatted("Texture_2D");
//             }
//         } else {
//             ImGui::TextUnformatted("N / A");
//         }
//
//         ImGui::TextUnformatted("File Size");
//         ImGui::SameLine(90.0f);
//         if (texture) {
//             ImGui::TextUnformatted(fmt::format("{} MB", math::Round( static_cast<double>( texture->GetSizeBytes() ) / 1000'000.0, 2)).c_str());
//         } else {
//             ImGui::TextUnformatted("N / A");
//         }
//
//         ImGui::TextUnformatted("Format");
//         ImGui::SameLine(90.0f);
//         if (texture) {
//             ImGui::TextUnformatted(GetTextureFormatString(texture->GetFormat()).data());
//         } else {
//             ImGui::TextUnformatted("N / A");
//         }
//
//         ImGui::TextUnformatted("DebugName");
//         ImGui::SameLine(90.0f);
//         if (texture) {
//             ImGui::TextUnformatted(fmt::format("Unknown").c_str());
//         } else {
//             ImGui::TextUnformatted("N / A");
//         }
//
//         ImGui::TextUnformatted("Channels");
//         ImGui::SameLine(90.0f);
//         if (texture) {
//             ImGui::TextUnformatted(GetChanelString(texture->GetChannels()).data());
//         } else {
//             ImGui::TextUnformatted("N / A");
//         }
//     }
//
//     RendererPanel::RendererPanel( const RendererPanelCreateInfo &info )
//         : Panel{ "Renderer" }, m_EditorState{ info.State } {
//         m_PanelHeaderName = gui::MakePanelName( ICON_MD_POWER_SETTINGS_NEW, m_PanelName );
//
//
//         // Construct the node graph for pass dependencies
//         GraphEditorBuilder builder{};
//
//         auto& passNodes{ m_EditorState->EditorSceneRenderer->GetPassList() };
//
//         for ( const auto& [passName, pass]: passNodes ) {
//             builder.PushNode( passName );
//         }
//
//         m_GraphEditor.Build( builder );
//     }
//
//     auto RendererPanel::OnUpdate( float timeStep ) -> void {
//         if (!m_PanelIsVisible) {
//             return;
//         }
//
//         ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );
//
//         ImGui::SeparatorText( "FrameGraph Info" );
//
//         gui::DrawNode( "Passes", [this] () -> void {
//             gui::UnindentScoped und{};
//             gui::CheckBox( "Show Pass Dependencies", m_ShowPassGraph );
//
//             DrawPassInfo();
//
//             if (m_ShowPassGraph) {
//                 m_GraphEditor.Render();
//             }
//         });
//
//         gui::DrawNode( "Renderer", [this] () -> void {
//             DrawRendererConfig();
//
//             // TODO: add aspect ratio, auto, 16:1 etc
//         });
//
//         ImGui::SeparatorText( "Renderer Settings" );
//         gui::DrawNode( "Monitor", [this] () -> void {
//             gui::UnindentScoped und{};
//
//             if (gui::CheckBox( "##RendererPanel::OnUpdate::Vsync", m_EnableVSync )) {
//
//             }
//             ImGui::SameLine();
//             ImGui::TextUnformatted( "Enable vsync" );
//         });
//
//         ImGui::SeparatorText( "Graphics Features" );
//         gui::DrawNode( "Ambient Occlusion (SSAO) Settings", [this] () -> void {
//             DrawSSAOSettings();
//         });
//
//         gui::DrawNode( "Shadow Mapping settings", [this] () -> void {
//             DrawShadowMappingSettings();
//         });
//
//         gui::DrawNode( "Image Based Lighting settings", [this] () -> void {
//             DrawIBLSettings();
//         });
//
//         gui::DrawNode( "Post processing", [this] () -> void {
//             DrawPostProcessing();
//             DrawToneMapSettings();
//         });
//
//         gui::DrawNode( "RayTracing", [this] () -> void {
//             DrawRayTracingSettings();
//         });
//
//
//         ImGui::End();
//     }
//
//     auto RendererPanel::IsVsyncEnabled() const -> bool {
//         return RenderService::Get()->GetContext()->IsVsyncEnabled();
//     }
//
//     auto RendererPanel::EnableSkyboxLDR() const -> bool {
//         return m_EnableSkyboxLDR;
//     }
//
//     auto RendererPanel::DrawPassInfo() -> void {
//         const auto& passList{ m_EditorState->EditorSceneRenderer->GetPassList() };
//
//         ImGui::TextUnformatted( StringUtil::Format( "Pass count: {}", passList.size() ).c_str() );
//         ImGui::Spacing();
//
//         // Select execution display units
//         static std::array<std::string, 4> units{
//             "Seconds", "Milliseconds", "Microseconds", "Nanoseconds"
//         };
//
//         static TimeUnit currentUnit{ TimeUnit::MICROSECONDS };
//         currentUnit = gui::Combo( units, currentUnit );
//
//         static std::vector<std::string> columns{ "Name", "Reads", "Writes", "Policy", "Status", "Executed", "_Elapsed"  };
//         const UInt32 elapsedTimeIndex{ static_cast<UInt32>( columns.size() - 1 ) };
//
//         columns[elapsedTimeIndex] = StringUtil::Format( "Elapsed ({})", Time::GetUnitString(currentUnit) );
//
//         if ( ImGui::BeginTable( "RendererPanel_PassTable", columns.size(),
//                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
//                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
//                                         ImGuiTableFlags_Hideable ) ) {
//
//             std::ranges::for_each( columns, []( const auto& name ) -> void { ImGui::TableSetupColumn( name.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f ); } );
//             ImGui::TableHeadersRow();
//
//             for ( const auto& pass: passList | std::views::values ) {
//                 ImGui::TableNextRow();
//
//                 // Column 0 – Pass Name
//                 ImGui::TableSetColumnIndex( 0 );
//                 ImGui::TextUnformatted( pass.Name.c_str() );
//
//                 // Column 1 – Reads
//                 ImGui::TableSetColumnIndex( 1 );
//                 if ( pass.Reads.empty() ) {
//                     ImGui::TextDisabled( "None" );
//                 } else {
//                     for ( const auto& r: pass.Reads ) {
//                         ImGui::BulletText( "%s", r.Name.c_str() );
//                     }
//                 }
//
//                 // Column 2 – Writes
//                 ImGui::TableSetColumnIndex( 2 );
//                 if ( pass.Writes.empty() ) {
//                     ImGui::TextDisabled( "None" );
//                 } else {
//                     for ( const auto& w: pass.Writes ) {
//                         ImGui::BulletText( "%s", w.Name.c_str() );
//                     }
//                 }
//
//                 // Column 3 – Execution Policy
//                 ImGui::TableSetColumnIndex( 3 );
//                 switch ( pass.ExecutionPolicy ) {
//                     case FramePassExecutionPolicy::PER_FRAME:
//                         ImGui::TextUnformatted( "Per Frame" );
//                         break;
//                     case FramePassExecutionPolicy::ONCE:
//                         ImGui::TextUnformatted( "Once" );
//                         break;
//                     case FramePassExecutionPolicy::ON_CHANGE:
//                         ImGui::TextUnformatted( "On change" );
//                         break;
//                 }
//
//                 // Column 4 – Status
//                 ImGui::TableSetColumnIndex( 4 );
//                 switch ( pass.Status ) {
//                     case FramePassNodeStatus::ACTIVE:
//                         ImGui::TextColored( ImVec4( 0.3f, 1.0f, 0.3f, 1.0f ), "Active" );
//                         break;
//                     case FramePassNodeStatus::SLEEPING:
//                         ImGui::TextColored( ImVec4( 1.0f, 0.25f, 0.25f, 1.0f ), "Sleeping" );
//                         break;
//                 }
//
//                 // Column 5 – Execution State
//                 ImGui::TableSetColumnIndex( 5 );
//                 if ( pass.HasExecuted )
//                     ImGui::TextColored( ImVec4( 0.4f, 0.8f, 1.0f, 1.0f ), "Yes" );
//                 else
//                     ImGui::TextDisabled( "No" );
//
//                 // Column 5 – Execution State
//                 ImGui::TableSetColumnIndex( 6 );
//                 ImGui::TextColored( ImVec4( 0.4f, 0.8f, 1.0f, 1.0f ), "%s", StringUtil::Format( "{:.1f}", pass.LastExecutionTime.Convert( currentUnit ) ).c_str() );
//             }
//
//             ImGui::EndTable();
//         }
//     }
//
//     auto RendererPanel::DrawRendererConfig() -> void {
//         gui::UnindentScoped und{};
//
//         gui::CheckBox( "##RendererPanel::DrawRendererConfig::wireframe", m_EditorState->ShowWireframe );
//         ImGui::SameLine();
//         ImGui::TextUnformatted( "Render wireframe" );
//
//         constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };
//         static Vec4F clearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
//         if ( ImGui::ColorEdit3( "Wireframe Clear color", glm::value_ptr( clearColor ), colorEditFlags ) ) {
//             m_EditorState->EditorSceneRenderer->SetWireframeClearColor( clearColor );
//         }
//
//         static Vec4F lineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
//         if ( ImGui::ColorEdit3( "Wireframe color", glm::value_ptr( lineColor ), colorEditFlags ) ) {
//             m_EditorState->EditorSceneRenderer->SetWireframeLineColor( lineColor );
//         }
//
//         static float lineWidth{ 1.0f };
//         if (gui::Slider( "Wireframe width", lineWidth, { 0.0f, 30.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetWireframeLineLineWidth( lineWidth );
//         }
//
//         // Infinite grid
//         ImGui::SeparatorText( "Infinite Grid" );
//         static bool enableGrid{ true };
//         if ( gui::CheckBox( "Enable", enableGrid ) ) {
//             m_EditorState->EditorSceneRenderer->EnableInfiniteGrid( enableGrid );
//         }
//
//         static Vec4F outerSquareColor{ 0.2f, 0.2f, 0.2f, 1.0f };
//         if ( ImGui::ColorEdit4( "Outer square color", glm::value_ptr( outerSquareColor ), colorEditFlags ) ) {
//             m_EditorState->EditorSceneRenderer->SetOuterSquareColor( outerSquareColor );
//         }
//
//         static Vec4F innerSquareColor{ 0.35f, 0.35f, 0.35f, 1.0f };
//         if ( ImGui::ColorEdit4( "Inner square color", glm::value_ptr( innerSquareColor ), colorEditFlags ) ) {
//             m_EditorState->EditorSceneRenderer->SetInnerSquareColor( innerSquareColor );
//         }
//
//         static float outerSquareWidth{ 1.5f };
//         if ( gui::Slider( "Outer square width", outerSquareWidth, { 0.0f, 10.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetOuterSquareWidth( outerSquareWidth );
//         }
//
//         static float innerSquareWidth{ 0.5f };
//         if ( gui::Slider( "Inner square width", innerSquareWidth, { 0.0f, 10.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetInnerSquareWidth( innerSquareWidth );
//         }
//
//         ImGui::SeparatorText( "Axis" );
//
//         static Vec4F zAxisColor{ 0.0f, 0.0f, 1.0f, 1.0f };
//         if ( ImGui::ColorEdit4( "Z Axis Color", glm::value_ptr( zAxisColor ), colorEditFlags ) ) {
//             m_EditorState->EditorSceneRenderer->SetZAxisColor( zAxisColor );
//         }
//
//         static Vec4F xAxisColor{ 1.0f, 0.0f, 0.0f, 1.0f };
//         if ( ImGui::ColorEdit4( "X Axis Color", glm::value_ptr( xAxisColor ), colorEditFlags ) ) {
//             m_EditorState->EditorSceneRenderer->SetXAxisColor( xAxisColor );
//         }
//
//         static float zAxisWidth{ 5.0f };
//         if ( gui::Slider( "Z Axis Width", zAxisWidth, { 0.0f, 10.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetZAxisWidth( zAxisWidth );
//         }
//
//         static float xAxisWidth{ 5.0f };
//         if ( gui::Slider( "X Axis Width", xAxisWidth, { 0.0f, 10.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetXAxisWidth( xAxisWidth );
//         }
//     }
//
//     auto RendererPanel::DrawSSAOSettings() -> void {
//         gui::UnindentScoped und{};
//
//         // Enable toggle
//         if (gui::CheckBox( "##RendererPanel::DrawSSAOSettings::EnableSSAO", m_EnableSSAO )) {
//             m_EditorState->EditorSceneRenderer->SetEnableSSAO(m_EnableSSAO);
//         }
//         ImGui::SameLine();
//         ImGui::TextUnformatted( "Enable SSAO" );
//
//         // Enable blurr toggle
//         if ( gui::CheckBox( "##RendererPanel::DrawSSAOSettings::EnableSSAOBlurr", m_EnableSSAOBlur ) ) {
//             m_EditorState->EditorSceneRenderer->SetEnableSSAOBlurred( m_EnableSSAOBlur );
//         }
//         ImGui::SameLine();
//         ImGui::TextUnformatted( "Enable SSAO blur" );
//
//         ImGui::Spacing();
//
//         gui::Slider( "Radius", m_SSAORadius, { 0.05f, 5.0f } );
//         gui::Slider( "Bias", m_SSAOBias, { 0.0f,  0.1f } );
//
//         if (gui::Slider("Power", m_SSAOStrength, { 0.1f, 5.0f })) {
//             m_EditorState->EditorSceneRenderer->SetSSAOIntensity( m_SSAOStrength );
//         }
//     }
//
//     auto RendererPanel::DrawPostProcessing() -> void {
//         gui::UnindentScoped und{};
//
//         static bool enableBloom{ false };
//         if (gui::CheckBox( "##RendererPanel::DrawPostProcessing::Bloom", enableBloom )) {
//             m_EditorState->EditorSceneRenderer->EnableBloom( enableBloom );
//         }
//         ImGui::SameLine();
//         ImGui::TextUnformatted( "Enable bloom" );
//
//         // Color saturation
//
//         // ImGui::Spacing();
//         // ImGui::Separator();
//         // ImGui::Spacing();
//
//         // static float contrast{ 1.0f };
//         // if (gui::Slider( "Contrast", contrast, { 0.0f, 10.0f } ) ) {
//         //     m_EditorState->EditorSceneRenderer->SetContrast( contrast );
//         // }
//         //
//         // static float saturation{ 1.0f };
//         // if (gui::Slider( "Saturation", saturation, { 0.0f, 10.0f } ) ) {
//         //     m_EditorState->EditorSceneRenderer->SetSaturation( saturation );
//         // }
//         //
//         // constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };
//         //
//         // ImGui::Spacing();
//         // ImGui::Separator();
//         // ImGui::Spacing();
//         //
//         // static Vec4F tintColor{ 0.1f, 0.2f, 0.3f, 1.0f };
//         // if ( ImGui::ColorEdit3( "Tint", glm::value_ptr( tintColor ), colorEditFlags ) ) {
//         //     m_EditorState->EditorSceneRenderer->SetTintColor( tintColor );
//         // }
//     }
//
//     auto RendererPanel::DrawToneMapSettings() -> void {
//         gui::UnindentScoped und{};
//
//         ImGui::Spacing();
//         ImGui::Separator();
//         ImGui::Spacing();
//
//         static float gamma{ 2.0f };
//         static float exposure{ 1.0f };
//
//         if (gui::Slider( "Gamma", gamma, { 0.0f, 10.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetImageGamma( gamma );
//         }
//
//         if (gui::Slider( "Exposure", exposure, { 0.0f, 10.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetImageExposure( exposure );
//         }
//
//         ImGui::Spacing();
//         ImGui::Spacing();
//
//         std::array<std::string, static_cast<Size>(ToneMappingType::Max_Count)> choices{
//             "Linear", "Reinhard", "Uncharted2", "Aces", "Khronos Neutral"
//         };
//
//         static ToneMappingType toneMapType{ ToneMappingType::Aces };
//         toneMapType = gui::Combo( choices, toneMapType );
//
//         if (m_FinalCompositionTarget != FinalCompositionTarget::ENUM_MAX) {
//             m_EditorState->EditorSceneRenderer->SetToneMapping( toneMapType );
//         }
//
//         ImGui::SameLine();
//         ImGui::TextUnformatted( "ToneMap type" );
//     }
//
//     auto RendererPanel::DrawRayTracingSettings() -> void {
//         gui::UnindentScoped und{};
//
//         // Path tracing
//         static bool enablePathTracing{ false };
//         if (gui::CheckBox("##RendererPanel::PathTracing", enablePathTracing)) {
//             // TODO:
//         }
//
//         gui::SetCursorHandOnLastItemHovered();
//         ImGui::SameLine();
//         ImGui::TextUnformatted("Enable Path Tracing");
//
//         // Ray Traced Shadows
//         static bool enableRTShadows{ false };
//         if (gui::CheckBox("##RendererPanel::RTShadows", enableRTShadows)) {
//             // TODO:
//         }
//
//         gui::SetCursorHandOnLastItemHovered();
//         ImGui::SameLine();
//         ImGui::TextUnformatted("Ray Traced Shadows");
//
//         // Ray Traced Ambient Occlusion
//         static bool enableRTAO{ false };
//         if (gui::CheckBox("##RendererPanel::RTAO", enableRTAO)) {
//             // TODO:
//         }
//
//         gui::SetCursorHandOnLastItemHovered();
//         ImGui::SameLine();
//         ImGui::TextUnformatted("Ray Traced Ambient Occlusion");
//
//         // Ray Traced Reflections and Refractions
//         static bool enableRTReflections{ false };
//         if (gui::CheckBox("##RendererPanel::RTReflections", enableRTReflections)) {
//             // TODO:
//         }
//
//         gui::SetCursorHandOnLastItemHovered();
//         ImGui::SameLine();
//         ImGui::TextUnformatted("Ray Traced Reflections");
//
//         // Ray Traced Global Illumination
//         static bool enableRTGI{ false };
//         if (gui::CheckBox("##RendererPanel::RTGI", enableRTGI)) {
//             // TODO:
//         }
//
//         gui::SetCursorHandOnLastItemHovered();
//         ImGui::SameLine();
//         ImGui::TextUnformatted("Ray Traced Global Illumination");
//     }
//
//     auto RendererPanel::DrawIBLSettings() -> void {
//         gui::UnindentScoped und{};
//
//         auto usingConvolutedCube{ m_EditorState->EditorSceneRenderer->IsUsingConvolutedCube() };
//         if (gui::CheckBox( "##RendererPanel::DrawIBLSettings::UseConv", usingConvolutedCube )) {
//             m_EditorState->EditorSceneRenderer->SetUseConvolutedCube(usingConvolutedCube);
//         }
//         ImGui::SameLine();
//         ImGui::TextUnformatted( "Use convoluted cube" );
//
//         gui::CheckBox( "##RendererPanel::DrawRendererConfig::LDR", m_EnableSkyboxLDR );
//         ImGui::SameLine();
//         ImGui::TextUnformatted( "Use Precomputed Cubemap" );
//
//         auto hdrDropTarget{
//             [this]() -> void {
//                 if (ImGui::BeginDragDropTarget()) {
//                     if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("HDR_LOAD_LIGHT_PANEL") }) {
//                         std::string hdrPath{ *static_cast<std::string*>( payload->Data ) };
//                         m_EditorState->EditorSceneRenderer->UpdateEquirectangularMapAsync(hdrPath);
//                         RuntimeConsole::Get()->Debug( StringUtil::Format("You dropped texture from HDR_LOAD_LIGHT_PANEL {}", hdrPath ) );
//                     }
//                     ImGui::EndDragDropTarget();
//                 }
//             }
//         };
//
//         TextureHandle textureHandle{ m_EditorState->EditorSceneRenderer->GetEquirectangularMap() };
//
//         if ( ImGui::BeginTable( "HDRTextureSelector", 2,
//                                 ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersInnerH |
//                                         ImGuiTableFlags_SizingStretchSame ) ) {
//             ImGui::TableSetupColumn( "Thumbnail", ImGuiTableColumnFlags_None, 70.0f );
//             ImGui::TableSetupColumn( "Info", ImGuiTableColumnFlags_None, 200.0f);
//
//             ImGui::TableHeadersRow();
//
//             ImGui::TableNextRow();
//             ImGui::TableSetColumnIndex( 0 );
//
//             TextureHandle displayedHandle{ textureHandle };
//             if ( displayedHandle.IsEmpty() ) {
//                 displayedHandle = AssetsService::Get()->GetDummyTexture();
//             }
//
//             if ( gui::PushImageButton(
//                          "##RendererPanel::DrawIBLSettings:DisplayTextureID",
//                          ImGuiService::Get()->GetTextureID( displayedHandle ),
//                          ImVec2{ 64, 64 } ) ) {
//                 // Clicked thumbnail
//             }
//
//             // Tooltip
//             gui::ToolTip( [&]() {
//                 ImGui::TextUnformatted( "Click to load texture" );
//                 if ( !textureHandle.IsEmpty() )
//                     ImGui::TextUnformatted( textureHandle->GetTextureUri().c_str() );
//             },ImGui::IsItemHovered() );
//
//             hdrDropTarget();
//
//             ImGui::TableSetColumnIndex( 1 );
//             ShowTextureDetails( textureHandle.GetRaw() );
//
//             ImGui::EndTable();
//         }
//
//         ImGui::Spacing();
//
//         if (!textureHandle.IsEmpty()) {
//             gui::InputText(StringUtil::Format( "{}", textureHandle->GetTextureUri() ), true );
//         } else {
//             gui::InputText(StringUtil::Format( "Drag EquirectangularMap here" ), true );
//         }
//
//         ImGui::Spacing();
//
//         static  std::array<std::string, 2> backgroundTypes{
//             "Skybox", "Clear color"
//         };
//         const SceneBackground current{ m_EditorState->ActiveEditorScene->GetSceneBackground() };
//         const SceneBackground selection{ gui::Combo( backgroundTypes, current ) };
//
//         m_EditorState->ActiveEditorScene->SetSceneBackground( selection );
//
//         ImGui::Spacing();
//
//         hdrDropTarget();
//
//         constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };
//
//         ImGui::Spacing();
//
//         static Vec4F clearColor{ 0.1f, 0.2f, 0.3f, 1.0f };
//         if ( ImGui::ColorEdit3( "Clear", glm::value_ptr( clearColor ), colorEditFlags ) ) {
//             m_EditorState->EditorSceneRenderer->SetClearColor( clearColor );
//         }
//
//         static float lineWidth{ 9.0f };
//         if (gui::Slider( "Max ReflectionLOD", lineWidth, { 0.0f, 30.0f } ) ) {
//             m_EditorState->EditorSceneRenderer->SetMaxReflectionLOD( lineWidth );
//         }
//     }
//
//     auto RendererPanel::DrawShadowMappingSettings() -> void {
//         std::array<std::string, static_cast<Size>(FinalCompositionTarget::ENUM_MAX)> choices{
//             "Color", "Emissive", "Normals", "Position", "Depth", "Final Image"
//         };
//
//         // These are directly taken from the core passes from the Scene renderer
//         std::array<std::string, static_cast<Size>(FinalCompositionTarget::ENUM_MAX)> images{
//             "GBuffer_Color", "GBuffer_Emissive","GBuffer_Normal", "GBuffer_Position", "DepthPrePass_Color", "Tonemap_ColorTarget",
//         };
//
//         m_FinalCompositionTarget = gui::Combo( choices, m_FinalCompositionTarget );
//
//         if (m_FinalCompositionTarget != FinalCompositionTarget::ENUM_MAX) {
//             m_EditorState->FinalComposition = m_EditorState->EditorSceneRenderer->GetTexture( images[static_cast<Size>(m_FinalCompositionTarget)] );
//         }
//     }
// }