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

#include <EASTL/array.h>

#include <implot.h>

#include <Core/LocalizationService.hh>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/GpuQueryProfiler.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/GPUStatsPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace renderer;
    using namespace renderer::rhi;

    GPUStatsPanel::GPUStatsPanel( const GPUStatsPanelCreateInfo& info )
        : Panel{ "GPU Stats", "Panel", info.mGroup, info.mIsToggleable },
          mState{ info.mState },
          mGpuDevice{ info.mGpuDevice } {
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_TABLE_CHART, mPanelName );
        mPanelIsVisible = false;
    }

    auto GPUStatsPanel::OnRender( core::f32 ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 650, 680 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        mPanelIsFocused = ImGui::IsWindowFocused();
        mPanelIsHovered = ImGui::IsWindowHovered();

        if ( !mGpuDevice ) {
            ImGui::TextDisabled( MKT_LOC( "gpu_stats_no_gpu_device" ).c_str() );
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted( mGpuDevice->GetDeviceName().data() );
        eastl::array<const char*, 2> views{ MKT_LOC( "gpu_stats_scene" ).c_str(), MKT_LOC( "gpu_stats_game" ).c_str() };
        core::i32 selectedView{ mInspectGameRenderer ? 1 : 0 };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "gpu_stats_view", "View" ).c_str(), &selectedView, views.data(), ( int )views.size() ) ) {
            mInspectGameRenderer = selectedView == 1;
        }

        if ( mState && ImGui::Button( imgui::widget::LocalizedLabel( "gpu_stats_capture_both_views", "Capture both views" ).c_str() ) ) {
            for ( auto* renderer: { mState->mEditorRenderer, mState->mGameRenderer } ) {
                if ( renderer ) {
                    if ( auto* capture{ renderer->GetGpuProfiler() } ) {
                        capture->SetEnabled( true );
                    }
                }
            }
        }

        auto* sceneRenderer{ mState ? ( mInspectGameRenderer ? mState->mGameRenderer : mState->mEditorRenderer ) : nullptr };
        auto* profiler{ sceneRenderer ? sceneRenderer->GetGpuProfiler() : nullptr };
        ImGui::TextWrapped( MKT_LOC( "gpu_stats_selected_view_s_graphics_submission_only_results_arrive_after_gpu" ).c_str() );

        if ( sceneRenderer ) {
            if ( const auto output{ sceneRenderer->GetFinalImage() } ) {
                ImGui::Text( MKT_LOC( "gpu_stats_render_target_dimensions_x" ).c_str(), output->GetWidth(), output->GetHeight() );
            }
        }

        if ( profiler ) {
            bool enabled{ profiler->IsEnabled() };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "gpu_stats_capture_gpu_queries", "Capture GPU queries" ).c_str(), enabled ) ) {
                profiler->SetEnabled( enabled );
            }

            if ( !profiler->GetError().empty() ) {
                ImGui::TextWrapped( "%s", profiler->GetError().data() );
            }

            const auto& snapshot{ profiler->GetSnapshot() };

            if ( mLastRenderer != sceneRenderer || snapshot.mSubmission < mLastSubmission ) {
                mHistoryCount = mHistoryHead = 0;
                mLastSubmission = 0;
                mLastRenderer = sceneRenderer;
            }

            if ( snapshot.mSubmission != mLastSubmission ) {
                mLastSubmission = snapshot.mSubmission;

                if ( snapshot.mHasTiming ) {
                    mHistory[mHistoryHead] = core::as<core::f32>( snapshot.mFrameMilliseconds );
                    mHistoryHead = ( mHistoryHead + 1 ) % core::as<core::u32>( mHistory.size() );
                    mHistoryCount = eastl::min( mHistoryCount + 1, core::as<core::u32>( mHistory.size() ) );
                }
            }

            if ( !snapshot.mSubmission ) {
                ImGui::TextDisabled( MKT_LOC( "gpu_stats_enable_capture_and_render_a_scene_to_collect_a_sample" ).c_str() );
            }
            else {
                ImGui::Text( MKT_LOC( "gpu_stats_completed_submission_dropped_samples" ).c_str(),
                    core::as<core::u64>( snapshot.mSubmission ),
                    core::as<core::u64>( profiler->GetDroppedSamples() ) );

                if ( snapshot.mHasTiming ) {
                    ImGui::Text( MKT_LOC( "gpu_stats_gpu_time_ms" ).c_str(), views[selectedView], snapshot.mFrameMilliseconds );
                    float maximum{ 1.0f };

                    for ( core::u32 index{}; index < mHistoryCount; ++index ) {
                        maximum = eastl::max( maximum, mHistory[index] );
                    }

                    ImGui::PushID( selectedView );

                    if ( ImPlot::BeginPlot( "##GpuTime", { -1, 180 }, ImPlotFlags_Crosshairs ) ) {
                        ImPlot::SetupAxes( MKT_LOC( "gpu_stats_sample_oldest_to_newest" ).c_str(), MKT_LOC( "gpu_stats_gpu_time_ms" ).c_str() );
                        ImPlot::SetupAxesLimits( 0, eastl::max( 1.0, double( mHistoryCount ) - 1.0 ),
                            0, maximum * 1.1, enabled ? ImPlotCond_Always : ImPlotCond_Once );
                        ImPlot::SetupAxisFormat( ImAxis_Y1, "%.2f" );
                        ImPlotSpec line{};
                        line.LineWeight = 1.5f;
                        line.FillAlpha = 0.15f;
                        line.Flags = ImPlotLineFlags_Shaded;
                        ImPlot::PlotLineG( MKT_LOC( "gpu_stats_gpu_time" ).c_str(), []( int index, void* data ) {
                            const auto& panel{ *static_cast<GPUStatsPanel*>( data ) };
                            const auto offset{ panel.mHistoryCount == panel.mHistory.size() ? panel.mHistoryHead : 0 };
                            const auto sample{ ( offset + core::as<core::u32>( index ) ) % panel.mHistory.size() };
                            return ImPlotPoint{ double( index ), panel.mHistory[sample] };
                        }, this, core::as<core::i32>( mHistoryCount ), line );
                        ImPlot::EndPlot();
                    }

                    ImGui::PopID();
                    ImGui::TextDisabled( MKT_LOC( "gpu_stats_disable_capture_to_zoom_and_pan_the_history" ).c_str() );
                }

                if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "gpu_stats_pipeline_statistics", "Pipeline statistics" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) &&
                     ImGui::BeginTable( "GpuCounters", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH ) ) {
                    for ( core::u32 counter{}; counter < core::as<core::u32>( renderer::rhi::PipelineStatistic::eCount ); ++counter ) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted( renderer::rhi::GetPipelineStatisticName( static_cast<renderer::rhi::PipelineStatistic>( counter ) ) );
                        ImGui::TableNextColumn();

                        if ( snapshot.mStatistics.mValidMask & ( core::u64{ 1 } << counter ) ) {
                            ImGui::Text( "%llu", static_cast<unsigned long long>( snapshot.mStatistics.mValues[counter] ) );
                        }
                        else {
                            ImGui::TextDisabled( MKT_LOC( "gpu_stats_unavailable" ).c_str() );
                        }
                    }

                    ImGui::EndTable();
                }

                if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "gpu_stats_pass_gpu_timings", "Pass GPU timings" ).c_str() ) &&
                     ImGui::BeginTable( "GpuPasses", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH ) ) {
                    for ( const auto& pass: snapshot.mPasses ) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted( pass.mName.c_str() );
                        ImGui::TableNextColumn();
                        ImGui::Text( "%.3f ms", pass.mMilliseconds );
                    }

                    ImGui::EndTable();
                }
            }
        }
        else {
            ImGui::TextDisabled( MKT_LOC( "gpu_stats_no_frame_graph_gpu_capture_is_unavailable" ).c_str() );
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "gpu_stats_query_capabilities", "Query capabilities" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            ImGui::TextWrapped( MKT_LOC( "gpu_stats_supported_means_exposed_by_the_active_rhi_backend_on_the" ).c_str() );

            for ( core::u32 index{}; index < core::as<core::u32>( renderer::rhi::QueryType::eCount ); ++index ) {
                const auto type{ core::as<renderer::rhi::QueryType>( index ) };
                const auto capability{ mGpuDevice->GetQueryCapabilities( type, QueueType::eGraphics ) };
                ImGui::BulletText( "%s: %s", GetQueryTypeName( type ), capability.mSupported ? MKT_LOC( "gpu_stats_supported" ).c_str() : MKT_LOC( "gpu_stats_unavailable" ).c_str() );

                if ( ImGui::IsItemHovered() && !capability.mSupported ) {
                    mikoto::imgui::widget::ItemTooltip( "%s", capability.mReason );
                }

                if ( capability.mSupported && type == renderer::rhi::QueryType::eTimestamp ) {
                    ImGui::TextDisabled( MKT_LOC( "gpu_stats_counter_period_ns_valid_bits" ).c_str(), capability.mTimestampPeriodNs, capability.mTimestampValidBits );
                }

                if ( capability.mSupported && type == renderer::rhi::QueryType::eOcclusion ) {
                    ImGui::TextDisabled( MKT_LOC( "gpu_stats_precise_sample_counts" ).c_str(), capability.mPreciseOcclusion ? MKT_LOC( "gpu_stats_available" ).c_str() : MKT_LOC( "gpu_stats_unavailable" ).c_str() );
                }
            }

            ImGui::TextWrapped( MKT_LOC( "gpu_stats_elapsed_milliseconds_are_calculated_from_timestamp_pairs_this_is_separate" ).c_str() );
        }

        ImGui::End();
    }
}// namespace mikoto::editor
