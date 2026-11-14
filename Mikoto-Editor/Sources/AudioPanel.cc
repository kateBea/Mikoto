// Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0.

#include <mutex>

#include <EASTL/algorithm.h>

#include <glm/gtc/constants.hpp>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/Utility.hh>

#include <Renderer/Core/ThumbnailRenderer.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/AudioPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    static auto BeginAudioProperties( core::cstr id ) -> bool {
        if ( !ImGui::BeginTable( id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings ) ) {
            return false;
        }

        // Keep labels and controls aligned across the preview sections.
        ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 10.0f );
        ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
        return true;
    }

    static auto AudioPropertyRow( eastl::string_view label ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( label.data(), label.data() + label.size() );

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth( -FLT_MIN );
    }

    struct AudioPanel::WaveformLoad {
        std::mutex mMutex{};
        asset::ImageHandle mImage{};
        eastl::string mError{};
        bool mComplete{};
    };

    auto DrawAudioDistanceCurves( audio::SourceSettings& settings ) -> bool {
        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "audio_curves", "AudioDistanceCurves" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return false;
        }

        ImGui::PushID( "AudioDistanceCurves" );

        const eastl::string titles[]{ MKT_LOC( "audio_volume" ), MKT_LOC( "audio_blend" ), MKT_LOC( "audio_spread" ), MKT_LOC( "audio_lowpass" ), MKT_LOC( "audio_reverb" ) };
        const char* names[]{ titles[0].c_str(), titles[1].c_str(), titles[2].c_str(), titles[3].c_str(), titles[4].c_str() };
        constexpr ImU32 colors[]{
            IM_COL32( 230, 108, 91, 255 ),
            IM_COL32( 85, 191, 117, 255 ),
            IM_COL32( 89, 164, 223, 255 ),
            IM_COL32( 191, 111, 211, 255 ),
            IM_COL32( 203, 191, 97, 255 )
        };

        auto* storage{ ImGui::GetStateStorage() };
        const auto curveID{ ImGui::GetID( "SelectedDistanceCurve" ) };

        core::i32 selected{ storage->GetInt( curveID, 0 ) };

        const eastl::string interpolationNames[]{ MKT_LOC( "audio_curve_linear" ),
            MKT_LOC( "audio_curve_smooth" ) };
        const char* interpolationLabels[]{ interpolationNames[0].c_str(), interpolationNames[1].c_str() };
        bool changed{};

        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2{ 0.0f, ImGui::GetStyle().CellPadding.y } );

        if ( BeginAudioProperties( "CurveSettings" ) ) {
            AudioPropertyRow( MKT_LOC( "audio_curve" ) );

            if ( ImGui::Combo( "##Curve", &selected, names, IM_ARRAYSIZE( names ) ) ) {
                storage->SetInt( curveID, selected );
            }

            auto& curve{ settings.mCurves[selected] };
            int interpolation{ static_cast<int>( curve.mInterpolation ) };
            AudioPropertyRow( MKT_LOC( "audio_curve_interpolation" ) );

            if ( ImGui::Combo( "##Interpolation", &interpolation, interpolationLabels, IM_ARRAYSIZE( interpolationLabels ) ) ) {
                curve.mInterpolation = static_cast<audio::CurveInterpolation>( interpolation );
                changed = true;
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        auto& curve{ settings.mCurves[selected] };
        changed |= imgui::CheckBox( MKT_LOC( "audio_enable_curve" ).c_str(), curve.mEnabled );

        ImGui::SameLine();

        if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "inspector_reset", "Reset" ).c_str() ) ) {
            curve = {};
            const float initial{ selected == 2 || selected == 4 ? 0.f : 1.f };
            curve.mPoints[0].y = curve.mPoints[1].y = initial;
            changed = true;
        }

        if ( selected == 0 ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "audio_volume_help" ).c_str() );
        }

        if ( selected == 1 ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "audio_blend_help" ).c_str() );
        }

        if ( selected == 2 ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "audio_spread_help" ).c_str() );
        }

        if ( selected == 3 ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "audio_lowpass_help" ).c_str() );
        }

        if ( selected == 4 ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "audio_reverb_help" ).c_str() );
        }

        const auto origin{ ImGui::GetCursorScreenPos() };
        const ImVec2 size{ glm::max( ImGui::GetContentRegionAvail().x, 100.f ), 190 };
        ImGui::InvisibleButton( "##DistanceGraph", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight );
        const bool hovered{ ImGui::IsItemHovered() };
        const bool active{ ImGui::IsItemActive() };
        const ImVec2 start{ origin.x + 28, origin.y + 12 }, extent{ size.x - 40, size.y - 38 };
        const auto pixel = [&]( core::float2 p ) {
            return ImVec2{ start.x + p.x * extent.x, start.y + ( 1 - p.y ) * extent.y };
        };
        const auto mouse{ ImGui::GetIO().MousePos };
        const core::float2 value{ glm::clamp( ( mouse.x - start.x ) / extent.x, 0.f, 1.f ), glm::clamp( 1 - ( mouse.y - start.y ) / extent.y, 0.f, 1.f ) };
        int nearest{ -1 };
        float best{ 81 };

        for ( core::u32 i{}; i < curve.mCount; ++i ) {
            const auto p{ pixel( curve.mPoints[i] ) };
            const float d{ ( p.x - mouse.x ) * ( p.x - mouse.x ) + ( p.y - mouse.y ) * ( p.y - mouse.y ) };

            if ( d < best ) {
                best = d;
                nearest = int( i );
            }
        }

        const auto dragID{ ImGui::GetID( "DraggedDistancePoint" ) };

        if ( hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) {
            storage->SetInt( dragID, nearest );
        }

        if ( hovered && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) && nearest < 0 && curve.mCount < audio::DistanceCurve::kMaxPoints ) {
            curve.mPoints[curve.mCount++] = value;
            curve.Sanitize();
            changed = true;
        }
        else if ( active && ImGui::IsMouseDragging( ImGuiMouseButton_Left, 0 ) ) {
            const int point{ storage->GetInt( dragID, -1 ) };

            if ( point >= 0 && point < int( curve.mCount ) ) {
                auto& p{ curve.mPoints[point] };
                p.y = value.y;

                if ( point > 0 && point + 1 < int( curve.mCount ) ) {
                    p.x = glm::clamp( value.x, curve.mPoints[point - 1].x, curve.mPoints[point + 1].x );
                }

                changed = true;
            }
        }

        if ( hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) && nearest > 0 && nearest + 1 < int( curve.mCount ) ) {
            for ( core::u32 i{ core::as<core::u32>( nearest ) }; i + 1 < curve.mCount; ++i ) {
                curve.mPoints[i] = curve.mPoints[i + 1];
            }

            --curve.mCount;
            changed = true;
        }

        auto* draw{ ImGui::GetWindowDrawList() };
        draw->AddRectFilled( origin, { origin.x + size.x, origin.y + size.y }, ImGui::GetColorU32( ImGuiCol_FrameBg ), 3 );

        for ( int i{}; i <= 4; ++i ) {
            const float t{ i * .25f };
            draw->AddLine( pixel( { t, 0 } ), pixel( { t, 1 } ), IM_COL32( 95, 95, 95, 80 ) );
            draw->AddLine( pixel( { 0, t } ), pixel( { 1, t } ), IM_COL32( 95, 95, 95, 80 ) );
            const auto label{ string::Format( "{:.0f}", settings.mMaxDistance * t ) };
            draw->AddText( { start.x + extent.x * t - 8, start.y + extent.y + 5 }, ImGui::GetColorU32( ImGuiCol_TextDisabled ), label.c_str() );
        }

        draw->AddText( { origin.x + 6, start.y }, ImGui::GetColorU32( ImGuiCol_TextDisabled ), "1" );
        draw->AddText( { origin.x + 6, start.y + extent.y - 12 }, ImGui::GetColorU32( ImGuiCol_TextDisabled ), "0" );

        for ( size_t c{}; c < settings.mCurves.size(); ++c ) {
            const auto& plotted{ settings.mCurves[c] };

            if ( !plotted.mEnabled && int( c ) != selected ) {
                continue;
            }

            const ImU32 color{ plotted.mEnabled ? colors[c] : IM_COL32( 140, 140, 140, 150 ) };
            // Sample the same evaluator used by the audio thread.
            const int segments{ glm::max( 1, static_cast<int>( extent.x / 2.f ) ) };
            auto previous{ pixel( { 0.f, plotted.Evaluate( 0.f ) } ) };

            for ( int p{ 1 }; p <= segments; ++p ) {
                const float distance{ static_cast<float>( p ) / segments };
                const auto next{ pixel( { distance, plotted.Evaluate( distance ) } ) };
                draw->AddLine( previous, next, color, 1.5f );
                previous = next;
            }

            if ( core::as<int>( c ) == selected ) {
                for ( core::u32 p{}; p < plotted.mCount; ++p ) {
                    draw->AddCircleFilled( pixel( plotted.mPoints[p] ), 4, color );
                }
            }
        }

        const float legendRight{ ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x };

        for ( size_t c{}; c < settings.mCurves.size(); ++c ) {
            const auto labelSize{ ImGui::CalcTextSize( names[c] ) };

            if ( c && ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + labelSize.x < legendRight ) {
                ImGui::SameLine();
            }

            ImGui::TextColored( ImGui::ColorConvertU32ToFloat4( colors[c] ), "%s", names[c] );
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "audio_graph_help" ).c_str() );
        ImGui::PopID();
        return changed;
    }


    AudioPanel::AudioPanel( const AudioPanelCreateInfo& description )
        : Panel{ "Audio", "Panel", description.mGroup, description.mIsToggleable },
          mState{ description.mState },
          mDevice{ description.mDevice } {
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_AUDIOTRACK, mPanelName );
        mPanelIsVisible = false;
    }

    AudioPanel::~AudioPanel() {
        ClearClip();
        mListener.reset();
        mAudioDevice.reset();
    }

    auto AudioPanel::ClearClip() -> void {
        if ( mSource ) {
            mSource->Stop();
        }

        mSource = {};
        mWaveformLoad.reset();
        mWaveform = {};
        mPath = {};
        mError.clear();
    }

    auto AudioPanel::LoadClip( const filesystem::Path& path ) -> void {
        ClearClip();
        mPath = path;

        if ( path.IsEmpty() ) {
            return;
        }

        RequestWaveform();

        try {
            if ( !asset::IsFileAudio( path ) || !path.IsFile() ) {
                MKT_THROW_RUNTIME_ERROR( "Choose an existing audio file." );
            }

            if ( !mAudioDevice ) {
                auto device{ audio::AudioDevice::Create( audio::AudioDeviceDescription{ .mMaxListeners = 1 } ) };
                device->Initialize();
                auto listener{ eastl::make_unique<audio::AudioListener>( 0, 0.0f, 0.0f, 0.0f, device.get() ) };
                listener->SetEnabled( true );
                mAudioDevice = eastl::move( device );
                mListener = eastl::move( listener );
            }

            mSource = mAudioDevice->CreateSource( path );

            if ( !mSource ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot create the audio preview source." );
            }

            mSource->SetSettings( mSettings );
            mSettings = mSource->GetSettings();
            mSource->SetListener( mListener.get() );
            ApplyPreviewPose();
        } catch ( const std::exception& error ) {
            mError = error.what();
        }
    }

    auto AudioPanel::RequestWaveform() -> void {
        auto load{ eastl::make_shared<WaveformLoad>() };
        mWaveformLoad = load;
        // The job owns only its mailbox and file path. Switching clips or closing
        // the panel never gives a late result access to a different preview.
        auto work{ [load, path = mPath] {
            asset::ImageHandle image{};
            eastl::string error{};

            try {
                image = renderer::ThumbnailRenderer::RenderAudioPreview( path, 2048, 384 );
            } catch ( const std::exception& exception ) {
                error = exception.what();
            }

            const std::lock_guard lock{ load->mMutex };
            load->mImage = eastl::move( image );
            load->mError = eastl::move( error );
            load->mComplete = true;
        } };

        if ( auto* tasks{ threading::TaskService::GetPtr() }; tasks && tasks->IsInitialized() ) {
            tasks->ExecuteWorkerTask( eastl::move( work ) );
        }
        else {
            work();
        }
    }

    auto AudioPanel::PollWaveform() -> void {
        const auto load{ mWaveformLoad };

        if ( !load ) {
            return;
        }

        asset::ImageHandle image{};
        {
            const std::lock_guard lock{ load->mMutex };

            if ( !load->mComplete ) {
                return;
            }

            image = eastl::move( load->mImage );

            if ( !load->mError.empty() ) {
                if ( !mError.empty() ) {
                    mError += "\n";
                }

                mError += load->mError;
            }
        }

        mWaveformLoad.reset();

        if ( image && mDevice ) {
            try {
                mWaveform = mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
                    .SetWidth( image->mWidth )
                    .SetHeight( image->mHeight )
                    .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                    .SetFormat( renderer::rhi::Format::eRGBA8_UNORM )
                    .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                    .SetImageData( image ) );
            } catch ( const std::exception& error ) {
                mError = error.what();
            }
        }
    }

    auto AudioPanel::ApplyPreviewPose() -> void {
        if ( !mSource || !mListener ) {
            return;
        }

        const auto yaw{ glm::radians( mListenerAngles.x ) };
        const auto pitch{ glm::radians( mListenerAngles.y ) };
        const core::float3 forward{ glm::sin( yaw ) * glm::cos( pitch ), glm::sin( pitch ), -glm::cos( yaw ) * glm::cos( pitch ) };
        mListener->SetPosition( mListenerPosition );
        mListener->SetVelocity( mListenerVelocity );
        mListener->SetOrientation( forward.x, forward.y, forward.z, 0.0f, 1.0f, 0.0f );
        mListener->Apply();
        mSource->SetPosition( mSourcePosition );
        mSource->SetVelocity( mSourceVelocity );
    }

    auto AudioPanel::DrawWaveform() -> void {
        const ImVec2 size{ glm::max( 1.0f, ImGui::GetContentRegionAvail().x ),
            glm::clamp( ImGui::GetContentRegionAvail().x * 0.20f, 120.0f, 280.0f ) };
        const auto start{ ImGui::GetCursorScreenPos() };
        // An image alone has no active ID. Capture the press for scrubbing until release,
        // including when the pointer leaves the waveform during a drag.
        ImGui::InvisibleButton( "##AudioWaveform", size, ImGuiButtonFlags_MouseButtonLeft );

        if ( mWaveform ) {
            ImGui::GetWindowDrawList()->AddImage( imgui::ImGuiService::Get()->GetTextureID( mWaveform ),
                start, { start.x + size.x, start.y + size.y } );
        }
        else {
            ImGui::GetWindowDrawList()->AddRectFilled( start, { start.x + size.x, start.y + size.y }, IM_COL32( 0, 0, 0, 255 ) );
            const auto& message{ MKT_LOC( mWaveformLoad ? "audio_panel_loading_waveform" : "audio_panel_choose_clip" ) };
            ImGui::GetWindowDrawList()->AddText( { start.x + 12, start.y + 12 }, ImGui::GetColorU32( ImGuiCol_Text ), message.c_str() );
        }

        if ( mSource && mSource->GetAudioDuration() > 0.0f ) {
            const auto duration{ mSource->GetAudioDuration() };

            if ( ImGui::IsItemHovered() || ImGui::IsItemActive() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            if ( ImGui::IsItemActive() && ImGui::IsMouseDown( ImGuiMouseButton_Left ) ) {
                mSource->Seek( glm::clamp( ( ImGui::GetMousePos().x - start.x ) / size.x, 0.0f, 1.0f ) * duration );
            }

            const auto position{ glm::clamp( mSource->GetCurrentProgress() / duration, 0.0f, 1.0f ) };
            const auto x{ start.x + position * size.x };
            ImGui::GetWindowDrawList()->AddLine( { x, start.y }, { x, start.y + size.y }, ImGui::GetColorU32( ImGuiCol_PlotHistogram ), 2.0f );
        }
    }

    auto AudioPanel::DrawPlayback() -> bool {
        bool changed{};

        if ( !ImGui::CollapsingHeader( MKT_LOC( "inspector_playback" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return false;
        }

        // Nested circular sliders share the outer table's control column.
        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2{ 0.0f, ImGui::GetStyle().CellPadding.y } );
        ImGui::BeginDisabled( !mSource );
        ImGui::BeginDisabled( mSource && mSource->IsPlaying() );

        if ( ImGui::Button( MKT_LOC( mSource && mSource->IsPaused() ? "inspector_resume" : "inspector_play" ).c_str() ) && mSource ) {
            mSource->Play();
        }

        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled( !mSource || !mSource->IsPlaying() );

        if ( ImGui::Button( MKT_LOC( "inspector_pause" ).c_str() ) && mSource ) {
            mSource->Pause();
        }

        ImGui::EndDisabled();
        ImGui::SameLine();

        if ( ImGui::Button( MKT_LOC( "inspector_stop" ).c_str() ) && mSource ) {
            mSource->Stop();
        }

        const auto duration{ mSource ? mSource->GetAudioDuration() : 0.0f };
        auto cursor{ mSource ? mSource->GetCurrentProgress() : 0.0f };

        if ( BeginAudioProperties( "PlaybackCursor" ) ) {
            AudioPropertyRow( MKT_LOC( "inspector_position_s" ) );

            if ( imgui::widget::CircleSlider( "##Position", &cursor, 0.0f, glm::max( duration, 0.001f ), "%.2f" ) && mSource ) {
                mSource->Seek( cursor );
            }

            ImGui::EndTable();
        }

        ImGui::TextDisabled( MKT_LOC( "inspector_seconds" ).c_str(), cursor, duration );
        ImGui::EndDisabled();
        changed |= imgui::CheckBox( MKT_LOC( "inspector_loop" ).c_str(), mSettings.mLooping );
        ImGui::SameLine();
        changed |= imgui::CheckBox( MKT_LOC( "inspector_mute" ).c_str(), mSettings.mMuted );

        if ( BeginAudioProperties( "PlaybackSettings" ) ) {
            AudioPropertyRow( MKT_LOC( "inspector_gain" ) );
            changed |= imgui::widget::CircleSlider( "##Gain", &mSettings.mVolume, 0.0f, audio::AudioSource::GetMaxVolume(), "%.2fx" );

            AudioPropertyRow( MKT_LOC( "inspector_pitch" ) );
            changed |= imgui::widget::CircleSlider( "##Pitch", &mSettings.mPitch, 0.01f, 4.0f, "%.2fx" );

            AudioPropertyRow( MKT_LOC( "audio_pan" ) );
            changed |= imgui::widget::CircleSlider( "##Pan", &mSettings.mPan, -1.0f, 1.0f, "%.2f" );
            ImGui::EndTable();
        }

        ImGui::PopStyleVar();
        return changed;
    }


    auto AudioPanel::DrawSpatialSettings() -> bool {
        if ( !ImGui::CollapsingHeader( MKT_LOC( "audio_settings_3d" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return false;
        }

        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2{ 0.0f, ImGui::GetStyle().CellPadding.y } );

        bool changed{ imgui::CheckBox( MKT_LOC( "inspector_3d_headphone_spatialization" ).c_str(), mSettings.mSpatialized ) };
        ImGui::BeginDisabled( !mSettings.mSpatialized );

        if ( BeginAudioProperties( "SpatialSettings" ) ) {
            AudioPropertyRow( MKT_LOC( "inspector_distance_model" ) );
            core::i32 attenuation{ core::as<core::i32>( mSettings.mAttenuation ) };

            if ( ImGui::Combo( "##DistanceModel", &attenuation, MKT_LOC( "inspector_none_inverse_linear_exponential" ).c_str() ) ) {
                mSettings.mAttenuation = core::as<audio::Attenuation>( attenuation );
                changed = true;
            }

            ImGui::BeginDisabled( mSettings.mAttenuation == audio::Attenuation::eNone || mSettings.mCurves[0].mEnabled );

            AudioPropertyRow( MKT_LOC( "inspector_min_distance" ) );
            changed |= imgui::widget::CircleSlider( "##MinDistance", &mSettings.mMinDistance, 0.01f, 9999.0f, "%.2f" );

            AudioPropertyRow( MKT_LOC( "inspector_rolloff" ) );
            changed |= imgui::widget::CircleSlider( "##Rolloff", &mSettings.mRolloff, 0.0f, 10.0f, "%.2f" );
            ImGui::EndDisabled();

            AudioPropertyRow( MKT_LOC( "inspector_max_distance" ) );
            changed |= imgui::widget::CircleSlider( "##MaxDistance", &mSettings.mMaxDistance, mSettings.mMinDistance + 0.01f, 10000.0f, "%.2f" );

            AudioPropertyRow( MKT_LOC( "inspector_doppler" ) );
            changed |= imgui::widget::CircleSlider( "##Doppler", &mSettings.mDoppler, 0.0f, 5.0f, "%.2f" );
            ImGui::EndTable();
        }

        if ( ImGui::CollapsingHeader( MKT_LOC( "audio_panel_listener_properties" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            ImGui::SeparatorText( MKT_LOC( "audio_panel_source" ).c_str() );

            if ( BeginAudioProperties( "PreviewSource" ) ) {
                AudioPropertyRow( MKT_LOC( "audio_panel_position" ) );
                ImGui::DragFloat3( "##Position", &mSourcePosition.x, 0.1f );

                AudioPropertyRow( MKT_LOC( "audio_panel_velocity" ) );
                ImGui::DragFloat3( "##Velocity", &mSourceVelocity.x, 0.1f );
                ImGui::EndTable();
            }

            ImGui::SeparatorText( MKT_LOC( "audio_panel_listener" ).c_str() );

            if ( BeginAudioProperties( "PreviewListener" ) ) {
                AudioPropertyRow( MKT_LOC( "audio_panel_position" ) );
                ImGui::DragFloat3( "##Position", &mListenerPosition.x, 0.1f );

                AudioPropertyRow( MKT_LOC( "audio_panel_velocity" ) );
                ImGui::DragFloat3( "##Velocity", &mListenerVelocity.x, 0.1f );

                AudioPropertyRow( MKT_LOC( "audio_panel_yaw" ) );
                imgui::widget::CircleSlider( "##Yaw", &mListenerAngles.x, -180.0f, 180.0f, "%.1f" );

                AudioPropertyRow( MKT_LOC( "audio_panel_pitch" ) );
                imgui::widget::CircleSlider( "##Pitch", &mListenerAngles.y, -89.0f, 89.0f, "%.1f" );
                ImGui::EndTable();
            }
        }

        changed |= DrawAudioDistanceCurves( mSettings );
        ImGui::EndDisabled();
        ImGui::TextDisabled( "%s", MKT_LOC( "inspector_hrtf_steam_audio_best_heard_on_headphones" ).c_str() );
        ImGui::PopStyleVar();
        return changed;
    }

    auto AudioPanel::OnRender( float ) -> void {
        MKT_BEGIN_PROFILER();
        const bool requested{ mState->mRequestAudioPanel };
        mState->mRequestAudioPanel = false;

        if ( requested ) {
            mPanelIsVisible = true;
            const auto& path{ mState->mPreviewAudio.GetPath() };

            if ( path != mPath || !mSource ) {
                LoadClip( path );
            }
        }

        PollWaveform();

        if ( !mPanelIsVisible ) {
            if ( mSource && ( mSource->IsPlaying() || mSource->IsPaused() ) ) {
                mSource->Stop();
            }

            return;
        }

        const imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_WindowPadding, ImVec2{ 14.0f, 12.0f } };
        const imgui::ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 8.0f } };
        const imgui::ImGuiScopedStyleVar border{ ImGuiStyleVar_ChildBorderSize, 1.0f };
        const imgui::ImGuiScopedStyleVar rounding{ ImGuiStyleVar_ChildRounding, 4.0f };
        ImGui::SetNextWindowSize( { 760.0f, 720.0f }, ImGuiCond_FirstUseEver );

        if ( requested ) {
            ImGui::SetNextWindowFocus();
        }

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            if ( !mPanelIsVisible && mSource ) {
                mSource->Stop();
            }

            ImGui::End();
            return;
        }

        mPanelIsHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows );
        mPanelIsFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_ChildWindows );

        // Each bordered section includes padding and grows with its contents;
        // the main panel owns scrolling for the complete layout.
        constexpr ImGuiChildFlags sectionFlags{ ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY };
        constexpr ImGuiWindowFlags sectionWindowFlags{ ImGuiWindowFlags_NoScrollWithMouse };

        if ( ImGui::BeginChild( "Clip preview", {}, sectionFlags, sectionWindowFlags ) ) {
            DrawWaveform();

            if ( ImGui::Button( MKT_LOC( "audio_panel_open" ).c_str() ) ) {
                const auto path{ filesystem::OpenFileDialog( { { MKT_LOC( "inspector_audio_files" ).c_str(), "wav,mp3,flac" } } ) };

                if ( !path.IsEmpty() ) {
                    mState->mPreviewAudio = asset::AssetRef<asset::AudioClip>{ path };
                    mState->mRequestAudioPanel = true;
                }
            }

            ImGui::SameLine();
            ImGui::BeginDisabled( mPath.IsEmpty() );

            if ( ImGui::Button( MKT_LOC( "audio_panel_reload" ).c_str() ) ) {
                const auto path{ mPath };
                LoadClip( path );
            }

            ImGui::SameLine();

            if ( ImGui::Button( MKT_LOC( "inspector_clear_clip" ).c_str() ) ) {
                ClearClip();
                mState->mPreviewAudio = {};
            }

            ImGui::EndDisabled();

            if ( !mPath.IsEmpty() ) {
                eastl::string path{ mPath.GetC_Str() };
                ImGui::SetNextItemWidth( -FLT_MIN );
                ImGui::InputText( "##AudioPath", path.data(), path.size() + 1, ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll );

                if ( ImGui::IsItemHovered() ) {
                    imgui::widget::ItemTooltip( "%s", path.c_str() );
                }
            }

            if ( !mError.empty() ) {
                ImGui::TextWrapped( "%s", mError.c_str() );
            }
        }

        ImGui::EndChild();
        ImGui::Spacing();

        bool playbackChanged{};

        if ( ImGui::BeginChild( "Playback settings", {}, sectionFlags, sectionWindowFlags ) ) {
            playbackChanged = DrawPlayback();
        }

        ImGui::EndChild();
        ImGui::Spacing();

        bool spatialChanged{};

        if ( ImGui::BeginChild( "Spatial settings", {}, sectionFlags, sectionWindowFlags ) ) {
            spatialChanged = DrawSpatialSettings();
        }

        ImGui::EndChild();

        if ( ( playbackChanged || spatialChanged ) && mSource ) {
            mSource->SetSettings( mSettings );
            mSettings = mSource->GetSettings();
        }

        ApplyPreviewPose();

        if ( !mPanelIsVisible && mSource ) {
            mSource->Stop();
        }

        ImGui::End();
    }
}// namespace mikoto::editor
