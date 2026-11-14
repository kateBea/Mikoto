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

#include <Audio/AudioSource.hh>
#include <Audio/AudioService.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Panels/AudioMixerPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    AudioMixerPanel::AudioMixerPanel( const AudioMixerPanelCreateInfo& info )
        : Panel{ "Audio Mixer", "Audio Mixer", info.mGroup, info.mIsToggleable }, mState{ info.mState } {
        mPanelIsVisible = false;
    }

    auto AudioMixerPanel::OnRender( core::f32 ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 560, 400 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        auto* service{ audio::AudioService::GetPtr() };
        auto* device{ service ? service->GetDevice() : nullptr };

        if ( !device || !device->IsInitialized() ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "audio_mixer_audio_device_unavailable" ).c_str() );
            ImGui::End();
            return;
        }

        ImGui::SeparatorText( MKT_LOC( "audio_mixer_master_output" ).c_str() );
        float volume{ device->GetMasterVolume() };

        if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "audio_mixer_master_gain", "Master gain" ).c_str(), &volume, 0, 2, "%.2fx" ) ) {
            device->SetMasterVolume( volume );
        }

        imgui::widget::ItemTooltip( MKT_LOC( "audio_mixer_applies_to_every_voice_this_session_control_is_not_saved" ).c_str() );
        ImGui::TextDisabled( "%s", MKT_LOC( "audio_mixer_stereo_output_3d_sources_use_steam_audio_hrtf" ).c_str() );
        ImGui::SeparatorText( MKT_LOC( "audio_mixer_scene_sources" ).c_str() );

        if ( !mState->mActiveScene ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "audio_mixer_open_a_scene_to_mix_its_audio_sources" ).c_str() );
            ImGui::End();
            return;
        }

        core::u32 count{};
        constexpr ImGuiTableFlags flags{ ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "Voices", 5, flags ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "audio_mixer_source" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( MKT_LOC( "audio_mixer_state" ).c_str(), ImGuiTableColumnFlags_WidthFixed, 65 );
            ImGui::TableSetupColumn( MKT_LOC( "audio_mixer_spatial" ).c_str(), ImGuiTableColumnFlags_WidthFixed, 60 );
            ImGui::TableSetupColumn( MKT_LOC( "audio_mixer_level" ).c_str(), ImGuiTableColumnFlags_WidthFixed, 180 );
            ImGui::TableSetupColumn( MKT_LOC( "audio_mixer_playback" ).c_str(), ImGuiTableColumnFlags_WidthFixed, 160 );
            ImGui::TableHeadersRow();

            for ( auto [entity, component, tag]: mState->mActiveScene->GetRegistry().view<scene::AudioSourceComponent, scene::TagComponent>().each() ) {
                auto source{ component.GetSource() };
                ++count;
                // Registry handles include the generation; truncated asset GUIDs
                // could alias controls belonging to different voices.
                ImGui::PushID( static_cast<int>( entity ) );
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( tag.GetTag().c_str() );
                ImGui::TableSetColumnIndex( 1 );

                if ( !source || !source->IsReady() ) {
                    ImGui::TextDisabled( "%s", MKT_LOC( "audio_mixer_no_clip" ).c_str() );
                    ImGui::PopID();
                    continue;
                }

                ImGui::TextUnformatted( source->IsPlaying() ? MKT_LOC( "audio_mixer_playing" ).c_str() : source->IsPaused() ? MKT_LOC( "audio_mixer_paused" ).c_str()
                                                                                                                            : MKT_LOC( "audio_mixer_stopped" ).c_str() );
                ImGui::TableSetColumnIndex( 2 );
                auto settings{ source->GetSettings() };
                bool changed{ imgui::CheckBox( "3D", settings.mSpatialized ) };
                imgui::widget::ItemTooltip( MKT_LOC( "audio_mixer_use_headphone_hrtf_positioning_off_preserves_stereo_for_music_and" ).c_str() );
                ImGui::TableSetColumnIndex( 3 );
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "audio_mixer_mute", "Mute" ).c_str(), settings.mMuted );

                ImGui::SameLine();

                ImGui::SetNextItemWidth( -1 );
                changed |= ImGui::SliderFloat( "##Gain", &settings.mVolume, 0, audio::AudioSource::GetMaxVolume(), "%.2fx" );
                imgui::widget::ItemTooltip( MKT_LOC( "audio_mixer_source_gain_1_0_is_the_original_level_high_gains" ).c_str() );

                if ( changed ) {
                    source->SetSettings( settings );
                }

                ImGui::TableSetColumnIndex( 4 );
                ImGui::BeginDisabled( source->IsPlaying() );

                if ( ImGui::SmallButton( source->IsPaused() ? MKT_LOC( "audio_mixer_resume" ).c_str() : MKT_LOC( "audio_mixer_play" ).c_str() ) ) {
                    source->Play();
                }

                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled( !source->IsPlaying() );

                if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "audio_mixer_pause", "Pause" ).c_str() ) ) {
                    source->Pause();
                }

                ImGui::EndDisabled();
                ImGui::SameLine();

                if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "audio_mixer_stop", "Stop" ).c_str() ) ) {
                    source->Stop();
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if ( count == 0 ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "audio_mixer_add_an_audio_component_to_an_entity_to_create_a" ).c_str() );
        }

        ImGui::TextDisabled( "%s", MKT_LOC( "audio_mixer_changes_during_play_affect_the_runtime_scene_copy" ).c_str() );
        ImGui::End();
    }
}// namespace mikoto::editor
