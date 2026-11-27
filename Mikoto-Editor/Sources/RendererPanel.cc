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

#include <Application/PropertyWidgets.hh>

#include <Core/Core.hh>
#include <Core/LocalizationService.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/TimeService.hh>
#include <Core/Types.hh>

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/GraphEditor.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/Utility.hh>

#include <Layers/EditorLayer.hh>

#include <Memory/Allocator.hh>

#include <Panels/RendererPanel.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/ForwardRenderer/Passes/OutlineModule.hh>
#include <Renderer/Particle/ParticleSystem.hh>
#include <Renderer/RayTracing/RayTracing.hh>
#include <Renderer/Rhi/Types.hh>

#include <Text/Utility.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto BeginRendererRows( const char *id, ImGuiTableFlags flags = ImGuiTableFlags_None ) -> bool {
        if ( !ImGui::BeginTable( id, 2, flags | ImGuiTableFlags_NoSavedSettings ) ) {
            return false;
        }

        ImGui::TableSetupColumn( "##Label", ImGuiTableColumnFlags_WidthStretch, 3.0f );
        ImGui::TableSetupColumn( "##Value", ImGuiTableColumnFlags_WidthStretch, 2.0f );
        return true;
    }

    static auto RendererControlRow( const char *label ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextWrapped( "%s", label );

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth( -1.0f );
    }

    static auto RendererMetricValue( const char *value ) -> void {
        const core::f32 width{ ImGui::CalcTextSize( value ).x };
        const core::f32 available{ ImGui::GetContentRegionAvail().x };

        if ( width <= available ) {
            ImGui::SetCursorPosX( ImGui::GetCursorPosX() + available - width );
            ImGui::TextUnformatted( value );
        }
        else {
            ImGui::TextWrapped( "%s", value );
        }
    }

    static auto RendererMetricRow( const char *label, const char *value ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextWrapped( "%s", label );

        ImGui::TableNextColumn();
        RendererMetricValue( value );
    }

    static auto DrawShadowDiagnostics( const renderer::ShadowDiagnostics &shadows ) -> void {
        ImGui::SeparatorText( MKT_LOC( "renderer_shadow_coverage" ).c_str() );
        if ( ImGui::BeginTable( "ShadowCoverage", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "lighting_lights" ).c_str(), ImGuiTableColumnFlags_WidthStretch, 2.0f );
            ImGui::TableSetupColumn( MKT_LOC( "renderer_active_lights" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( MKT_LOC( "renderer_mapped_lights" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableHeadersRow();

            const eastl::array types{
                MKT_LOC( "renderer_directional" ).c_str(),
                MKT_LOC( "renderer_point" ).c_str(),
                MKT_LOC( "renderer_spot" ).c_str()
            };

            for ( core::usize type{}; type < types.size(); ++type ) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextWrapped( "%s", types[type] );

                ImGui::TableNextColumn();
                RendererMetricValue( string::Format( "{}", shadows.mActiveLights[type] ).c_str() );
                ImGui::TableNextColumn();
                RendererMetricValue( string::Format( "{}", shadows.mMappedLights[type] ).c_str() );
            }

            ImGui::EndTable();
        }

        ImGui::SeparatorText( MKT_LOC( "renderer_shadow_submissions" ).c_str() );
        imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_shadow_maps_require_cast_shadows_on_the_light_up_to" ).c_str() );

        if ( BeginRendererRows( "ShadowSubmissions", ImGuiTableFlags_RowBg ) ) {
            RendererMetricRow( MKT_LOC( "renderer_shadow_depth_maps_label" ).c_str(),
                string::Format( "{}", shadows.mSubmittedMaps ).c_str() );
            RendererMetricRow( MKT_LOC( "renderer_shadow_batches_per_map_label" ).c_str(),
                string::Format( "{}", shadows.mCasterBatches ).c_str() );
            RendererMetricRow( MKT_LOC( "renderer_shadow_batches_label" ).c_str(),
                string::Format( "{}", shadows.mSubmittedBatches ).c_str() );
            RendererMetricRow( MKT_LOC( "renderer_shadow_culled_pairs_label" ).c_str(),
                string::Format( "{} / {}", shadows.mCulledInstances, shadows.mTestedInstances ).c_str() );

            if ( shadows.mUnculledViews != 0 ) {
                RendererMetricRow( MKT_LOC( "renderer_shadow_fallback_views_label" ).c_str(),
                    string::Format( "{}", shadows.mUnculledViews ).c_str() );
            }

            ImGui::EndTable();
        }
    }

    RendererPanel::RendererPanel( const RendererPanelCreateInfo &createInfo )
        : Panel{ "Renderer", "Panel", createInfo.mGroup, createInfo.mIsToggleable },
          mEditorState{ createInfo.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_POWER_SETTINGS_NEW, mPanelName );

        if ( !mEditorState->mEditorRenderer || !mEditorState->mEditorRenderer->HasFrameGraph() ) {
            return;
        }

        // Construct the node graph for pass dependencies
        imgui::GraphEditorBuilder builder{};
        auto &nodeControl{ mEditorState->mEditorRenderer->GetNodeControl() };

        for ( const auto &entry: nodeControl.mNodes ) {
            const auto &passName{ entry.first };
            const auto &pass{ entry.second };

            builder.PushNode( passName );
        }

        mGraphEditor.Build( builder );
    }

    auto RendererPanel::OnRender( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( ImVec2{ 420.0f, 640.0f }, ImGuiCond_FirstUseEver );
        if ( !imgui::BeginWindow( GetHeaderName().data(), MKT_ADDRESSOF( mPanelIsVisible ), ImGuiWindowFlags_NoCollapse ) ) {
            ImGui::End();
            return;
        }

        if ( !mEditorState->mEditorRenderer || !mEditorState->mEditorRenderer->IsRenderAvailable() ) {
            ImGui::TextWrapped( MKT_LOC( "renderer_scene_rendering_is_unavailable_with_the_selected_graphics_backend_you" ).c_str() );
            ImGui::End();
            return;
        }

        auto &sceneRenderer{ *mEditorState->mEditorRenderer };
        if ( sceneRenderer.IsPreparing() ) {
            ImGui::TextWrapped( "Preparing renderer pipelines..." );
        }

        if ( !sceneRenderer.HasFrameGraph() ) {
            auto changed{ false };
            auto settings{ sceneRenderer.GetSettings() };

            if ( ImGui::CollapsingHeader( MKT_LOC( "renderer_shadows" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                if ( BeginRendererRows( "CompatibilityShadows" ) ) {
                    int quality{ core::as<int>( settings.mShadowQuality ) };
                    RendererControlRow( MKT_LOC( "renderer_quality" ).c_str() );
                    if ( ImGui::Combo( "##ShadowQuality", &quality, MKT_LOC( "renderer_low_medium_high" ).c_str() ) ) {
                        settings.mShadowQuality = static_cast<renderer::ShadowMapQuality>( quality );
                        changed = true;
                    }

                    RendererControlRow( MKT_LOC( "renderer_shadow_distance" ).c_str() );
                    changed |= imgui::Slider( "##ShadowDistance", settings.mShadowDistance, core::float2{ 200.0f, 700.0f } );

                    ImGui::EndTable();
                }

                DrawShadowDiagnostics( sceneRenderer.GetShadowDiagnostics() );
            }

            if ( ImGui::CollapsingHeader( MKT_LOC( "renderer_entity_selection_outline" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) &&
                 BeginRendererRows( "CompatibilityOutline" ) ) {
                RendererControlRow( MKT_LOC( "renderer_width" ).c_str() );
                changed |= imgui::Slider( "##OutlineWidth", settings.mSelectionOutlineWidth,
                    core::float2{ 1.0f, renderer::OutlineModule::kMaxOutlineWidth }, "%.1f px" );

                RendererControlRow( MKT_LOC( "renderer_color" ).c_str() );
                changed |= ImGui::ColorEdit4( "##OutlineColor", &settings.mSelectionOutlineColor.x,
                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB );

                ImGui::EndTable();
            }

            if ( changed ) {
                sceneRenderer.ApplySettings( settings );
            }

            ImGui::End();
            return;
        }

        {
            auto &renderer{ sceneRenderer };
            auto settings{ renderer.GetSettings() };
            bool changed{};

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_render_targets", "Render targets" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                if ( BeginRendererRows( "RenderTargetSettings" ) ) {
                    int resolution{ static_cast<int>( settings.mResolution ) };

                    RendererControlRow( MKT_LOC( "renderer_resolution" ).c_str() );
                    if ( ImGui::Combo( "##renderer_resolution", &resolution,
                             "1280 x 720\0"
                             "1920 x 1080\0"
                             "2560 x 1440\0"
                             "3840 x 2160\0" ) ) {
                        settings.mResolution = static_cast<renderer::rhi::RenderResolution>( resolution );
                        changed = true;
                    }

                    const char *sampleNames[]{ MKT_LOC( "renderer_off_1x" ).c_str(), "2x", "4x", "8x", "16x" };
                    const auto *device{ renderer::RenderSystem::Get()->GetGpuDevice() };
                    const auto maximum{ device->GetMaxSceneMultisampling() };

                    RendererControlRow( MKT_LOC( "renderer_geometry_msaa" ).c_str() );
                    if ( ImGui::BeginCombo( "##renderer_geometry_msaa", sampleNames[static_cast<unsigned>( settings.mMultisampling )] ) ) {
                        for ( unsigned index{}; index <= core::as<unsigned>( maximum ); ++index ) {
                            const auto samples{ static_cast<renderer::rhi::Multisampling>( index ) };

                            if ( !device->SupportsSceneMultisampling( samples ) ) {
                                continue;
                            }

                            if ( ImGui::Selectable( sampleNames[index], samples == settings.mMultisampling ) ) {
                                settings.mMultisampling = samples;
                                changed = true;
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::EndTable();
                }

                ImGui::TextWrapped( "%s", MKT_LOC( "renderer_msaa_covers_opaque_and_transparent_meshes_post_effects_text_particles" ).c_str() );
            }

            if ( ImGui::CollapsingHeader( MKT_LOC( "renderer_infinite_grid" ).c_str() ) ) {
                auto &grid{ settings.mInfiniteGrid };
                auto &visible{ mEditorState->mViewSettings.mShowGrid };

                if ( BeginRendererRows( "InfiniteGridSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    if ( imgui::CheckBox( "##infinite_grid_enabled", visible ) ) {
                        renderer.SetEnableInfiniteGrid( visible );
                    }

                    ImGui::BeginDisabled( !visible );
                    RendererControlRow( MKT_LOC( "renderer_grid_cell_size" ).c_str() );
                    changed |= DrawFloatProperty( "##grid_cell_size", &grid.mCellSize, { 0.01f, 10000.0f }, "%.2f m", true );
                    RendererControlRow( MKT_LOC( "renderer_grid_opacity" ).c_str() );
                    changed |= DrawFloatProperty( "##grid_opacity", &grid.mOpacity, { 0.0f, 1.0f }, "%.2f", true );
                    RendererControlRow( MKT_LOC( "renderer_grid_line_width" ).c_str() );
                    changed |= DrawFloatProperty( "##grid_line_width", &grid.mLineWidth, { 0.5f, 4.0f }, "%.1f px", true );
                    RendererControlRow( MKT_LOC( "renderer_grid_axis_width" ).c_str() );
                    changed |= DrawFloatProperty( "##grid_axis_width", &grid.mAxisWidth, { 0.5f, 8.0f }, "%.1f px", true );

                    // Keep the conventional world-axis colors beside each toggle.
                    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4{ 0.9f, 0.12f, 0.1f, 1.0f } );
                    RendererControlRow( MKT_LOC( "renderer_grid_x_axis" ).c_str() );
                    changed |= imgui::CheckBox( "##grid_x_axis", grid.mShowXAxis );
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4{ 0.12f, 0.8f, 0.22f, 1.0f } );
                    RendererControlRow( MKT_LOC( "renderer_grid_y_axis" ).c_str() );
                    changed |= imgui::CheckBox( "##grid_y_axis", grid.mShowYAxis );
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor( ImGuiCol_Text, ImVec4{ 0.12f, 0.35f, 0.95f, 1.0f } );
                    RendererControlRow( MKT_LOC( "renderer_grid_z_axis" ).c_str() );
                    changed |= imgui::CheckBox( "##grid_z_axis", grid.mShowZAxis );
                    ImGui::PopStyleColor();
                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }
            }

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_hardware_ray_tracing", "Hardware ray tracing" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                const auto *device{ renderer::RenderSystem::Get()->GetGpuDevice() };
                const bool supported{ device->SupportsRayTracing() };
                const bool hybridSupported{ device->IsGraphicsApi( renderer::rhi::GraphicsAPI::eVulkan ) || device->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D12 ) };
                auto &tracing{ settings.mRayTracing };
                ImGui::BeginDisabled( !supported );

                ImGui::SeparatorText( MKT_LOC( "renderer_path_tracing" ).c_str() );
                if ( BeginRendererRows( "PathTracingSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_path_tracing", tracing.mPathTracingEnabled );

                    if ( settings.mRayTracing.mPathTracingEnabled ) {
                        int samples{ static_cast<int>( settings.mRayTracing.mPathSamplesPerFrame ) };
                        int bounces{ static_cast<int>( settings.mRayTracing.mPathMaxBounces ) };
                        int limit{ static_cast<int>( settings.mRayTracing.mPathMaxSamples ) };
                        RendererControlRow( MKT_LOC( "renderer_samples_per_pixel" ).c_str() );
                        changed |= ImGui::SliderInt( "##renderer_samples_per_pixel",
                            &samples, 1, static_cast<int>( renderer::RayTracingConfiguration::kMaxPathSamplesPerFrame ) );
                        RendererControlRow( MKT_LOC( "renderer_maximum_bounces" ).c_str() );
                        changed |= ImGui::SliderInt( "##renderer_maximum_bounces", &bounces, 1, 32 );
                        const bool nrd{ settings.mDenoiser.mBackend == renderer::DenoiserBackend::eNrd && renderer.IsDenoiserActive() };
                        ImGui::BeginDisabled( nrd );
                        RendererControlRow( MKT_LOC( "renderer_sample_limit" ).c_str() );
                        changed |= ImGui::InputInt( "##renderer_sample_limit", &limit );
                        ImGui::EndDisabled();
                        settings.mRayTracing.mPathSamplesPerFrame = static_cast<core::u32>( glm::max( samples, 1 ) );
                        settings.mRayTracing.mPathMaxBounces = static_cast<core::u32>( glm::max( bounces, 1 ) );
                        settings.mRayTracing.mPathMaxSamples = static_cast<core::u32>( glm::max( limit, 1 ) );
                        if ( nrd ) {
                            RendererMetricRow( "Temporal history", "NRD (see denoising controls)" );
                        }
                        else {
                            RendererMetricRow( MKT_LOC( "renderer_accumulated_samples_label" ).c_str(),
                                string::Format( "{}", renderer.GetPathSampleCount() ).c_str() );
                        }

                        RendererControlRow( MKT_LOC( "renderer_accumulation" ).c_str() );
                        if ( ImGui::Button( imgui::widget::LocalizedLabel( "renderer_restart_accumulation", "Restart accumulation" ).c_str() ) ) {
                            if ( nrd ) {
                                ++settings.mDenoiser.mResetRevision;
                                changed = true;
                            }
                            else {
                                renderer.ResetPathAccumulation();
                            }
                        }
                    }
                    ImGui::EndTable();
                }

                // Hybrid controls stay visible while the path tracer is selected.
                // Their values are retained for returning to raster rendering.
                ImGui::BeginDisabled( tracing.mPathTracingEnabled || !hybridSupported );
                ImGui::SeparatorText( MKT_LOC( "renderer_ray_traced_shadows" ).c_str() );
                if ( BeginRendererRows( "RayTracedShadowSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_ray_traced_shadows", tracing.mShadowsEnabled );
                    ImGui::BeginDisabled( !tracing.mShadowsEnabled );
                    int samples{ core::as<int>( tracing.mShadowSamplesPerFrame ) };
                    RendererControlRow( MKT_LOC( "renderer_samples_per_pixel" ).c_str() );
                    changed |= ImGui::SliderInt( "##rt_shadow_samples", &samples, 1, 16 );
                    tracing.mShadowSamplesPerFrame = core::as<core::u32>( samples );
                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }
                ImGui::TextWrapped( "%s", MKT_LOC( "renderer_ray_shadow_softness_help" ).c_str() );

                ImGui::SeparatorText( MKT_LOC( "renderer_ray_traced_reflections" ).c_str() );
                if ( BeginRendererRows( "RayTracedReflectionSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_ray_traced_reflections", tracing.mReflectionsEnabled );
                    ImGui::BeginDisabled( !tracing.mReflectionsEnabled );
                    int samples{ core::as<int>( tracing.mRayGiSamplesPerFrame ) };
                    int bounces{ core::as<int>( tracing.mReflectionMaxBounces ) };
                    RendererControlRow( MKT_LOC( "renderer_indirect_samples_per_frame" ).c_str() );
                    changed |= ImGui::SliderInt( "##rt_reflection_samples", &samples, 1, 8 );
                    imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_shared_indirect_sampling_help" ).c_str() );
                    RendererControlRow( MKT_LOC( "renderer_maximum_bounces" ).c_str() );
                    changed |= ImGui::SliderInt( "##rt_reflection_bounces", &bounces, 1, 8 );
                    tracing.mRayGiSamplesPerFrame = core::as<core::u32>( samples );
                    tracing.mReflectionMaxBounces = core::as<core::u32>( bounces );
                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }

                ImGui::SeparatorText( MKT_LOC( "renderer_ray_traced_global_illumination" ).c_str() );
                if ( BeginRendererRows( "RayTracedGiSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_ray_traced_global_illumination", tracing.mGlobalIlluminationEnabled );
                    ImGui::BeginDisabled( !tracing.mGlobalIlluminationEnabled );
                    int samples{ core::as<int>( tracing.mRayGiSamplesPerFrame ) };
                    int bounces{ core::as<int>( tracing.mRayGiMaxBounces ) };
                    RendererControlRow( MKT_LOC( "renderer_indirect_samples_per_frame" ).c_str() );
                    changed |= ImGui::SliderInt( "##renderer_gi_samples_per_frame", &samples, 1, 8 );
                    imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_shared_indirect_sampling_help" ).c_str() );
                    RendererControlRow( MKT_LOC( "renderer_gi_indirect_bounces" ).c_str() );
                    changed |= ImGui::SliderInt( "##renderer_gi_indirect_bounces", &bounces, 1, 8 );
                    tracing.mRayGiSamplesPerFrame = core::as<core::u32>( samples );
                    tracing.mRayGiMaxBounces = core::as<core::u32>( bounces );
                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }
                ImGui::EndDisabled();

                ImGui::SeparatorText( MKT_LOC( "renderer_ray_traced_ao" ).c_str() );
                if ( BeginRendererRows( "RayTracedAoSettings" ) ) {
                    ImGui::BeginDisabled( tracing.mPathTracingEnabled );
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_ray_traced_ao", tracing.mAmbientOcclusionEnabled );
                    ImGui::EndDisabled();
                    int samples{ core::as<int>( tracing.mAoSamplesPerFrame ) };
                    RendererControlRow( MKT_LOC( "renderer_ao_samples" ).c_str() );
                    changed |= ImGui::SliderInt( "##renderer_ao_samples", &samples, 1, 64 );
                    tracing.mAoSamplesPerFrame = core::as<core::u32>( samples );
                    RendererControlRow( MKT_LOC( "renderer_ao_distance" ).c_str() );
                    changed |= DrawFloatProperty( "##renderer_ao_distance", &tracing.mAoMaxDistance, { 0.0f, 10000.0f }, "%.3f", true );
                    RendererControlRow( MKT_LOC( "renderer_ao_cosine" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_ao_cosine", tracing.mUseCosineWeightedAoSampling );
                    RendererControlRow( MKT_LOC( "renderer_ao_debug" ).c_str() );
                    if ( ImGui::Button( imgui::widget::LocalizedLabel( "renderer_ao_debug", "View ray-traced AO" ).c_str() ) ) {
                        mEditorState->mFinalComposition = renderer.GetFinalImage( renderer::FinalImageType::eRayTracedAmbientOcclusion );
                    }
                    ImGui::EndTable();
                }
                ImGui::TextWrapped( "%s", MKT_LOC( "renderer_ao_help" ).c_str() );
                ImGui::EndDisabled();

                if ( !supported ) {
                    ImGui::TextWrapped( "%s", MKT_LOC( "renderer_hardware_ray_tracing_is_unavailable_on_this_device_backend" ).c_str() );
                }
                else if ( !hybridSupported ) {
                    ImGui::TextWrapped( "%s", MKT_LOC( "renderer_hybrid_effects_require_the_vulkan_or_d3d12_scene_renderer" ).c_str() );
                }
                if ( !renderer.GetRayTracingError().empty() ) {
                    ImGui::TextWrapped( "%s", renderer.GetRayTracingError().c_str() );
                }
            }

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_rasterization", "Rasterization" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                ImGui::BeginDisabled( settings.mRayTracing.mPathTracingEnabled );
                ImGui::SeparatorText( MKT_LOC( "renderer_ssao" ).c_str() );
                if ( BeginRendererRows( "RasterSsaoSettings" ) ) {
                    ImGui::PushID( "SSAO" );
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##SSAO", settings.mSsaoEnabled );

                    ImGui::BeginDisabled( !settings.mSsaoEnabled );
                    RendererControlRow( MKT_LOC( "renderer_intensity" ).c_str() );
                    changed |= imgui::Slider( "##renderer_intensity_ssao", settings.mSsaoIntensity, core::float2{ 0.0f, 4.0f } );
                    ImGui::EndDisabled();
                    ImGui::PopID();

                    ImGui::EndTable();
                }

                ImGui::SeparatorText( MKT_LOC( "renderer_shadows" ).c_str() );
                if ( BeginRendererRows( "ShadowSettings" ) ) {
                    int quality{ core::as<int>( settings.mShadowQuality ) };
                    RendererControlRow( MKT_LOC( "renderer_quality" ).c_str() );
                    if ( ImGui::Combo( "##ShadowQuality", &quality, MKT_LOC( "renderer_low_medium_high" ).c_str() ) ) {
                        settings.mShadowQuality = static_cast<renderer::ShadowMapQuality>( quality );
                        changed = true;
                    }

                    imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_controls_shadow_map_detail_ray_traced_shadows_and_path_tracing" ).c_str() );
                    RendererControlRow( MKT_LOC( "renderer_shadow_distance" ).c_str() );
                    changed |= imgui::Slider( "##ShadowDistance", settings.mShadowDistance, core::float2{ 200.0f, 700.0f } );

                    RendererControlRow( MKT_LOC( "renderer_cascaded_shadows" ).c_str() );
                    changed |= imgui::CheckBox( "##CascadedShadows", settings.mCascadedShadowsEnabled );
                    imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_cascaded_shadows_help" ).c_str() );

                    RendererControlRow( MKT_LOC( "renderer_soft_shadows" ).c_str() );
                    changed |= imgui::CheckBox( "##SoftShadows", settings.mSoftShadowsEnabled );
                    imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_soft_shadows_help" ).c_str() );

                    RendererControlRow( MKT_LOC( "renderer_visualize_cascades" ).c_str() );
                    changed |= imgui::CheckBox( "##VisualizeCascades", settings.mVisualizeCascades );
                    imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_scene_tint_red_green_blue_yellow_from_near_to_far" ).c_str() );

                    ImGui::EndTable();
                }

                DrawShadowDiagnostics( renderer.GetShadowDiagnostics() );

                ImGui::SeparatorText( MKT_LOC( "renderer_ssr_title" ).c_str() );
                const bool replaced{ settings.mRayTracing.mPathTracingEnabled || settings.mRayTracing.mReflectionsEnabled };
                ImGui::BeginDisabled( replaced );
                if ( BeginRendererRows( "ScreenSpaceReflectionSettings" ) ) {
                    auto &reflections{ settings.mScreenSpaceReflections };
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##ssr_enabled", reflections.mEnabled );
                    ImGui::BeginDisabled( !reflections.mEnabled );
                    RendererControlRow( MKT_LOC( "renderer_ssr_half_resolution" ).c_str() );
                    changed |= imgui::CheckBox( "##ssr_half_resolution", reflections.mHalfResolution );

                    int steps{ core::as<int>( reflections.mMaxSteps ) };
                    int samples{ core::as<int>( reflections.mSpatialSamples ) };
                    RendererControlRow( MKT_LOC( "renderer_ssr_max_steps" ).c_str() );
                    changed |= ImGui::SliderInt( "##ssr_max_steps", &steps, 16, 256 );
                    RendererControlRow( MKT_LOC( "renderer_ssr_spatial_samples" ).c_str() );
                    changed |= ImGui::SliderInt( "##ssr_spatial_samples", &samples, 1, 16 );
                    reflections.mMaxSteps = core::as<core::u32>( steps );
                    reflections.mSpatialSamples = core::as<core::u32>( samples );
                    RendererControlRow( MKT_LOC( "renderer_ssr_max_distance" ).c_str() );
                    changed |= imgui::Slider( "##ssr_max_distance", reflections.mMaxDistance, core::float2{ 0.1f, 200.0f }, "%.1f m" );
                    RendererControlRow( MKT_LOC( "renderer_ssr_thickness" ).c_str() );
                    changed |= imgui::Slider( "##ssr_thickness", reflections.mThickness, core::float2{ 0.005f, 1.0f }, "%.3f m" );
                    RendererControlRow( MKT_LOC( "renderer_ssr_max_roughness" ).c_str() );
                    changed |= imgui::Slider( "##ssr_max_roughness", reflections.mMaxRoughness, core::float2{ 0.05f, 1.0f }, "%.2f" );
                    RendererControlRow( MKT_LOC( "renderer_ssr_history_weight" ).c_str() );
                    changed |= imgui::Slider( "##ssr_history_weight", reflections.mHistoryWeight, core::float2{ 0.0f, 0.98f }, "%.2f" );
                    RendererControlRow( MKT_LOC( "renderer_ssr_sampling_bias" ).c_str() );
                    changed |= imgui::Slider( "##ssr_sampling_bias", reflections.mSamplingBias, core::float2{ 0.0f, 0.9f }, "%.2f" );
                    RendererControlRow( MKT_LOC( "renderer_intensity" ).c_str() );
                    changed |= imgui::Slider( "##ssr_intensity", reflections.mIntensity, core::float2{ 0.0f, 1.0f }, "%.2f" );
                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }
                ImGui::EndDisabled();
                ImGui::TextWrapped( "%s", MKT_LOC( replaced ? "renderer_ssr_replaced" : "renderer_ssr_help" ).c_str() );
                ImGui::EndDisabled();
                if ( settings.mRayTracing.mPathTracingEnabled ) {
                    ImGui::TextDisabled( "%s", MKT_LOC( "renderer_available_in_raster_rendering" ).c_str() );
                }
            }

            if ( ImGui::CollapsingHeader( "Denoising" ) ) {
                auto &denoiser{ settings.mDenoiser };
                if ( BeginRendererRows( "DenoiserSelection" ) ) {
                    RendererControlRow( "Active denoiser" );
                    const char *backends[]{ "NRD", "OIDN" };
                    int activeBackend{ static_cast<int>( denoiser.mBackend ) };
                    if ( ImGui::Combo( "##ActiveDenoiser", &activeBackend, backends, IM_ARRAYSIZE( backends ) ) ) {
                        denoiser.mBackend = static_cast<renderer::DenoiserBackend>( activeBackend );
                        changed = true;
                    }
                    RendererControlRow( "Enabled" );
                    changed |= imgui::CheckBox( "##DenoiserEnabled", denoiser.mEnabled );
                    RendererMetricRow( "Current view", renderer.IsPreparing() ? "Preparing" : renderer.IsDenoiserActive() ? "Filtering" : denoiser.mEnabled ? "Progressive / no matching RT signal" : "Disabled" );
                    ImGui::EndTable();
                }

                // The dropdown selects execution; tabs only inspect settings.
                // Reveal the active backend when its selection changes, then
                // allow either settings tab to be inspected independently.
                auto *storage{ ImGui::GetStateStorage() };
                const ImGuiID activeID{ ImGui::GetID( "##ObservedActiveDenoiser" ) };
                const ImGuiID viewID{ ImGui::GetID( "##DenoiserSettingsView" ) };
                const int activeBackend{ static_cast<int>( denoiser.mBackend ) };
                const bool revealActive{ storage->GetInt( activeID, -1 ) != activeBackend };
                int settingsView{ storage->GetInt( viewID, activeBackend ) };
                if ( ImGui::BeginTabBar( "##DenoiserBackends" ) ) {
                    const auto nrdFlags{ revealActive && denoiser.mBackend == renderer::DenoiserBackend::eNrd ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None };
                    if ( ImGui::BeginTabItem( "NRD", nullptr, nrdFlags ) ) {
                        settingsView = static_cast<int>( renderer::DenoiserBackend::eNrd );
                        ImGui::EndTabItem();
                    }
                    const auto oidnFlags{ revealActive && denoiser.mBackend == renderer::DenoiserBackend::eOidn ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None };
                    if ( ImGui::BeginTabItem( "OIDN", nullptr, oidnFlags ) ) {
                        settingsView = static_cast<int>( renderer::DenoiserBackend::eOidn );
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                storage->SetInt( activeID, activeBackend );
                storage->SetInt( viewID, settingsView );

                if ( settingsView == static_cast<int>( renderer::DenoiserBackend::eNrd ) ) {
                    const bool supported{ renderer::RenderSystem::Get()->GetGpuDevice()->SupportsRayTracing() && MIKOTO_ENABLE_NRD };
                    ImGui::BeginDisabled( !supported );
                    if ( BeginRendererRows( "DenoiserSettings" ) ) {
                        ImGui::BeginDisabled( !denoiser.mEnabled );
                        RendererControlRow( "Method" );
                        const char *methods[]{ "REBLUR", "RELAX" };
                        int method{ static_cast<int>( denoiser.mType ) };
                        if ( ImGui::Combo( "##nrd_method", &method, methods, 2 ) ) {
                            denoiser.mType = static_cast<renderer::DenoiserType>( method );
                            changed = true;
                        }
                        int history{ static_cast<int>( denoiser.mHistoryFrames ) };
                        int fast{ static_cast<int>( denoiser.mFastHistoryFrames ) };
                        RendererControlRow( "History frames" );
                        changed |= ImGui::SliderInt( "##nrd_history", &history, 1, 63 );
                        RendererControlRow( "Fast history frames" );
                        changed |= ImGui::SliderInt( "##nrd_fast_history", &fast, 1, history );
                        denoiser.mHistoryFrames = static_cast<core::u32>( history );
                        denoiser.mFastHistoryFrames = static_cast<core::u32>( fast );
                        if ( denoiser.mType == renderer::DenoiserType::eRelax ) {
                            int iterations{ static_cast<int>( denoiser.mAtrousIterations ) };
                            RendererControlRow( "A-trous iterations" );
                            changed |= ImGui::SliderInt( "##nrd_atrous", &iterations, 2, 8 );
                            denoiser.mAtrousIterations = static_cast<core::u32>( iterations );
                        }
                        RendererControlRow( "Disocclusion threshold" );
                        changed |= ImGui::SliderFloat( "##nrd_disocclusion", &denoiser.mDisocclusionThreshold, .001f, .1f, "%.3f" );
                        RendererControlRow( "Adaptive ray budget" );
                        changed |= imgui::CheckBox( "##nrd_adaptive_sampling", denoiser.mAdaptiveSampling );
                        imgui::widget::ItemTooltip( "Uses one sample per pixel while the camera moves; stationary views use the requested SPP." );
                        RendererControlRow( "Responsive lighting history" );
                        changed |= imgui::CheckBox( "##nrd_responsive_history", denoiser.mResponsiveHistory );
                        imgui::widget::ItemTooltip( "Reduces temporal lag when video screens or scene lighting change." );
                        RendererControlRow( "Anti-firefly" );
                        changed |= imgui::CheckBox( "##nrd_antifirefly", denoiser.mAntiFirefly );
                        RendererControlRow( "History" );
                        if ( ImGui::Button( "Reset history##nrd" ) ) {
                            ++denoiser.mResetRevision;
                            changed = true;
                        }
                        ImGui::EndDisabled();
                        ImGui::EndTable();
                    }
                    ImGui::EndDisabled();
                    ImGui::TextWrapped( "NRD filters supported surfaces. Glass, media and depth of field retain progressive accumulation. Preview controls are in Tools > Graphics > Output." );
                    if ( !MIKOTO_ENABLE_NRD ) {
                        ImGui::TextWrapped( "NRD is disabled in this build." );
                    }
                }
                else {
                    const auto *device{ renderer::RenderSystem::Get()->GetGpuDevice() };
                    const bool supported{ MIKOTO_ENABLE_OIDN && device->GetGraphicsApi() == renderer::rhi::GraphicsAPI::eVulkan && device->SupportsRayTracing() };
                    ImGui::BeginDisabled( !supported );
                    if ( BeginRendererRows( "OidnSettings" ) ) {
                        ImGui::BeginDisabled( !denoiser.mEnabled );
                        RendererControlRow( "Quality" );
                        const char *qualities[]{ "Fast", "Balanced", "High" };
                        int quality{ static_cast<int>( denoiser.mOidnQuality ) };
                        if ( ImGui::Combo( "##oidn_quality", &quality, qualities, IM_ARRAYSIZE( qualities ) ) ) {
                            denoiser.mOidnQuality = static_cast<renderer::OidnQuality>( quality );
                            changed = true;
                        }
                        RendererControlRow( "Raw / denoised split" );
                        changed |= ImGui::SliderFloat( "##oidn_split", &denoiser.mSplitScreen, 0.0f, 1.0f, "%.2f" );
                        ImGui::EndDisabled();
                        ImGui::EndTable();
                    }
                    ImGui::EndDisabled();
                    ImGui::TextWrapped( "OIDN filters the full HDR image, including transmission, fog and depth of field. Progressive accumulation continues; High quality costs more GPU time." );
                    if ( !supported ) {
                        ImGui::TextWrapped( "OIDN requires a Vulkan build with OIDN enabled and hardware ray tracing." );
                    }
                }
            }

            if ( ImGui::CollapsingHeader( MKT_LOC( "renderer_clouds_title" ).c_str() ) ) {
                const bool pathTracing{ settings.mRayTracing.mPathTracingEnabled };
                ImGui::BeginDisabled( pathTracing );
                if ( BeginRendererRows( "CloudSettings" ) ) {
                    auto &clouds{ settings.mClouds };
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##clouds_enabled", clouds.mEnabled );
                    ImGui::BeginDisabled( !clouds.mEnabled );
                    RendererControlRow( MKT_LOC( "renderer_ssr_half_resolution" ).c_str() );
                    changed |= imgui::CheckBox( "##clouds_half_resolution", clouds.mHalfResolution );
                    RendererControlRow( MKT_LOC( "renderer_clouds_shadows" ).c_str() );
                    changed |= imgui::CheckBox( "##clouds_surface_shadows", clouds.mCastSurfaceShadows );

                    RendererControlRow( "Packed distance field" );
                    changed |= imgui::CheckBox( "##clouds_distance_field", clouds.mDistanceFieldEnabled );
                    RendererControlRow( "Edge darkening" );
                    changed |= DrawFloatProperty( "##clouds_mEdgeDarkening", &clouds.mEdgeDarkening, { 0.0f, 1.0f }, "%.2f", true );
                    RendererControlRow( "Inner glow" );
                    changed |= DrawFloatProperty( "##clouds_mInnerGlow", &clouds.mInnerGlow, { 0.0f, 1.0f }, "%.2f", true );
                    RendererControlRow( "Detail mip bias" );
                    changed |= DrawFloatProperty( "##clouds_mDetailMipBias", &clouds.mDetailMipBias, { -2.0f, 2.0f }, "%.2f", true );
                    RendererControlRow( "Animated jitter range" );
                    changed |= DrawFloatProperty( "##clouds_mAnimatedJitterDistance", &clouds.mAnimatedJitterDistance, { 0.0f, 2000.0f }, "%.0f m", true );

                    int resolution{ clouds.mFieldResolution <= 64 ? 0 : clouds.mFieldResolution <= 128 ? 1
                                                                                                       : 2 };
                    int steps{ core::as<int>( clouds.mMaxSteps ) };
                    int sunSteps{ core::as<int>( clouds.mSunSteps ) };
                    int debug{ core::as<int>( clouds.mDebugView ) };
                    RendererControlRow( MKT_LOC( "renderer_clouds_field_resolution" ).c_str() );
                    changed |= ImGui::Combo( "##clouds_field_resolution", &resolution,
                        "64 x 32 x 64\0"
                        "128 x 64 x 128\0"
                        "256 x 128 x 256\0" );
                    clouds.mFieldResolution = 64u << resolution;
                    RendererControlRow( MKT_LOC( "renderer_clouds_view_steps" ).c_str() );
                    changed |= ImGui::SliderInt( "##clouds_view_steps", &steps, 32, 512 );
                    RendererControlRow( MKT_LOC( "renderer_clouds_light_steps" ).c_str() );
                    changed |= ImGui::SliderInt( "##clouds_light_steps", &sunSteps, 4, 64 );
                    clouds.mMaxSteps = core::as<core::u32>( steps );
                    clouds.mSunSteps = core::as<core::u32>( sunSteps );
                    RendererControlRow( MKT_LOC( "renderer_clouds_step_length" ).c_str() );
                    changed |= imgui::Slider( "##clouds_step_length", clouds.mStepLength, core::float2{ .5f, 128.0f }, "%.1f m" );
                    RendererControlRow( MKT_LOC( "renderer_clouds_max_distance" ).c_str() );
                    changed |= imgui::Slider( "##clouds_max_distance", clouds.mMaxDistance, core::float2{ 100.0f, 200000.0f }, "%.0f m" );
                    RendererControlRow( MKT_LOC( "renderer_clouds_detail" ).c_str() );
                    changed |= imgui::Slider( "##clouds_detail", clouds.mDetailStrength, core::float2{ 0.0f, 2.0f }, "%.2f" );
                    RendererControlRow( MKT_LOC( "renderer_clouds_scattering" ).c_str() );
                    changed |= imgui::Slider( "##clouds_scattering", clouds.mMultipleScattering, core::float2{ 0.0f, .9f }, "%.2f" );
                    RendererControlRow( MKT_LOC( "renderer_clouds_ambient" ).c_str() );
                    changed |= imgui::Slider( "##clouds_ambient", clouds.mAmbientIntensity, core::float2{ 0.0f, 4.0f }, "%.2f" );
                    RendererControlRow( MKT_LOC( "renderer_clouds_history" ).c_str() );
                    changed |= imgui::Slider( "##clouds_history", clouds.mHistoryWeight, core::float2{ 0.0f, .98f }, "%.2f" );
                    RendererControlRow( MKT_LOC( "renderer_clouds_debug" ).c_str() );
                    changed |= ImGui::Combo( "##clouds_debug", &debug, MKT_LOC( "renderer_clouds_debug_options" ).c_str() );
                    clouds.mDebugView = static_cast<renderer::CloudsDebugView>( debug );
                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }
                ImGui::EndDisabled();
                ImGui::TextWrapped( "%s", MKT_LOC( pathTracing ? "renderer_clouds_path_help" : "renderer_clouds_help" ).c_str() );
            }

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_entity_selection_outline", "Entity selection outline" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                if ( BeginRendererRows( "OutlineSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_width" ).c_str() );
                    changed |= imgui::Slider( "##renderer_width", settings.mSelectionOutlineWidth, core::float2{ 1.0f, renderer::OutlineModule::kMaxOutlineWidth }, "%.1f px" );
                    RendererControlRow( MKT_LOC( "renderer_color" ).c_str() );
                    changed |= ImGui::ColorEdit4( "##renderer_color", &settings.mSelectionOutlineColor.x,
                        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB );

                    ImGui::EndTable();
                }

                ImGui::TextDisabled( MKT_LOC( "renderer_applied_to_the_selected_entity_and_its_highlighted_hierarchy" ).c_str() );
            }

            if ( auto *particles{ renderer::ParticleSystem::GetPtr() }; particles && ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_particles", "Particles" ).c_str() ) ) {
                const auto mode{ particles->GetSimulationMode() };

                if ( BeginRendererRows( "ParticleSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_simulation" ).c_str() );
                    if ( ImGui::BeginCombo( "##renderer_simulation", mode == renderer::ParticleSimulationMode::eGpu ? MKT_LOC( "renderer_gpu_compute" ).c_str() : MKT_LOC( "renderer_cpu" ).c_str() ) ) {
                        for ( const auto choice: { renderer::ParticleSimulationMode::eCpu, renderer::ParticleSimulationMode::eGpu } ) {
                            if ( ImGui::Selectable( choice == renderer::ParticleSimulationMode::eGpu ? MKT_LOC( "renderer_gpu_compute" ).c_str() : MKT_LOC( "renderer_cpu" ).c_str(), choice == mode ) ) {
                                particles->SetSimulationMode( choice );
                            }
                        }

                        ImGui::EndCombo();
                    }

                    ImGui::EndTable();
                }

                ImGui::TextWrapped( MKT_LOC( "renderer_applies_to_all_scenes_switching_restarts_emitters_on_their_next" ).c_str() );

                if ( mode == renderer::ParticleSimulationMode::eGpu ) {
                    ImGui::TextWrapped( MKT_LOC( "renderer_experimental_256_emitters_and_65_536_reserved_particle_slots_per" ).c_str() );
                }
            }

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_cinematic_depth_of_field", "Cinematic depth of field" ).c_str() ) ) {
                ImGui::PushID( "CinematicDoF" );
                ImGui::BeginDisabled( settings.mRayTracing.mPathTracingEnabled );

                if ( BeginRendererRows( "DepthOfFieldSettings" ) ) {
                    int quality{ static_cast<int>( settings.mDepthOfFieldQuality ) };

                    RendererControlRow( MKT_LOC( "renderer_quality" ).c_str() );
                    if ( ImGui::Combo( "##renderer_quality", &quality, MKT_LOC( "renderer_off_low_medium_high_cinematic" ).c_str() ) ) {
                        settings.mDepthOfFieldQuality = static_cast<renderer::DepthOfFieldQuality>( quality );
                        changed = true;
                    }

                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "renderer_controls_aperture_sampling_higher_settings_improve_bokeh_at_a_greater" ).c_str() );

                    ImGui::BeginDisabled( settings.mDepthOfFieldQuality == renderer::DepthOfFieldQuality::eOff );

                    RendererControlRow( MKT_LOC( "renderer_visualize_focus_layers" ).c_str() );
                    if ( imgui::CheckBox( "##renderer_visualize_focus_layers", settings.mVisualizeDepthOfField ) ) {
                        changed = true;

                        if ( settings.mVisualizeDepthOfField ) {
                            mEditorState->mFinalComposition = renderer.GetFinalImage( renderer::FinalImageType::eTonemap_Output );
                        }
                    }

                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "renderer_shows_the_camera_s_circle_of_confusion_before_blur_brightness" ).c_str() );

                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }

                if ( settings.mVisualizeDepthOfField ) {
                    ImGui::TextColored( ImVec4{ 0.2f, 1.0f, 0.2f, 1.0f }, "%s", MKT_LOC( "renderer_green_foreground" ).c_str() );
                    ImGui::TextColored( ImVec4{ 0.3f, 0.5f, 1.0f, 1.0f }, "%s", MKT_LOC( "renderer_blue_background" ).c_str() );
                    ImGui::TextUnformatted( MKT_LOC( "renderer_black_in_focus" ).c_str() );
                }

                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::TextWrapped( "%s", MKT_LOC( "renderer_enable_dof_and_edit_the_lens_in_the_camera_inspector" ).c_str() );

                if ( settings.mRayTracing.mPathTracingEnabled ) {
                    ImGui::TextDisabled( "%s", MKT_LOC( "renderer_available_in_raster_rendering" ).c_str() );
                }

                ImGui::PopID();
            }

            if ( changed ) {
                renderer.CancelPick();
                renderer.ApplySettings( settings );
                mEditorState->mResolution = settings.mResolution;
            }
        }

        imgui::DrawNode( MKT_LOC( "renderer_tone_mapping" ).c_str(), [this]() -> void {
            const auto scene{ mEditorState->mActiveScene };

            if ( !scene ) {
                ImGui::TextDisabled( "%s", MKT_LOC( "renderer_open_a_scene_to_configure_tone_mapping" ).c_str() );
                return;
            }

            const eastl::array<core::cstr, 5> names{
                MKT_LOC( "renderer_aces_fitted" ).c_str(), MKT_LOC( "renderer_linear_clamp" ).c_str(), "Reinhard", "Uncharted 2", "Khronos PBR Neutral"
            };
            const eastl::array<core::cstr, 5> descriptions{
                MKT_LOC( "renderer_filmic_contrast_with_compressed_highlights" ).c_str(),
                MKT_LOC( "renderer_applies_exposure_and_clamps_highlights_useful_for_comparison" ).c_str(),
                MKT_LOC( "renderer_smoothly_compresses_each_color_channel_as_brightness_increases" ).c_str(),
                MKT_LOC( "renderer_filmic_curve_normalized_against_a_reference_white_point" ).c_str(),
                MKT_LOC( "renderer_preserves_color_through_the_midtones_and_compresses_bright_highlights" ).c_str()
            };
            static_assert( names.size() == static_cast<core::usize>( renderer::ToneMapper::eCount ) );
            core::i32 selected{ static_cast<core::i32>( scene->GetToneMapping() ) };

            if ( BeginRendererRows( "ToneMappingSettings" ) ) {
                RendererControlRow( MKT_LOC( "renderer_tone_mapper" ).c_str() );
                if ( ImGui::Combo( "##ToneMapping", &selected, names.data(), static_cast<core::i32>( names.size() ) ) ) {
                    const auto type{ static_cast<renderer::ToneMapper>( selected ) };
                    // Scene ownership preserves the choice through save/load and
                    // the editor's Play snapshot. Apply it immediately for preview.
                    scene->SetToneMapping( type );
                    mEditorState->mEditorRenderer->SetTonemapType( type );
                }

                ImGui::EndTable();
            }

            ImGui::TextWrapped( "%s", descriptions[static_cast<core::usize>( selected )] );
            ImGui::TextDisabled( "%s", MKT_LOC( "renderer_exposure_and_gamma_follow_the_active_camera" ).c_str() );
            ImGui::TextWrapped( "%s", MKT_LOC( "renderer_view_the_default_output_to_compare_operators_saved_with_the" ).c_str() );
        } );
        imgui::DrawNode( MKT_LOC( "renderer_frustum_culling" ).c_str(), [this]() -> void {
            auto settings{ mEditorState->mEditorRenderer->GetSettings() };

            if ( BeginRendererRows( "FrustumCullingSettings" ) ) {
                RendererControlRow( MKT_LOC( "renderer_enable_frustum_culling" ).c_str() );
                if ( imgui::CheckBox( "##renderer_enable_frustum_culling", settings.mFrustumCullingEnabled ) ) {
                    mEditorState->mEditorRenderer->ApplySettings( settings );
                }

                ImGui::SameLine();
                imgui::widget::MakeHelpPopUp( MKT_LOC( "renderer_disable_to_check_whether_camera_frustum_tests_are_hiding_geometry" ).c_str() );

                RendererControlRow( MKT_LOC( "renderer_visualize_main_camera_frustum" ).c_str() );
                if ( imgui::CheckBox( "##renderer_visualize_main_camera_frustum", settings.mVisualizeFrustum ) ) {
                    mEditorState->mEditorRenderer->ApplySettings( settings );

                    if ( settings.mVisualizeFrustum ) {
                        mEditorState->mFinalComposition = mEditorState->mEditorRenderer->GetFinalImage( renderer::FinalImageType::eTonemap_Output );
                    }
                }

                ImGui::SameLine();
                imgui::widget::MakeHelpPopUp( MKT_LOC( "renderer_scene_view_only_green_intersecting_draw_bounds_red_outside_amber" ).c_str() );

                ImGui::EndTable();
            }

            if ( !mEditorState->mGameCamera ) {
                ImGui::TextWrapped( "%s", MKT_LOC( "renderer_set_an_active_camera_as_main_camera_in_the_inspector" ).c_str() );
            }
        } );
        imgui::DrawNode( MKT_LOC( "renderer_bloom" ).c_str(), [this]() -> void {
            auto &bloom{ mEditorState->mEditorRenderer->GetBloomSettings() };
            if ( BeginRendererRows( "BloomSettings" ) ) {
                RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                imgui::CheckBox( "##renderer_enabled", bloom.mEnabled );
                RendererControlRow( MKT_LOC( "renderer_threshold" ).c_str() );
                imgui::Slider( "##renderer_threshold", bloom.mThreshold, core::float2{ 0.0f, 100.0f }, "%.2f" );
                RendererControlRow( MKT_LOC( "renderer_intensity" ).c_str() );
                imgui::Slider( "##renderer_intensity", bloom.mIntensity, core::float2{ 0.0f, 10.0f }, "%.2f" );
                RendererControlRow( MKT_LOC( "renderer_scatter" ).c_str() );
                imgui::Slider( "##renderer_scatter", bloom.mScatter, core::float2{ 0.0f, 1.0f }, "%.2f" );
                RendererControlRow( MKT_LOC( "renderer_radius" ).c_str() );
                imgui::Slider( "##renderer_radius", bloom.mRadius, core::float2{ 0.5f, 3.0f }, "%.2f" );

                ImGui::EndTable();
            }

            ImGui::TextWrapped( "%s", MKT_LOC( "renderer_bloom_uses_emissive_materials_with_enable_bloom_checked_intensity_sets" ).c_str() );
        } );

        ImGui::End();
    }
}// namespace mikoto::editor
