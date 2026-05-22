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

#include <EASTL/array.h>
#include <EASTL/memory.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/TimeService.hh>
#include <Core/RuntimeConsole.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/GraphEditor.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Memory/Allocator.hh>

#include <Layers/EditorLayer.hh>

#include <Panels/RendererPanel.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::renderer;

    static auto ShowTextureDetails( const ITexture* texture ) -> void {
        if (!texture) {
            return;
        }

        const u32 width{ texture ? texture->GetWidth()  : 0 };
        const u32 height{ texture ? texture->GetHeight() : 0 };

        ImGui::TextUnformatted("Dimensions");
        ImGui::SameLine(90.0f);
        if (width == 0 || height == 0) {
            ImGui::TextUnformatted("N / A");
        } else {
            ImGui::TextUnformatted(fmt::format("[{}, {}]", width, height).c_str());
        }

        ImGui::TextUnformatted("Type");
        ImGui::SameLine(90.0f);

        ImGui::TextUnformatted("File Size");
        ImGui::SameLine(90.0f);
        ImGui::TextUnformatted(fmt::format("{} MB", math::Round( static_cast<double>( texture->GetSizeBytes() ) / 1000'000.0, 2)).c_str());

        ImGui::TextUnformatted("Format");
        ImGui::SameLine(90.0f);
        if (texture) {

        } else {
            ImGui::TextUnformatted("N / A");
        }

        ImGui::TextUnformatted("DebugName");
        ImGui::SameLine(90.0f);
        if (texture) {
            ImGui::TextUnformatted(fmt::format("Unknown").c_str());
        } else {
            ImGui::TextUnformatted("N / A");
        }

        ImGui::TextUnformatted("Channels");
        ImGui::SameLine(90.0f);

    }

    RendererPanel::RendererPanel( const RendererPanelCreateInfo &createInfo )
        : Panel{ "Renderer" },  mEditorState{ createInfo.mState } {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_POWER_SETTINGS_NEW, mPanelName );

        // Construct the node graph for pass dependencies
        GraphEditorBuilder builder{};
        auto& nodeControl{ mEditorState->mSceneRenderer->GetNodeControl() };

        for ( const auto& [passName, pass]: nodeControl.mNodes ) {
            builder.PushNode( passName );
        }

        mGraphEditor.Build( builder );
    }

    auto RendererPanel::OnUpdate( float timeStep ) -> void {
        if (!mPanelIsVisible) {
            return;
        }

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ),
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGui::SeparatorText( "FrameGraph Info" );

        gui::DrawNode( "Passes", [this] () -> void {
            gui::UnindentScoped und{};
            gui::CheckBox( "Show Pass Dependencies", mShowPassGraph );

            DrawPassInfo();

            if (mShowPassGraph) {
                mGraphEditor.Render();
            }
        });

        gui::DrawNode( "Renderer", [this] () -> void {
            DrawRendererConfig();

            // TODO: add aspect ratio, auto, 16:1 etc
        });

        ImGui::SeparatorText( "Renderer Settings" );
        gui::DrawNode( "Monitor", [this] () -> void {
            gui::UnindentScoped und{};

            ImGui::SameLine();
            ImGui::TextUnformatted( "Refresh Rate" );
        });

        ImGui::SeparatorText( "Graphics Features" );
        gui::DrawNode( "Ambient Occlusion (SSAO) Settings", [this] () -> void {
            DrawSSAOSettings();
        });

        gui::DrawNode( "Shadow Mapping settings", [this] () -> void {
            DrawShadowMappingSettings();
        });

        gui::DrawNode( "Image Based Lighting settings", [this] () -> void {
            DrawIBLSettings();
        });

        gui::DrawNode( "Post processing", [this] () -> void {
            DrawPostProcessing();
            DrawToneMapSettings();
        });

        gui::DrawNode( "RayTracing", [this] () -> void {
            DrawRayTracingSettings();
        });


        ImGui::End();
    }

    auto RendererPanel::DrawPassInfo() -> void {
        const auto& passList{ mEditorState->mSceneRenderer->GetNodeControl() };

        ImGui::TextUnformatted( string::Format( "Pass count: {}", passList.mNodes.size() ).c_str() );
        ImGui::Spacing();

        // Select execution display units
        static eastl::array<std::string, 4> units{
            "Seconds", "Milliseconds", "Microseconds", "Nanoseconds"
        };

        static TimeUnit currentUnit{ TimeUnit::eMicroseconds };
        currentUnit = gui::Combo( units, currentUnit );

        static eastl::vector<eastl::string> columns{ "Name","Policy" };

        if ( ImGui::BeginTable( "RendererPanel_PassTable", columns.size(),
                                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                        ImGuiTableFlags_Hideable ) ) {

            std::ranges::for_each( columns, []( const auto& name ) -> void {
                ImGui::TableSetupColumn( name.c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f );
            } );
            ImGui::TableHeadersRow();

            for ( const auto& pass: passList.mNodes | std::views::values ) {
                ImGui::TableNextRow();

                // Column 0 – Pass Name
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( pass.mName.c_str() );

                // Column 1 – Execution Policy
                ImGui::TableSetColumnIndex( 3 );
                switch ( pass.mExecutionPolicy ) {
                    case FGExecutionPolicy::ePerFrame:
                        ImGui::TextUnformatted( "Per Frame" );
                        break;
                    case FGExecutionPolicy::eOnce:
                        ImGui::TextUnformatted( "Once" );
                        break;
                    case FGExecutionPolicy::eOnChange:
                        ImGui::TextUnformatted( "On change" );
                        break;
                }
            }

            ImGui::EndTable();
        }
    }

    auto RendererPanel::DrawRendererConfig() -> void {
        gui::UnindentScoped und{};

        //gui::CheckBox( "##RendererPanel::DrawRendererConfig::wireframe", mEditorState->ShowWireframe );
        ImGui::SameLine();
        ImGui::TextUnformatted( "Render wireframe" );

        constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };
        static float4 clearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        if ( ImGui::ColorEdit3( "Wireframe Clear color", glm::value_ptr( clearColor ), colorEditFlags ) ) {
            //mEditorState->EditorSceneRenderer->SetWireframeClearColor( clearColor );
        }

        static float4 lineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        if ( ImGui::ColorEdit3( "Wireframe color", glm::value_ptr( lineColor ), colorEditFlags ) ) {
            //mEditorState->EditorSceneRenderer->SetWireframeLineColor( lineColor );
        }

        static float lineWidth{ 1.0f };
        if (gui::Slider( "Wireframe width", lineWidth, { 0.0f, 30.0f } ) ) {
            //mEditorState->EditorSceneRenderer->SetWireframeLineLineWidth( lineWidth );
        }

        // Infinite grid
        ImGui::SeparatorText( "Infinite Grid" );
        static bool enableGrid{ true };
        if ( gui::CheckBox( "Enable", enableGrid ) ) {
            //mEditorState->EditorSceneRenderer->EnableInfiniteGrid( enableGrid );
        }

        static float4 outerSquareColor{ 0.2f, 0.2f, 0.2f, 1.0f };
        if ( ImGui::ColorEdit4( "Outer square color", glm::value_ptr( outerSquareColor ), colorEditFlags ) ) {
            //mEditorState->EditorSceneRenderer->SetOuterSquareColor( outerSquareColor );
        }

        static float4 innerSquareColor{ 0.35f, 0.35f, 0.35f, 1.0f };
        if ( ImGui::ColorEdit4( "Inner square color", glm::value_ptr( innerSquareColor ), colorEditFlags ) ) {
            //mEditorState->EditorSceneRenderer->SetInnerSquareColor( innerSquareColor );
        }

        static float outerSquareWidth{ 1.5f };
        if ( gui::Slider( "Outer square width", outerSquareWidth, { 0.0f, 10.0f } ) ) {
            //mEditorState->EditorSceneRenderer->SetOuterSquareWidth( outerSquareWidth );
        }

        static float innerSquareWidth{ 0.5f };
        if ( gui::Slider( "Inner square width", innerSquareWidth, { 0.0f, 10.0f } ) ) {
            //mEditorState->EditorSceneRenderer->SetInnerSquareWidth( innerSquareWidth );
        }

        ImGui::SeparatorText( "Axis" );

        static float4 zAxisColor{ 0.0f, 0.0f, 1.0f, 1.0f };
        if ( ImGui::ColorEdit4( "Z Axis Color", glm::value_ptr( zAxisColor ), colorEditFlags ) ) {
            //mEditorState->EditorSceneRenderer->SetZAxisColor( zAxisColor );
        }

        static float4 xAxisColor{ 1.0f, 0.0f, 0.0f, 1.0f };
        if ( ImGui::ColorEdit4( "X Axis Color", glm::value_ptr( xAxisColor ), colorEditFlags ) ) {
            //mEditorState->EditorSceneRenderer->SetXAxisColor( xAxisColor );
        }

        static float zAxisWidth{ 5.0f };
        if ( gui::Slider( "Z Axis Width", zAxisWidth, { 0.0f, 10.0f } ) ) {
            //mEditorState->EditorSceneRenderer->SetZAxisWidth( zAxisWidth );
        }

        static float xAxisWidth{ 5.0f };
        if ( gui::Slider( "X Axis Width", xAxisWidth, { 0.0f, 10.0f } ) ) {
            //mEditorState->EditorSceneRenderer->SetXAxisWidth( xAxisWidth );
        }
    }

    auto RendererPanel::DrawSSAOSettings() -> void {
        gui::UnindentScoped und{};

        // Enable toggle
        if (gui::CheckBox( "##RendererPanel::DrawSSAOSettings::EnableSSAO", mEnableSSAO )) {
            //mEditorState->mSceneRenderer->SetEnableSSAO(mEnableSSAO);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted( "Enable SSAO" );

        ImGui::SameLine();
        ImGui::TextUnformatted( "Enable SSAO blur" );

        ImGui::Spacing();

        gui::Slider( "Radius", mSsaoRadius, { 0.05f, 5.0f } );
        gui::Slider( "Bias", mSsaoBias, { 0.0f,  0.1f } );

        if (gui::Slider("Power", mSsaoStrength, { 0.1f, 5.0f })) {
            //mEditorState->EditorSceneRenderer->SetSSAOIntensity( mSsaoStrength );
        }
    }

    auto RendererPanel::DrawPostProcessing() -> void {
        gui::UnindentScoped und{};

        static bool enableBloom{ false };
        if (gui::CheckBox( "##RendererPanel::DrawPostProcessing::Bloom", enableBloom )) {
            //mEditorState->mSceneRenderer->EnableBloom( enableBloom );
        }
        ImGui::SameLine();
        ImGui::TextUnformatted( "Enable bloom" );

        // Color saturation

        // ImGui::Spacing();
        // ImGui::Separator();
        // ImGui::Spacing();

        // static float contrast{ 1.0f };
        // if (gui::Slider( "Contrast", contrast, { 0.0f, 10.0f } ) ) {
        //     m_EditorState->EditorSceneRenderer->SetContrast( contrast );
        // }
        //
        // static float saturation{ 1.0f };
        // if (gui::Slider( "Saturation", saturation, { 0.0f, 10.0f } ) ) {
        //     m_EditorState->EditorSceneRenderer->SetSaturation( saturation );
        // }
        //
        // constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };
        //
        // ImGui::Spacing();
        // ImGui::Separator();
        // ImGui::Spacing();
        //
        // static Vec4F tintColor{ 0.1f, 0.2f, 0.3f, 1.0f };
        // if ( ImGui::ColorEdit3( "Tint", glm::value_ptr( tintColor ), colorEditFlags ) ) {
        //     m_EditorState->EditorSceneRenderer->SetTintColor( tintColor );
        // }
    }

    auto RendererPanel::DrawToneMapSettings() -> void {
        gui::UnindentScoped und{};

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        static float gamma{ 2.0f };
        static float exposure{ 1.0f };

        if (gui::Slider( "Gamma", gamma, { 0.0f, 10.0f } ) ) {
            //mEditorState->mSceneRenderer->SetImageGamma( gamma );
        }

        if (gui::Slider( "Exposure", exposure, { 0.0f, 10.0f } ) ) {
            //mEditorState->mSceneRenderer->SetImageExposure( exposure );
        }

        ImGui::Spacing();
        ImGui::Spacing();

        eastl::array<std::string, as<size_t>(ToneMappingType::Max_Count)> choices{
            "Linear", "Reinhard", "Uncharted2", "Aces", "Khronos Neutral"
        };

        static ToneMappingType toneMapType{ ToneMappingType::Aces };
        toneMapType = gui::Combo( choices, toneMapType );

        if (mPresentTargetType != PresentTargetType::eCount) {
            //mEditorState->mSceneRenderer->SetToneMapping( toneMapType );
        }

        ImGui::SameLine();
        ImGui::TextUnformatted( "ToneMap type" );
    }

    auto RendererPanel::DrawRayTracingSettings() -> void {
        gui::UnindentScoped und{};

        // Path tracing
        static bool enablePathTracing{ false };
        if (gui::CheckBox("##RendererPanel::PathTracing", enablePathTracing)) {
            // TODO:
        }

        gui::SetCursorHandOnLastItemHovered();
        ImGui::SameLine();
        ImGui::TextUnformatted("Enable Path Tracing");

        // Ray Traced Shadows
        static bool enableRTShadows{ false };
        if (gui::CheckBox("##RendererPanel::RTShadows", enableRTShadows)) {
            // TODO:
        }

        gui::SetCursorHandOnLastItemHovered();
        ImGui::SameLine();
        ImGui::TextUnformatted("Ray Traced Shadows");

        // Ray Traced Ambient Occlusion
        static bool enableRTAO{ false };
        if (gui::CheckBox("##RendererPanel::RTAO", enableRTAO)) {
            // TODO:
        }

        gui::SetCursorHandOnLastItemHovered();
        ImGui::SameLine();
        ImGui::TextUnformatted("Ray Traced Ambient Occlusion");

        // Ray Traced Reflections and Refractions
        static bool enableRTReflections{ false };
        if (gui::CheckBox("##RendererPanel::RTReflections", enableRTReflections)) {
            // TODO:
        }

        gui::SetCursorHandOnLastItemHovered();
        ImGui::SameLine();
        ImGui::TextUnformatted("Ray Traced Reflections");

        // Ray Traced Global Illumination
        static bool enableRTGI{ false };
        if (gui::CheckBox("##RendererPanel::RTGI", enableRTGI)) {
            // TODO:
        }

        gui::SetCursorHandOnLastItemHovered();
        ImGui::SameLine();
        ImGui::TextUnformatted("Ray Traced Global Illumination");
    }

    auto RendererPanel::DrawIBLSettings() -> void {
        gui::UnindentScoped und{};

        auto hdrDropTarget{
            [this]() -> void {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("HDR_LOAD_LIGHT_PANEL") }) {
                        eastl::string hdrPath{ *as<eastl::string*>( payload->Data ) };
                        //mEditorState->mSceneRenderer->UpdateEquirectangularMapAsync(hdrPath);
                        RuntimeConsole::Get()->Debug( string::Format("You dropped texture from HDR_LOAD_LIGHT_PANEL {}", hdrPath ) );
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        };

        TextureHandle textureHandle{};

        if ( ImGui::BeginTable( "HDRTextureSelector", 2,
                                ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersInnerH |
                                        ImGuiTableFlags_SizingStretchSame ) ) {
            ImGui::TableSetupColumn( "Thumbnail", ImGuiTableColumnFlags_None, 70.0f );
            ImGui::TableSetupColumn( "Info", ImGuiTableColumnFlags_None, 200.0f);

            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            TextureHandle displayedHandle{ textureHandle };
            if ( displayedHandle.IsEmpty() ) {
                displayedHandle = AssetsService::Get()->GetDummyTexture();
            }

            if ( gui::PushImageButton(
                         "##RendererPanel::DrawIBLSettings:DisplayTextureID",
                         ImGuiService::Get()->GetTextureID( displayedHandle ),
                         ImVec2{ 64, 64 } ) ) {
                // Clicked thumbnail
            }

            // Tooltip
            gui::ToolTip( [&]() {
                ImGui::TextUnformatted( "Click to load texture" );
            },ImGui::IsItemHovered() );

            hdrDropTarget();

            ImGui::TableSetColumnIndex( 1 );
            ShowTextureDetails( textureHandle.GetRaw() );

            ImGui::EndTable();
        }

        ImGui::Spacing();

        hdrDropTarget();

        constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

        ImGui::Spacing();

        static float4 clearColor{ 0.1f, 0.2f, 0.3f, 1.0f };
        if ( ImGui::ColorEdit3( "Clear", glm::value_ptr( clearColor ), colorEditFlags ) ) {
            mEditorState->mSceneRenderer->SetClearColor( clearColor );
        }
    }

    auto RendererPanel::DrawShadowMappingSettings() -> void {

    }
}