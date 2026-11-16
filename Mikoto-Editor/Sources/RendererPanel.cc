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
#include <Core/Profiler.hh>
#include <Core/TimeService.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/LocalizationService.hh>

#include <Memory/Allocator.hh>

#include <Text/Utility.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/RayTracing/RayTracing.hh>
#include <Renderer/ForwardRenderer/Passes/OutlineModule.hh>
#include <Renderer/Particle/ParticleSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/GraphEditor.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/RendererPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD static auto BeginRendererRows( const char* id, ImGuiTableFlags flags = ImGuiTableFlags_None ) -> bool {
        if ( !ImGui::BeginTable( id, 2, flags | ImGuiTableFlags_NoSavedSettings ) ) {
            return false;
        }

        ImGui::TableSetupColumn( "##Label", ImGuiTableColumnFlags_WidthStretch, 3.0f );
        ImGui::TableSetupColumn( "##Value", ImGuiTableColumnFlags_WidthStretch, 2.0f );
        return true;
    }

    static auto RendererControlRow( const char* label ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextWrapped( "%s", label );

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth( -1.0f );
    }

    static auto RendererMetricValue( const char* value ) -> void {
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

    static auto RendererMetricRow( const char* label, const char* value ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextWrapped( "%s", label );

        ImGui::TableNextColumn();
        RendererMetricValue( value );
    }

    static auto DrawShadowDiagnostics( const renderer::ShadowDiagnostics& shadows ) -> void {
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

    RendererPanel::RendererPanel( const RendererPanelCreateInfo& createInfo )
        : Panel{ "Renderer", "Panel", createInfo.mGroup, createInfo.mIsToggleable },
          mEditorState{ createInfo.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_POWER_SETTINGS_NEW, mPanelName );

        if ( !mEditorState->mEditorRenderer || !mEditorState->mEditorRenderer->HasFrameGraph() ) {
            return;
        }

        // Construct the node graph for pass dependencies
        imgui::GraphEditorBuilder builder{};
        auto& nodeControl{ mEditorState->mEditorRenderer->GetNodeControl() };

        for ( const auto& entry: nodeControl.mNodes ) {
            const auto& passName{ entry.first };
            const auto& pass{ entry.second };

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

        auto& sceneRenderer{ *mEditorState->mEditorRenderer };
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
            auto& renderer{ sceneRenderer };
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

                    const char* sampleNames[]{ MKT_LOC( "renderer_off_1x" ).c_str(), "2x", "4x", "8x", "16x" };
                    const auto* device{ renderer::RenderSystem::Get()->GetGpuDevice() };
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

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_hardware_ray_tracing", "Hardware ray tracing" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                const bool supported{ renderer::RenderSystem::Get()->GetGpuDevice()->SupportsRayTracing() };
                if ( BeginRendererRows( "RayTracingSettings" ) ) {
                    ImGui::BeginDisabled( !supported );
                    RendererControlRow( MKT_LOC( "renderer_path_tracing" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_path_tracing", settings.mRayTracing.mPathTracingEnabled );

                    if ( settings.mRayTracing.mPathTracingEnabled ) {
                        int samples{ static_cast<int>( settings.mRayTracing.mPathSamplesPerFrame ) };
                        int bounces{ static_cast<int>( settings.mRayTracing.mPathMaxBounces ) };
                        int limit{ static_cast<int>( settings.mRayTracing.mPathMaxSamples ) };
                        RendererControlRow( MKT_LOC( "renderer_samples_per_pixel" ).c_str() );
                        changed |= ImGui::SliderInt( "##renderer_samples_per_pixel",
                            &samples, 1, static_cast<int>( renderer::RayTracingConfiguration::kMaxPathSamplesPerFrame ) );
                        RendererControlRow( MKT_LOC( "renderer_maximum_bounces" ).c_str() );
                        changed |= ImGui::SliderInt( "##renderer_maximum_bounces", &bounces, 1, 32 );
                        RendererControlRow( MKT_LOC( "renderer_sample_limit" ).c_str() );
                        changed |= ImGui::InputInt( "##renderer_sample_limit", &limit );
                        settings.mRayTracing.mPathSamplesPerFrame = static_cast<core::u32>( glm::max( samples, 1 ) );
                        settings.mRayTracing.mPathMaxBounces = static_cast<core::u32>( glm::max( bounces, 1 ) );
                        settings.mRayTracing.mPathMaxSamples = static_cast<core::u32>( glm::max( limit, 1 ) );
                        RendererMetricRow( MKT_LOC( "renderer_accumulated_samples_label" ).c_str(),
                            string::Format( "{}", renderer.GetPathSampleCount() ).c_str() );

                        RendererControlRow( MKT_LOC( "renderer_accumulation" ).c_str() );
                        if ( ImGui::Button( imgui::widget::LocalizedLabel( "renderer_restart_accumulation", "Restart accumulation" ).c_str() ) ) {
                            renderer.ResetPathAccumulation();
                        }
                    }
                    else {
                        const auto* device{ renderer::RenderSystem::Get()->GetGpuDevice() };
                        const bool rasterSupported{ device->IsGraphicsApi( renderer::rhi::GraphicsAPI::eVulkan ) || device->IsGraphicsApi( renderer::rhi::GraphicsAPI::eD3D12 ) };
                        ImGui::BeginDisabled( !rasterSupported );
                        RendererControlRow( MKT_LOC( "renderer_ray_traced_shadows" ).c_str() );
                        changed |= imgui::CheckBox( "##renderer_ray_traced_shadows", settings.mRayTracing.mShadowsEnabled );
                        RendererControlRow( MKT_LOC( "renderer_ray_traced_reflections" ).c_str() );
                        changed |= imgui::CheckBox( "##renderer_ray_traced_reflections", settings.mRayTracing.mReflectionsEnabled );
                        RendererControlRow( MKT_LOC( "renderer_ray_traced_global_illumination" ).c_str() );
                        changed |= imgui::CheckBox( "##renderer_ray_traced_global_illumination", settings.mRayTracing.mGlobalIlluminationEnabled );

                        if ( settings.mRayTracing.mGlobalIlluminationEnabled ) {
                            int samples{ static_cast<int>( settings.mRayTracing.mRayGiSamplesPerFrame ) };
                            int bounces{ static_cast<int>( settings.mRayTracing.mRayGiMaxBounces ) };
                            RendererControlRow( MKT_LOC( "renderer_gi_samples_per_frame" ).c_str() );
                            changed |= ImGui::SliderInt( "##renderer_gi_samples_per_frame", &samples, 1, 8 );
                            RendererControlRow( MKT_LOC( "renderer_gi_indirect_bounces" ).c_str() );
                            changed |= ImGui::SliderInt( "##renderer_gi_indirect_bounces", &bounces, 1, 8 );
                            settings.mRayTracing.mRayGiSamplesPerFrame = static_cast<core::u32>( samples );
                            settings.mRayTracing.mRayGiMaxBounces = static_cast<core::u32>( bounces );
                            imgui::widget::ItemTooltip( "%s", MKT_LOC( "renderer_replaces_diffuse_environment_lighting_on_opaque_meshes_samples_accumulate_while" ).c_str() );
                        }

                        ImGui::EndDisabled();

                        if ( !rasterSupported ) {
                            ImGui::TextWrapped( MKT_LOC( "renderer_hybrid_effects_require_the_vulkan_or_d3d12_scene_renderer" ).c_str() );
                        }
                    }

                    ImGui::EndDisabled();

                    ImGui::EndTable();
                }

                if ( !supported ) {
                    ImGui::TextWrapped( MKT_LOC( "renderer_hardware_ray_tracing_is_unavailable_on_this_device_backend" ).c_str() );
                }

                if ( !renderer.GetRayTracingError().empty() ) {
                    ImGui::TextWrapped( "%s", renderer.GetRayTracingError().c_str() );
                }
            }

            if ( ImGui::CollapsingHeader( MKT_LOC( "renderer_shadows" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
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

            if ( auto* particles{ renderer::ParticleSystem::GetPtr() }; particles && ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_particles", "Particles" ).c_str() ) ) {
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

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "renderer_chromatic_aberration", "Chromatic aberration" ).c_str() ) ) {
                auto& chromatic{ settings.mChromaticAberration };
                ImGui::BeginDisabled( settings.mRayTracing.mPathTracingEnabled );
                if ( BeginRendererRows( "ChromaticAberrationSettings" ) ) {
                    RendererControlRow( MKT_LOC( "renderer_enabled" ).c_str() );
                    changed |= imgui::CheckBox( "##renderer_enabled_chromaticaberration", chromatic.mEnabled );
                    RendererControlRow( MKT_LOC( "renderer_defaults" ).c_str() );
                    if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "renderer_reset_chromaticaberration", "Reset##ChromaticAberration" ).c_str() ) ) {
                        const bool enabled{ chromatic.mEnabled };
                        chromatic = renderer::ChromaticAberrationSettings{};
                        chromatic.mEnabled = enabled;
                        changed = true;
                    }

                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "renderer_restore_the_original_three_pixel_diagonal_rgb_separation" ).c_str() );

                    ImGui::BeginDisabled( !chromatic.mEnabled );
                    RendererControlRow( MKT_LOC( "renderer_channel_offset_px" ).c_str() );
                    changed |= imgui::Slider( "##renderer_channel_offset_px", chromatic.mIntensity, core::float2{ 0.0f, 32.0f }, "%.2f" );
                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "renderer_pixel_step_between_red_green_and_blue_along_the_dominant" ).c_str() );
                    RendererControlRow( MKT_LOC( "renderer_direction" ).c_str() );
                    changed |= imgui::Slider( "##renderer_direction", chromatic.mDirection, core::float2{ -180.0f, 180.0f }, MKT_LOC( "renderer_deg" ).c_str() );
                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "renderer_45_degrees_original_diagonal_0_horizontal_90_vertical_offsets_are" ).c_str() );
                    ImGui::EndDisabled();

                    ImGui::EndTable();
                }

                ImGui::EndDisabled();
                ImGui::TextWrapped( "%s", MKT_LOC( "renderer_directional_rgb_separation_across_the_whole_image_based_on_the" ).c_str() );
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
        if ( ImGui::CollapsingHeader( MKT_LOC( "renderer_ambient_occlusion" ).c_str() ) ) {
            auto& renderer{ *mEditorState->mEditorRenderer };
            auto settings{ renderer.GetSettings() };
            bool changed{};

            if ( BeginRendererRows( "AmbientOcclusionSettings" ) ) {
                ImGui::PushID( "SSAO" );
                ImGui::BeginDisabled( settings.mRayTracing.mPathTracingEnabled );
                RendererControlRow( MKT_LOC( "renderer_ssao" ).c_str() );
                changed |= imgui::CheckBox( "##SSAO", settings.mSsaoEnabled );

                ImGui::BeginDisabled( !settings.mSsaoEnabled );
                RendererControlRow( MKT_LOC( "renderer_intensity" ).c_str() );
                changed |= imgui::Slider( "##renderer_intensity_ssao", settings.mSsaoIntensity, core::float2{ 0.0f, 4.0f } );
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::PopID();

                const bool supported{ renderer::RenderSystem::Get()->GetGpuDevice()->SupportsRayTracing() };
                ImGui::BeginDisabled( !supported );
                ImGui::BeginDisabled( settings.mRayTracing.mPathTracingEnabled );
                RendererControlRow( MKT_LOC( "renderer_ray_traced_ao" ).c_str() );
                changed |= imgui::CheckBox( "##renderer_ray_traced_ao",
                    settings.mRayTracing.mAmbientOcclusionEnabled );
                ImGui::EndDisabled();

                int samples{ core::as<int>( settings.mRayTracing.mAoSamplesPerFrame ) };
                RendererControlRow( MKT_LOC( "renderer_ao_samples" ).c_str() );
                changed |= ImGui::SliderInt( "##renderer_ao_samples", &samples, 1, 64 );
                settings.mRayTracing.mAoSamplesPerFrame = core::as<core::u32>( samples );
                RendererControlRow( MKT_LOC( "renderer_ao_distance" ).c_str() );
                changed |= ImGui::DragFloat( "##renderer_ao_distance",
                    &settings.mRayTracing.mAoMaxDistance, 0.01f, 0.0f, 10000.0f );
                RendererControlRow( MKT_LOC( "renderer_ao_cosine" ).c_str() );
                changed |= imgui::CheckBox( "##renderer_ao_cosine",
                    settings.mRayTracing.mUseCosineWeightedAoSampling );
                ImGui::TextWrapped( "%s", MKT_LOC( "renderer_ao_help" ).c_str() );
                if ( ImGui::Button( imgui::widget::LocalizedLabel( "renderer_ao_debug", "View ray-traced AO" ).c_str() ) ) {
                    mEditorState->mFinalComposition = renderer.GetFinalImage( renderer::FinalImageType::eRayTracedAmbientOcclusion );
                }
                ImGui::EndDisabled();

                ImGui::EndTable();
            }

            if ( changed ) {
                renderer.CancelPick();
                renderer.ApplySettings( settings );
            }
        }

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
            auto& bloom{ mEditorState->mEditorRenderer->GetBloomSettings() };
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
}
