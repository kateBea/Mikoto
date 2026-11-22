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

#include <imgui.h>

#include <EASTL/array.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtx/compatibility.hpp>

#include <implot.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <Core/LocalizationService.hh>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/StatsPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    static auto Metrics( const char* id ) -> bool {
        if ( !ImGui::BeginTable( id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings ) ) {
            return false;
        }

        ImGui::TableSetupColumn( "##Label", ImGuiTableColumnFlags_WidthStretch, 3.0f );
        ImGui::TableSetupColumn( "##Value", ImGuiTableColumnFlags_WidthStretch, 2.0f );
        return true;
    }

    static auto MetricValue( const char* value ) -> void {
        const float width{ ImGui::CalcTextSize( value ).x };
        const float available{ ImGui::GetContentRegionAvail().x };
        if ( width <= available ) {
            ImGui::SetCursorPosX( ImGui::GetCursorPosX() + available - width );
            ImGui::TextUnformatted( value );
        }
        else {
            ImGui::TextWrapped( "%s", value );
        }
    }

    static auto Row( const char* label, const char* value, const char* tooltip = nullptr ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextWrapped( "%s", label );
        ImGui::TableNextColumn();
        MetricValue( value );

        if ( tooltip ) {
            imgui::widget::ItemTooltip( "%s", tooltip );
        }
    }

    static auto StreamingMemoryRow( const char* label, const char* cpu, const char* gpu, const char* tooltip ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextWrapped( "%s", label );
        imgui::widget::ItemTooltip( "%s", tooltip );

        ImGui::TableNextColumn();
        MetricValue( cpu );
        ImGui::TableNextColumn();
        MetricValue( gpu );
    }

    MKT_NODISCARD static auto Memory( core::f64 bytes ) -> eastl::string {
        constexpr core::f64 bytesPerMiB{ 1024.0 * 1024.0 };
        constexpr core::f64 bytesPerGiB{ bytesPerMiB * 1024.0 };
        return bytes >= bytesPerGiB ? string::Format( "{:.2f} GiB", bytes / bytesPerGiB ) :
            string::Format( "{:.1f} MiB", bytes / bytesPerMiB );
    }

    StatsPanel::StatsPanel( const StatsPanelCreateInfo& info )
        : Panel{ "Stats", "Panel", info.mGroup, info.mIsToggleable },
          mState{ info.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_TABLE_CHART, mPanelName );
    }

    auto StatsPanel::Capture() -> void {
        MKT_BEGIN_PROFILER();
        mFrames = mHistory.Summarize();
        mSnapshot = {};

        if ( auto* assets{ asset::AssetRegistryService::GetPtr() }; assets && assets->IsInitialized() ) {
            const auto streaming{ assets->GetStreamingStatistics() };
            mSnapshot.streamingReservedCpu = double( streaming.mReserved.mCpuBytes );
            mSnapshot.streamingReservedGpu = double( streaming.mReserved.mGpuBytes );
            mSnapshot.streamingCpu = double( streaming.mResidentPayload.mCpuBytes );
            mSnapshot.streamingGpu = double( streaming.mResidentPayload.mGpuBytes );
            mSnapshot.streamingQueued = streaming.mQueued;
            mSnapshot.streamingLoading = streaming.mLoading;
            mSnapshot.streamingEvicted = streaming.mEvictedAssets;
            mSnapshot.streamingOverBudget = streaming.mOverBudget;
        }

        mSnapshot.cpu = SystemStats::Get()->GetCpuName();
        mSnapshot.cpuPercent = SystemStats::Get()->GetCpuUsage();
        mSnapshot.totalRam = SystemStats::Get()->GetTotalRam();
        mSnapshot.availableRam = SystemStats::Get()->GetFreeRam();
        mSnapshot.processRam = SystemStats::Get()->GetProcessRamUsage();
        mSnapshot.uptime = core::TimeService::Get()->GetTime();

        const auto* apiName{ GetGraphicsApiName( RenderSystem::Get()->GetActiveGraphicsApi() ) };
        mSnapshot.api = apiName ? apiName : MKT_LOC( "stats_unknown" ).c_str();

        if ( auto* device{ RenderSystem::Get()->GetGpuDevice() } ) {
            mSnapshot.gpu = device->GetDeviceName();
            mSnapshot.gpuHeapUsage = static_cast<double>( device->GetMemoryUsage() );

            const auto process{ device->GetProcessMemoryUsage() };
            mSnapshot.processGpuMemoryAvailable = process.mAvailable;
            mSnapshot.processVram = static_cast<double>( process.mLocalBytes );
            mSnapshot.processSharedGpuRam = static_cast<double>( process.mSharedBytes );
        }

        if ( mState && mState->mActiveScene ) {
            const auto& scene = *mState->mActiveScene;
            mSnapshot.scene = scene.GetName();
            mSnapshot.sceneState = scene.IsPlaying() ? MKT_LOC( "stats_playing" ).c_str() : scene.IsSimulating() ? MKT_LOC( "stats_simulating" ).c_str()
                                                                                                                 : MKT_LOC( "stats_editing" ).c_str();
            const auto& registry = scene.GetRegistry();
            mSnapshot.entities = scene.GetEntities().size();
            mSnapshot.meshes = registry.view<scene::MeshComponent>().size();
            mSnapshot.lights = registry.view<scene::LightComponent>().size();
            mSnapshot.texts = registry.view<scene::TextComponent>().size();
            mSnapshot.rigidBodies = registry.view<scene::RigidBodyComponent>().size();
            mSnapshot.colliders =
                registry.view<scene::BoxColliderComponent>().size() +
                registry.view<scene::SphereColliderComponent>().size() +
                registry.view<scene::CapsuleColliderComponent>().size() +
                registry.view<scene::MeshColliderComponent>().size();

            const auto sources = registry.view<scene::AudioSourceComponent>();
            mSnapshot.audioSources = sources.size();

            for ( const auto entity: sources ) {
                const auto source{ sources.get<scene::AudioSourceComponent>( entity ).GetSource() };

                if ( source && source->IsPlaying() ) {
                    ++mSnapshot.playingAudio;
                }
            }
        }

        if ( mState && mState->mEditorRenderer ) {
            auto& renderer = *mState->mEditorRenderer;
            const auto dimensions{ InferDimensions( renderer.GetRenderResolution() ) };
            const auto width{ dimensions.first };
            const auto height{ dimensions.second };
            mSnapshot.renderWidth = as<core::u32>( width );
            mSnapshot.renderHeight = as<core::u32>( height );
        }

        mCaptured = true;
    }

    auto StatsPanel::OnRender( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        if ( !mPaused && mHistory.Push( timeStep ) ) {
            mElapsed += timeStep;

            if ( !mCaptured || mElapsed >= mIntervalUpdate ) {
                Capture();
                mElapsed = glm::fmod( mElapsed, mIntervalUpdate );
            }
        }

        ImGui::SetNextWindowSize( { 570, 700 }, ImGuiCond_FirstUseEver );

        if ( imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            mPanelIsFocused = ImGui::IsWindowFocused();
            mPanelIsHovered = ImGui::IsWindowHovered();

            if ( ImGui::Button( mPaused ? MKT_LOC( "stats_resume_capture" ).c_str() : MKT_LOC( "stats_pause_capture" ).c_str() ) ) {
                mPaused = !mPaused;
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "stats_reset_history", "Reset history" ).c_str() ) ) {
                mHistory.Clear();
                mFrames = {};
                mElapsed = 0;
            }

            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "stats_copy_summary", "Copy summary" ).c_str() ) ) {
                const auto summary = string::Format( fmt::runtime( MKT_LOC( "stats_clipboard_summary" ).c_str() ),
                    mFrames.mFps, mFrames.mAverageMs, mFrames.mP95Ms, mFrames.mWorstMs, mSnapshot.cpuPercent, Memory( mSnapshot.processRam ),
                    mSnapshot.processGpuMemoryAvailable ? Memory( mSnapshot.processVram ) : eastl::string{ MKT_LOC( "stats_unavailable" ).c_str() }, mSnapshot.scene, mSnapshot.sceneState );

                ImGui::SetClipboardText( summary.c_str() );
            }

            ImGui::SetNextItemWidth( 160 );

            if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "stats_system_refresh", "System refresh" ).c_str(), &mIntervalUpdate, 0.1f, 2.0f, "%.1f s" ) ) {
                core::SystemStats::Get()->SetUpdateFrequency( mIntervalUpdate );
            }

            if ( mPaused ) {
                ImGui::TextDisabled( "%s", MKT_LOC( "stats_capture_paused_values_and_history_are_frozen" ).c_str() );
            }

            if ( !mCaptured ) {
                ImGui::TextDisabled( "%s", MKT_LOC( "stats_waiting_for_a_valid_frame_sample" ).c_str() );
            }

            DrawPerformance();
            DrawSystemInfo();
            DrawSceneInfo();
        }

        ImGui::End();
    }

    auto StatsPanel::DrawPerformance() -> void {
        MKT_BEGIN_PROFILER();

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "stats_frame_timing", "Frame timing" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        if ( mFrames.mCount ) {
            ImGui::Text( "%.1f FPS", mFrames.mFps );
            ImGui::SameLine();
            ImGui::TextDisabled( MKT_LOC( "stats_ms_average_samples" ).c_str(), mFrames.mAverageMs, mFrames.mCount );
        }

        if ( Metrics( "FrameMetrics" ) ) {
            Row( MKT_LOC( "stats_median" ).c_str(), string::Format( "{:.2f} ms", mFrames.mMedianMs ).c_str() );
            Row( MKT_LOC( "stats_95th_percentile" ).c_str(), string::Format( "{:.2f} ms", mFrames.mP95Ms ).c_str() );
            Row( MKT_LOC( "stats_worst_frame" ).c_str(), string::Format( "{:.2f} ms", mFrames.mWorstMs ).c_str() );
            ImGui::EndTable();
        }

        constexpr eastl::array rates{ 30, 60, 120, 144, 165, 180, 210, 240, 260 };
        constexpr core::f32 millisecondsPerSecond{ 1000.0f };
        constexpr core::f32 budgetPickerWidth{ 140.0f };
        const auto currentBudget{ string::Format( "{} FPS", rates[mTargetIndex] ) };

        ImGui::SetNextItemWidth( budgetPickerWidth );
        if ( ImGui::BeginCombo( imgui::widget::LocalizedLabel( "stats_frame_budget", "Frame budget" ).c_str(), currentBudget.c_str() ) ) {
            for ( core::i32 index{}; index < core::as<core::i32>( rates.size() ); ++index ) {
                const auto label{ string::Format( "{} FPS", rates[index] ) };
                const bool selected{ mTargetIndex == index };

                if ( ImGui::Selectable( label.c_str(), selected ) ) {
                    mTargetIndex = index;
                    core::TimeService::Get()->SetFrameBudget( 1000.0 / rates[index] );
                }

                if ( selected ) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        const core::f32 budget{ millisecondsPerSecond / core::as<core::f32>( rates[mTargetIndex] ) };
        float plotMaximum{ budget * 2 };

        for ( usize i{}; i < mHistory.Count(); ++i ) {
            plotMaximum = eastl::max( plotMaximum, mHistory.At( i ) );
        }

        if ( ImPlot::BeginPlot( "##FrameHistory", { -1, 180 }, ImPlotFlags_Crosshairs ) ) {
            ImPlot::SetupAxes( MKT_LOC( "stats_frame_oldest_to_newest" ).c_str(), MKT_LOC( "stats_time_ms" ).c_str() );
            const auto limits{ mPaused ? ImPlotCond_Once : ImPlotCond_Always };
            ImPlot::SetupAxesLimits( 0, eastl::max( 1.0, double( mHistory.Count() ) - 1.0 ),
                0, plotMaximum * 1.1, limits );
            ImPlot::SetupAxisFormat( ImAxis_Y1, "%.1f" );
            ImPlotSpec line{};
            line.LineWeight = 1.5f;
            line.FillAlpha = 0.15f;
            line.Flags = ImPlotLineFlags_Shaded;
            ImPlot::PlotLineG( MKT_LOC( "stats_frame_time" ).c_str(), []( int index, void* data ) {
                return ImPlotPoint{ double( index ), static_cast<FrameHistory*>( data )->At( size_t( index ) ) };
            }, &mHistory, static_cast<int>( mHistory.Count() ), line );
            ImPlotSpec reference{};
            reference.LineColor = ImGui::GetStyleColorVec4( ImGuiCol_PlotHistogram );
            reference.Flags = ImPlotInfLinesFlags_Horizontal;
            const auto label{ string::Format( "Budget ({:.2f} ms)", budget ) };
            ImPlot::PlotInfLines( label.c_str(), &budget, 1, reference );
            ImPlot::EndPlot();
        }

        ImGui::TextDisabled( "%s", MKT_LOC( "stats_pause_capture_to_zoom_and_pan_the_history" ).c_str() );
        ImGui::TextDisabled( MKT_LOC( "stats_rolling_window_up_to_frames_fps_frames_elapsed_time" ).c_str(), FrameHistory::kCapacity );
    }

    auto StatsPanel::DrawSystemInfo() -> void {
        MKT_BEGIN_PROFILER();

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "stats_system_and_memory", "System and memory" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        const auto unavailable{ MKT_LOC( "stats_unavailable" ) };
        const auto cpuLabel{ MKT_LOC( "stats_cpu" ) };
        const auto gpuLabel{ MKT_LOC( "stats_gpu" ) };

        if ( ImGui::BeginTable( "SystemHardware", 2, ImGuiTableFlags_NoSavedSettings ) ) {
            const float labelWidth{ eastl::max( ImGui::CalcTextSize( cpuLabel.c_str() ).x, ImGui::CalcTextSize( gpuLabel.c_str() ).x ) };
            ImGui::TableSetupColumn( "##Hardware", ImGuiTableColumnFlags_WidthFixed, labelWidth );
            ImGui::TableSetupColumn( "##Model", ImGuiTableColumnFlags_WidthStretch );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled( "%s", cpuLabel.c_str() );
            ImGui::TableNextColumn();
            ImGui::TextWrapped( "%s", mSnapshot.cpu.empty() ? unavailable.c_str() : mSnapshot.cpu.c_str() );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled( "%s", gpuLabel.c_str() );
            ImGui::TableNextColumn();
            ImGui::TextWrapped( "%s", mSnapshot.gpu.empty() ? unavailable.c_str() : mSnapshot.gpu.c_str() );
            ImGui::EndTable();
        }

        if ( Metrics( "SystemActivity" ) ) {
            Row( MKT_LOC( "stats_cpu_usage_whole_system" ).c_str(), string::Format( "{:.1f}%", mSnapshot.cpuPercent ).c_str() );
            Row( MKT_LOC( "stats_graphics_api" ).c_str(), mSnapshot.api.c_str() );
            Row( MKT_LOC( "stats_uptime" ).c_str(), core::TimeService::Get()->ToString( mSnapshot.uptime ).c_str() );
            ImGui::EndTable();
        }

        ImGui::SeparatorText( MKT_LOC( "stats_memory_heading" ).c_str() );
        if ( mSnapshot.totalRam > 0 ) {
            const float used{ core::as<core::f32>( eastl::clamp( 1.0 - mSnapshot.availableRam / mSnapshot.totalRam, 0.0, 1.0 ) ) };
            const auto percentage{ string::Format( "{:.1f}%", used * 100.0f ) };
            ImGui::TextUnformatted( MKT_LOC( "stats_system_ram_usage" ).c_str() );
            ImGui::ProgressBar( used, { -1.0f, ImGui::GetTextLineHeightWithSpacing() }, percentage.c_str() );
        }

        if ( Metrics( "SystemMemory" ) ) {
            Row( MKT_LOC( "stats_process_ram_resident" ).c_str(), Memory( mSnapshot.processRam ).c_str() );
            Row( MKT_LOC( "stats_available_system_ram" ).c_str(), mSnapshot.totalRam > 0 ? Memory( mSnapshot.availableRam ).c_str() : unavailable.c_str() );
            Row( MKT_LOC( "stats_total_system_ram" ).c_str(), mSnapshot.totalRam > 0 ? Memory( mSnapshot.totalRam ).c_str() : unavailable.c_str() );
            Row( MKT_LOC( "stats_process_vram_local" ).c_str(), mSnapshot.processGpuMemoryAvailable ? Memory( mSnapshot.processVram ).c_str() : unavailable.c_str() );
            Row( MKT_LOC( "stats_process_gpu_shared_ram" ).c_str(), mSnapshot.processGpuMemoryAvailable ? Memory( mSnapshot.processSharedGpuRam ).c_str() : unavailable.c_str() );
            Row( MKT_LOC( "stats_gpu_usage_backend_estimate" ).c_str(), mSnapshot.gpu.empty() ? unavailable.c_str() : Memory( mSnapshot.gpuHeapUsage ).c_str(),
                MKT_LOC( "stats_backend_dependent_heap_accounting_this_is_not_the_process_vram" ).c_str() );
            ImGui::EndTable();
        }

        ImGui::SeparatorText( MKT_LOC( "stats_asset_streaming_heading" ).c_str() );
        if ( Metrics( "StreamingActivity" ) ) {
            Row( MKT_LOC( "stats_streaming_jobs_queued_loading" ).c_str(), string::Format( "{} / {}", mSnapshot.streamingQueued, mSnapshot.streamingLoading ).c_str() );
            Row( MKT_LOC( "stats_streaming_payload_budget" ).c_str(),
                MKT_LOC( mSnapshot.streamingOverBudget ? "stats_streaming_over_budget" : "stats_within_budget" ).c_str(),
                mSnapshot.streamingOverBudget ? MKT_LOC( "stats_exceeded_referenced_assets_remain_resident" ).c_str() : nullptr );
            Row( MKT_LOC( "stats_assets_evicted_from_cache" ).c_str(), string::Format( "{}", mSnapshot.streamingEvicted ).c_str() );
            ImGui::EndTable();
        }

        if ( ImGui::BeginTable( "StreamingMemory", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings ) ) {
            ImGui::TableSetupColumn( "##Label", ImGuiTableColumnFlags_WidthStretch, 2.0f );
            ImGui::TableSetupColumn( "##Cpu", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "##Gpu", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableNextRow( ImGuiTableRowFlags_Headers );
            ImGui::TableSetColumnIndex( 1 );
            MetricValue( cpuLabel.c_str() );
            ImGui::TableNextColumn();
            MetricValue( gpuLabel.c_str() );

            StreamingMemoryRow( MKT_LOC( "stats_streaming_resident_payload" ).c_str(), Memory( mSnapshot.streamingCpu ).c_str(),
                Memory( mSnapshot.streamingGpu ).c_str(), MKT_LOC( "stats_logical_cached_model_and_texture_payloads_not_process_residency_excludes" ).c_str() );
            StreamingMemoryRow( MKT_LOC( "stats_streaming_import_reservations" ).c_str(), Memory( mSnapshot.streamingReservedCpu ).c_str(),
                Memory( mSnapshot.streamingReservedGpu ).c_str(), MKT_LOC( "stats_estimated_admission_reservations_not_measured_staging_or_driver_allocation_an" ).c_str() );
            ImGui::EndTable();
        }
    }

    auto StatsPanel::DrawSceneInfo() -> void {
        MKT_BEGIN_PROFILER();

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "stats_scene", "Scene" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        if ( mSnapshot.scene.empty() ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "stats_no_active_scene" ).c_str() );
            return;
        }

        ImGui::Text( "%s | %s", mSnapshot.scene.c_str(), mSnapshot.sceneState.c_str() );

        if ( Metrics( "SceneMetrics" ) ) {
            Row( MKT_LOC( "stats_render_resolution" ).c_str(), string::Format( "{} x {}", mSnapshot.renderWidth, mSnapshot.renderHeight ).c_str() );
            Row( MKT_LOC( "stats_entities_meshes" ).c_str(), string::Format( "{} / {}", mSnapshot.entities, mSnapshot.meshes ).c_str() );
            Row( MKT_LOC( "stats_lights_text_components" ).c_str(), string::Format( "{} / {}", mSnapshot.lights, mSnapshot.texts ).c_str() );
            Row( MKT_LOC( "stats_rigid_bodies_colliders" ).c_str(), string::Format( "{} / {}", mSnapshot.rigidBodies, mSnapshot.colliders ).c_str() );
            Row( MKT_LOC( "stats_audio_sources_playing" ).c_str(), string::Format( "{} / {}", mSnapshot.audioSources, mSnapshot.playingAudio ).c_str() );
            ImGui::EndTable();
        }

        ImGui::TextDisabled( "%s", MKT_LOC( "stats_component_counts_include_inactive_entities" ).c_str() );
    }
}// namespace mikoto::editor
