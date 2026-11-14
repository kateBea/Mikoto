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

#include <Core/LocalizationService.hh>

#include <Audio/AudioSource.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <ImGui/Utility.hh>

#include <Panels/AudioProfilerPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    AudioProfilerPanel::AudioProfilerPanel( const AudioProfilerPanelCreateInfo& info )
        : Panel{ "Audio Profiler", "Audio Profiler", info.mGroup, info.mIsToggleable }, mState{ info.mState } {
        mPanelIsVisible = false;
    }

    auto AudioProfilerPanel::OnRender( core::f32 ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 560, 400 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        if ( !mState->mActiveScene ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "audio_profiler_open_a_scene_to_inspect_its_audio_voices" ).c_str() );
            ImGui::End();
            return;
        }

        core::u32 sources{};
        core::u32 playing{};
        core::u32 paused{};
        core::u32 spatial{};
        constexpr ImGuiTableFlags flags{ ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "Audio voices", 5, flags ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "audio_profiler_source" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( MKT_LOC( "audio_profiler_state" ).c_str() );
            ImGui::TableSetupColumn( MKT_LOC( "audio_profiler_position_duration" ).c_str() );
            ImGui::TableSetupColumn( MKT_LOC( "audio_profiler_gain" ).c_str() );
            ImGui::TableSetupColumn( MKT_LOC( "audio_profiler_output" ).c_str() );
            ImGui::TableHeadersRow();

            for ( auto [entity, component, tag]: mState->mActiveScene->GetRegistry().view<scene::AudioSourceComponent, scene::TagComponent>().each() ) {
                const auto source{ component.GetSource() };

                if ( !source || !source->IsReady() ) {
                    continue;
                }

                ++sources;
                const bool isPlaying{ source->IsPlaying() };
                const bool isPaused{ source->IsPaused() };
                const bool isSpatial{ source->IsSpatialized() };
                playing += isPlaying;
                paused += isPaused;
                spatial += isSpatial;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( tag.GetTag().c_str() );
                ImGui::TableSetColumnIndex( 1 );
                ImGui::TextUnformatted( isPlaying ? MKT_LOC( "audio_profiler_playing" ).c_str() : isPaused ? MKT_LOC( "audio_profiler_paused" ).c_str()
                                                                                                           : MKT_LOC( "audio_profiler_stopped" ).c_str() );
                ImGui::TableSetColumnIndex( 2 );
                ImGui::Text( "%.2f / %.2f s", source->GetCurrentProgress(), source->GetAudioDuration() );
                ImGui::TableSetColumnIndex( 3 );

                if ( source->IsMuted() ) {
                    ImGui::TextDisabled( "%s", MKT_LOC( "audio_profiler_muted" ).c_str() );
                }
                else {
                    ImGui::Text( "%.2fx", source->GetVolume() );
                }

                ImGui::TableSetColumnIndex( 4 );
                ImGui::TextUnformatted( isSpatial ? MKT_LOC( "audio_profiler_3d_hrtf" ).c_str() : MKT_LOC( "audio_profiler_2d_stereo" ).c_str() );
            }

            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text( MKT_LOC( "audio_profiler_loaded_playing_paused_spatial" ).c_str(), sources, playing, paused, spatial );
        // These are voice properties, not measured sample peaks or DSP timings.
        // Keep the UI explicit until the backend publishes real meters.
        ImGui::TextWrapped( "%s", MKT_LOC( "audio_profiler_active_scene_voice_monitor_gain_is_a_source_setting_not" ).c_str() );
        ImGui::End();
    }
}// namespace mikoto::editor
