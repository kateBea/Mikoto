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

#include <imgui.h>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Filesystem/File.hh>

#include <Platform/CoreEvents.hh>

#include <Assets/AssetRegistryService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>

#include <Layers/TextDebugLayer.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace renderer;
    using namespace renderer::rhi;
    using namespace scene;

    TextDebugLayer::TextDebugLayer()
        : mName{ "Text Rendering Debug" } {
    }

    auto TextDebugLayer::OnCreate() -> void {
        try {
            mFonts[0] = asset::AssetRegistryService::Get()->LoadAsset<renderer::Font>( filesystem::Path{ "Resources/Fonts/Noto_Sans_JP/static/NotoSansJP-Regular.ttf" } );
            mFonts[1] = asset::AssetRegistryService::Get()->LoadAsset<renderer::Font>( filesystem::Path{ "Resources/Fonts/Inter/static/Inter-Regular.ttf" } );

            if ( !mFonts[0] || !mFonts[1] ) {
                MKT_THROW_RUNTIME_ERROR( "Text debug fonts could not be loaded." );
            }
            // Keep diagnostic entities out of the user's project and save files.
            mScene = eastl::make_unique<scene::Scene>( "Unicode and Emoji Debug" );

            for ( core::usize column{}; column < mEntities.size(); ++column ) {
                auto* entity{ mScene->CreateEntity( column == 0 ? "MSDF" : "Slug" ) };
                mEntities[column] = entity->Get();
                auto& component{ entity->AddComponent<scene::TextComponent>() };
                component.SetIsWorldText( false );
                component.SetBackend( column == 0 ? scene::TextBackend::eMSDF : scene::TextBackend::eSlug );
            }

            mCamera.SetViewportSize( 1280.0f, 720.0f );
            mCamera.LookAt( { 0, 0, 5 }, { 0, 0, 0 } );
            mRenderer = ISceneRenderer::Create()
                .SetName( "Text Debug" )
                .SetDevice( renderer::RenderSystem::Get()->GetGpuDevice() )
                .SetRenderResolution( renderer::rhi::RenderResolution::e720P )
                .Build();
            mRenderer->SetEnvironmentLightingEnabled( false );
            mRenderer->SetEnableInfiniteGrid( false );
            mRenderer->SetEnableSsao( false );
            mRenderer->SetEditorOverlaysEnabled( false );
            mRenderer->Initialize();
            mRenderer->SetMainCamera( &mCamera );
            ReloadSample();
            mScene->Update( 0.0f );
        } catch ( const std::exception& error ) {
            mError = error.what();
        }
    }

    auto TextDebugLayer::ReloadSample() -> void {
        // An owned snapshot bypasses cached contents so Reload always reflects
        // the user's latest edit without changing the currently open project.
        filesystem::File sample{ "Resources/Text samples/Unicode and emoji.txt" };
        eastl::string contents{ sample.GetContentsString() };
        // Accept UTF-8 files saved with a BOM, without drawing it as a glyph.
        if ( contents.size() >= 3 && contents.compare( 0, 3, "\xEF\xBB\xBF" ) == 0 ) {
            contents.erase( 0, 3 );
        }

        if ( contents.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "The Unicode sample file is empty or missing." );
        }

        mSample = eastl::move( contents );

        if ( mSample.back() != '\n' ) {
            mSample += '\n';
        }

        mSampleLines = core::as<core::u32>( eastl::count( mSample.begin(), mSample.end(), '\n' ) );
        UpdateText();
    }

    auto TextDebugLayer::UpdateText() -> void {
        // Deliberate line breaks keep the two columns comparable. The repeat
        // count stresses atlas paging and upload capacity without adding entities.
        mContents.clear();

        for ( core::i32 repeat{}; repeat < mRepetitions; ++repeat ) {
            mContents += mSample;
        }

        for ( core::usize column{}; column < mEntities.size(); ++column ) {
            auto& component{ mScene->GetRegistry().get<scene::TextComponent>( mEntities[column] ) };
            component.SetFont( mFonts[mFontIndex] );
            component.SetContents( mContents );
            component.SetSize( mFontSize );
            component.SetLineSpacing( 1.25f );
            component.SetColor( core::float4{ 0.8f, 0.9f, 1.0f, mOpacity } );
            component.SetOutlineWidth( mOutlineWidth );
            component.SetOutlineColor( mOutlineColor );
            component.GetScreenPlacement().position = { 16.0f + core::as<core::f32>( column ) * 640.0f, 12.0f - mScroll };
        }
    }

    auto TextDebugLayer::OnUpdate( core::f32 ) -> void {
        if ( !mVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 1100, 760 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( imgui::widget::LocalizedLabel( "text_debug_layer_text_rendering_debug_f10", "Text Rendering Debug (F10)" ).c_str(), &mVisible ) ) {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled( "Resources/Text samples/Unicode and emoji.txt" );

        if ( mRenderer && ImGui::Button( imgui::widget::LocalizedLabel( "text_debug_layer_reload_sample", "Reload sample" ).c_str() ) ) {
            try {
                ReloadSample();
                mError.clear();
            } catch ( const std::exception& error ) {
                mError = error.what();
            }
        }

        if ( !mError.empty() ) {
            ImGui::TextWrapped( "%s", mError.c_str() );
        }
        else if ( mRenderer ) {
            bool changed{ ImGui::Combo( imgui::widget::LocalizedLabel( "text_debug_layer_font", "Font" ).c_str(), &mFontIndex, "Noto Sans JP\0Inter\0" ) };
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "text_debug_layer_font_size", "Font size" ).c_str(), &mFontSize, 10.0f, 24.0f );
            changed |= ImGui::SliderInt( imgui::widget::LocalizedLabel( "text_debug_layer_passage_repetitions", "Passage repetitions" ).c_str(), &mRepetitions, 1, 60 );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "text_debug_layer_opacity", "Opacity" ).c_str(), &mOpacity, 0.0f, 1.0f );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "text_debug_layer_outline_width", "Outline width" ).c_str(), &mOutlineWidth, 0.0f, 0.125f, "%.3f em<" );// units: em's?
            changed |= ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "text_debug_layer_outline_colour", "Outline colour" ).c_str(), &mOutlineColor.x );
            const core::f32 maximumScroll{ eastl::max( 0.0f, core::as<core::f32>( mFonts[mFontIndex]->GetLineHeight() ) * mFontSize * 1.25f * mSampleLines * mRepetitions - 660.0f ) };
            mScroll = eastl::min( mScroll, maximumScroll );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "text_debug_layer_scroll", "Scroll" ).c_str(), &mScroll, 0.0f, maximumScroll, "%.0f px" );

            if ( changed ) {
                UpdateText();
            }

            ImGui::TextUnformatted( MKT_LOC( "text_debug_layer_msdf_left_slug_right_colour_emoji_should_match_in_both" ).c_str() );
            ImGui::TextWrapped( MKT_LOC( "text_debug_layer_font_coverage_differs_inter_may_show_replacements_for_japanese_general" ).c_str() );

            try {
                mRenderer->Render( mScene.get() );
                const auto output{ mRenderer->GetFinalImage() };
                const auto available{ ImGui::GetContentRegionAvail() };
                const core::f32 width{ eastl::min( available.x, available.y * 16.0f / 9.0f ) };

                if ( output && width > 0 ) {
                    ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( output ), { width, width * 9.0f / 16.0f } );
                }
            } catch ( const std::exception& error ) {
                mError = error.what();
            }
        }

        ImGui::End();
    }

    auto TextDebugLayer::OnRender( float ) -> void {
    }

    auto TextDebugLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto TextDebugLayer::OnEvent( platform::Event& event ) -> void {
        if ( event.IsType( platform::EventType::KEY_PRESSED_EVENT ) &&
             static_cast<const platform::KeyPressedEvent&>( event ).GetKeyCode() == platform::KeyCode::Key_F10 ) {
            mVisible = !mVisible;
            event.SetHandled( true );
        }
    }

    auto TextDebugLayer::OnDestroy() -> void {
        if ( mRenderer ) {
            renderer::RenderSystem::Get()->GetGpuDevice()->WaitIdle();
            mRenderer->Shutdown();
            mRenderer.reset();
        }

        mScene.reset();
        mFonts = {};
    }
}// namespace mikoto::editor
