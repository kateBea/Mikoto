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

#include <memory>
#include <array>

#include <Core/RuntimeConsole.hh>

#include <ImGui/IconsMaterialDesign.h>

#include <GraphNodes/GraphEditor.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Panels/RendererPanel.hh>

#include <Common/String.hh>
#include <Layers/EditorLayer.hh>
#include <Renderer/Core/RenderService.hh>

namespace Mikoto {

    static auto ShowTextureDetails( const Texture* texture ) -> void {
        const UInt32 width  = texture ? texture->GetWidth()  : 0;
        const UInt32 height = texture ? texture->GetHeight() : 0;

        ImGui::TextUnformatted("Dimensions");
        ImGui::SameLine(90.0f);
        if (width == 0 || height == 0) {
            ImGui::TextUnformatted("N / A");
        } else {
            ImGui::TextUnformatted(fmt::format("[{}, {}]", width, height).c_str());
        }

        ImGui::TextUnformatted("Type");
        ImGui::SameLine(90.0f);
        if (texture) {
            if (auto cube{ dynamic_cast<const TextureCube*>(texture)}; cube || texture->IsHDR() ) {
                ImGui::TextUnformatted("HDR");
            }

            if (auto flatTexture{ dynamic_cast<const Texture2D*>(texture)} ) {
                ImGui::TextUnformatted("Texture_2D");
            }
        } else {
            ImGui::TextUnformatted("N / A");
        }

        ImGui::TextUnformatted("File Size");
        ImGui::SameLine(90.0f);
        if (texture) {
            ImGui::TextUnformatted(fmt::format("{} MB", Math::Round( static_cast<double>( texture->GetSizeBytes() ) / 1000'000.0, 2)).c_str());
        } else {
            ImGui::TextUnformatted("N / A");
        }

        ImGui::TextUnformatted("Format");
        ImGui::SameLine(90.0f);
        if (texture) {
            ImGui::TextUnformatted(GetTextureFormatString(texture->GetFormat()).data());
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
        if (texture) {
            ImGui::TextUnformatted(GetChanelString(texture->GetChannels()).data());
        } else {
            ImGui::TextUnformatted("N / A");
        }
    }

    RendererPanel::RendererPanel( const RendererPanelCreateInfo &info )
        : Panel{ "Renderer" }, m_EditorState{ info.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_POWER_SETTINGS_NEW, m_PanelName );
    }

    auto RendererPanel::OnUpdate( float timeStep ) -> void {
        if (!m_PanelIsVisible) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGui::SeparatorText( "FrameGraph Info" );

        ImGuiUtils::DrawNode( "Passes", [this] () -> void {
            DrawPassInfo();

            ImGuiUtils::CheckBox( "Show Pass Dependencies", m_ShowPassGraph );

            if (m_ShowPassGraph) {
                ShowExampleAppCustomNodeGraph( std::addressof( m_ShowPassGraph ) );
            }
        });

        ImGuiUtils::DrawNode( "Renderer", [this] () -> void {
            DrawRendererConfig();
        });

        ImGui::SeparatorText( "Renderer Settings" );
        ImGuiUtils::DrawNode( "Monitor", [this] () -> void {
            ImGuiUtils::UnindentScoped und{};

            if (ImGuiUtils::CheckBox( "##RendererPanel::OnUpdate::Vsync", m_EnableVSync )) {

            }
            ImGui::SameLine();
            ImGui::TextUnformatted( "Enable vsync" );
        });

        ImGui::SeparatorText( "Graphics Features" );
        ImGuiUtils::DrawNode( "Ambient Occlusion (SSAO) Settings", [this] () -> void {
            DrawSSAOSettings();
        });

        ImGui::SeparatorText( "Graphics Features" );
        ImGuiUtils::DrawNode( "Shadow Mapping settings", [this] () -> void {
            DrawShadowMappingSettings();
        });

        ImGuiUtils::DrawNode( "Image Based Lighting settings", [this] () -> void {
            DrawIBLSettings();
        });

        ImGui::End();
    }

    auto RendererPanel::IsVsyncEnabled() const -> bool {
        return RenderService::Get()->GetContext()->IsVsyncEnabled();
    }

    auto RendererPanel::EnableSkyboxLDR() const -> bool {
        return m_EnableSkyboxLDR;
    }

    auto RendererPanel::DrawPassInfo() -> void {
        ImGuiUtils::UnindentScoped und{};

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
        ImGuiUtils::UnindentScoped und{};

        ImGuiUtils::CheckBox( "##RendererPanel::DrawRendererConfig::wireframe", m_EditorState->ShowWireframe );
        ImGui::SameLine();
        ImGui::TextUnformatted( "Render wireframe" );

        constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };
        static Vec4F clearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        if ( ImGui::ColorEdit3( "Wireframe Clear color", glm::value_ptr( clearColor ), colorEditFlags ) ) {
            m_EditorState->EditorSceneRenderer->SetWireframeClearColor( clearColor );
        }

        static Vec4F lineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        if ( ImGui::ColorEdit3( "Wireframe color", glm::value_ptr( lineColor ), colorEditFlags ) ) {
            m_EditorState->EditorSceneRenderer->SetWireframeLineColor( lineColor );
        }

        static float lineWidth{ 1.0f };
        if (ImGuiUtils::Slider( "Wireframe width", lineWidth, { 0.0f, 30.0f } ) ) {
            m_EditorState->EditorSceneRenderer->SetWireframeLineLineWidth( lineWidth );
        }
    }

    auto RendererPanel::DrawSSAOSettings() -> void {
        ImGuiUtils::UnindentScoped und{};

        // Enable toggle
        if (ImGuiUtils::CheckBox( "##RendererPanel::DrawSSAOSettings::EnableSSAO", m_EnableSSAO )) {
            m_EditorState->EditorSceneRenderer->SetEnableSSAO(m_EnableSSAO);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted( "Enable SSAO" );

        ImGui::Spacing();

        ImGuiUtils::Slider( "Radius", m_SSAORadius, { 0.05f, 5.0f } );
        ImGuiUtils::Slider( "Bias", m_SSAOBias, { 0.0f,  0.1f } );
        ImGuiUtils::Slider( "Power", m_SSAOStrength, { 0.1f,  3.0f } );
    }

    auto RendererPanel::DrawIBLSettings() -> void {
        ImGuiUtils::UnindentScoped und{};

        auto usingConvolutedCube{ m_EditorState->EditorSceneRenderer->IsUsingConvolutedCube() };
        if (ImGuiUtils::CheckBox( "##RendererPanel::DrawIBLSettings::UseConv", usingConvolutedCube )) {
            m_EditorState->EditorSceneRenderer->SetUseConvolutedCube(usingConvolutedCube);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted( "Use convoluted cube" );

        ImGuiUtils::CheckBox( "##RendererPanel::DrawRendererConfig::LDR", m_EnableSkyboxLDR );
        ImGui::SameLine();
        ImGui::TextUnformatted( "Use CubeMap LDR" );

        auto hdrDropTarget{
            [this]() -> void {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("HDR_LOAD_LIGHT_PANEL") }) {
                        std::string hdrPath{ *static_cast<std::string*>( payload->Data ) };
                        m_EditorState->EditorSceneRenderer->UpdateEquirectangularMapAsync(hdrPath);
                        RuntimeConsole::Get()->Debug( StringUtil::Format("You dropped texture from HDR_LOAD_LIGHT_PANEL {}", hdrPath ) );
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        };

        TextureHandle textureHandle{ m_EditorState->EditorSceneRenderer->GetEquirectangularMap() };

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

            if ( ImGuiUtils::PushImageButton(
                         displayedHandle->GetHandle(),
                         ImGuiService::Get()->GetTextureID( displayedHandle ),
                         ImVec2{ 64, 64 } ) ) {
                // Clicked thumbnail
            }

            // Tooltip
            ImGuiUtils::ToolTip( [&]() {
                ImGui::TextUnformatted( "Click to load texture" );
                if ( !textureHandle.IsEmpty() )
                    ImGui::TextUnformatted( textureHandle->GetTextureUri().c_str() );
            },
                                 ImGui::IsItemHovered() );

            hdrDropTarget();

            ImGui::TableSetColumnIndex( 1 );
            ShowTextureDetails( textureHandle.GetRaw() );

            ImGui::EndTable();
        }

        ImGui::Spacing();

        if (!textureHandle.IsEmpty()) {
            ImGuiUtils::InputText(StringUtil::Format( "{}", textureHandle->GetTextureUri() ), true );
        } else {
            ImGuiUtils::InputText(StringUtil::Format( "Drag EquirectangularMap here" ), true );
        }

        ImGui::Spacing();

        static  std::array<std::string, 2> backgroundTypes{
            "Skybox", "Clear color"
        };

        const SceneBackground current{ m_EditorState->ActiveEditorScene->GetSceneBackground() };
        const SceneBackground selection{ ImGuiUtils::Combo( backgroundTypes, current ) };

        m_EditorState->ActiveEditorScene->SetSceneBackground( selection );

        ImGui::Spacing();

        hdrDropTarget();

        ImGui::Spacing();

        float gamma    { m_EditorState->ActiveEditorScene->GetGamma() };
        float exposure { m_EditorState->ActiveEditorScene->GetExposure() };

        if (ImGuiUtils::Slider( "Gamma", gamma, { 0.0f, 10.0f } ) ) { m_EditorState->ActiveEditorScene->SetGamma( gamma ); }
        if (ImGuiUtils::Slider( "Exposure", exposure, { 0.0f, 10.0f } ) ) { m_EditorState->ActiveEditorScene->SetExposure( exposure ); }

        constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

        ImGui::Spacing();

        static Vec4F clearColor{ 0.1f, 0.2f, 0.3f, 1.0f };
        if ( ImGui::ColorEdit3( "Clear", glm::value_ptr( clearColor ), colorEditFlags ) ) {
            m_EditorState->EditorSceneRenderer->SetClearColor( clearColor );
        }

        static float lineWidth{ 9.0f };
        if (ImGuiUtils::Slider( "Max ReflectionLOD", lineWidth, { 0.0f, 30.0f } ) ) {
            m_EditorState->EditorSceneRenderer->SetMaxReflectionLOD( lineWidth );
        }

    }

    auto RendererPanel::DrawShadowMappingSettings() -> void {
        std::array<std::string, static_cast<Size>(FinalCompositionTarget::ENUM_MAX)> choices{
            "Color", "Normals", "Position", "Final Image"
        };

        // These are directly taken from the core passes from the Scene renderer
        std::array<std::string, static_cast<Size>(FinalCompositionTarget::ENUM_MAX)> images{
            "GBuffer_Color", "GBuffer_Normal", "GBuffer_Position", "FinalShadingPass_ColorTarget"
        };

        m_FinalCompositionTarget = ImGuiUtils::Combo( choices, m_FinalCompositionTarget );

        if (m_FinalCompositionTarget != FinalCompositionTarget::ENUM_MAX) {
            m_EditorState->FinalComposition = m_EditorState->EditorSceneRenderer->GetTexture( images[static_cast<Size>(m_FinalCompositionTarget)] );
        }
    }
}