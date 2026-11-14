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

#include <exception>

#include <imgui.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/LocalizationService.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/AssetRegistryService.hh>

#include <Material/SkyboxMaterial.hh>

#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>

#include <Scene/Scene.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/LightingPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    LightingPanel::LightingPanel( const LightingPanelCreateInfo& info )
        : Panel{ "Lighting", "Panel", info.mGroup, info.mIsToggleable }, mEditorState{ info.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_LIGHTBULB, mPanelName );
    }

    auto LightingPanel::OnRender( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImVec2 initialSize{ ImVec2( 400.0f, 300.0f ) };
        ImGui::SetNextWindowSize( initialSize, ImGuiCond_FirstUseEver );

        imgui::BeginWindow( GetHeaderName().data(), MKT_ADDRESSOF( mPanelIsVisible ),
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        // Get the total width available in the current window
        core::f32 availableWidth{ ImGui::GetContentRegionAvail().x };

        core::i32 tabCount{ 3 };

        core::f32 itemSpacingX{ ImGui::GetStyle().ItemSpacing.x };
        core::f32 tabWidth{ ( availableWidth / tabCount ) - ( itemSpacingX * 0.5f ) };

        ImGui::PushStyleVar( ImGuiStyleVar_TabMinWidthBase, tabWidth );

        if ( ImGui::BeginTabBar( "LightingTabBar", ImGuiTabBarFlags_FittingPolicyResizeDown ) ) {
            if ( ImGui::BeginTabItem( MKT_LOC( "lighting_environment" ).c_str() ) ) {
                DrawEnvironment();
                ImGui::EndTabItem();
            }

            imgui::SetCursorHandOnLastItemHovered();

            if ( ImGui::BeginTabItem( MKT_LOC( "lighting_lights" ).c_str() ) ) {
                ImGui::EndTabItem();
            }

            imgui::SetCursorHandOnLastItemHovered();

            if ( ImGui::BeginTabItem( MKT_LOC( "lighting_scene" ).c_str() ) ) {
                ImGui::EndTabItem();
            }

            imgui::SetCursorHandOnLastItemHovered();

            ImGui::EndTabBar();
        }

        ImGui::PopStyleVar();
        ImGui::End();
    }

    auto LightingPanel::DrawEnvironment() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mEditorState->mActiveScene ) {
            return;
        }

        auto& scene{ *mEditorState->mActiveScene };
        ImGui::BeginDisabled( mEditorState->mRuntimeActive );
        const core::cstr choices[]{
            MKT_LOC( "lighting_environment_map_ibl" ).c_str(),
            MKT_LOC( "lighting_scene_lights_only" ).c_str(),
            MKT_LOC( "lighting_atmosphere_sky_ibl" ).c_str()
        };
        core::i32 source{ static_cast<core::i32>( scene.GetEnvironmentLightingSource() ) };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "lighting_lighting_source", "Lighting source" ).c_str(), &source, choices, 3 ) ) {
            scene.SetEnvironmentLightingSource( static_cast<scene::EnvironmentLightingSource>( source ) );
        }

        ImGui::TextWrapped( MKT_LOC( "lighting_environment_map_and_atmosphere_provide_ambient_lighting_and_reflections_scene" ).c_str() );

        if ( scene.GetEnvironmentLightingSource() == scene::EnvironmentLightingSource::eAtmosphere ) {
            DrawAtmosphere();
            ImGui::EndDisabled();
            return;
        }

        auto environment{ scene.GetEnvironmentMaterial().As<material::SkyboxMaterial>() };
        ImGui::SeparatorText( MKT_LOC( "lighting_environment_map" ).c_str() );

        if ( environment && environment->HasRequiredTextures() ) {
            if ( environment->IsType( material::SkyboxType::eEquirectangular ) ) {
                ImGui::TextWrapped( "%s", environment->GetEquirectangularAsset().GetRef().GetPath().GetC_Str() );
            }
            else {
                ImGui::TextUnformatted( MKT_LOC( "lighting_six_face_cubemap_edit_faces_in_the_skybox_inspector" ).c_str() );
            }
        }
        else {
            ImGui::TextWrapped( MKT_LOC( "lighting_no_complete_environment_map_choose_a_panorama_below_or_assign" ).c_str() );
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "lighting_choose_environment_map", "Choose environment map..." ).c_str() ) ) {
            try {
                const std::initializer_list<filesystem::FileDialogPair> filters{
                    { .mDescription = MKT_LOC( "lighting_environment_panorama" ).c_str(), .mPattern = "hdr,png,jpg,jpeg,ktx2" }
                };
                const filesystem::Path path{ filesystem::OpenFileDialog( filters ) };

                if ( !path.IsEmpty() ) {
                    const auto texture{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D ) };

                    if ( !texture.IsReady() ) {
                        mEnvironmentError = MKT_LOC( "lighting_could_not_load_the_environment_map" ).c_str();
                    }
                    else {
                        if ( !environment ) {
                            auto* entity{ scene.CreateEntity( "Environment" ) };
                            environment = entity->AddComponent<scene::SkyboxMaterialComponent>().GetMaterial().As<material::SkyboxMaterial>();
                        }

                        environment->SetType( material::SkyboxType::eEquirectangular );
                        environment->SetEquirectangular( texture );
                        mEnvironmentError.clear();
                    }
                }
            } catch ( const std::exception& error ) {
                mEnvironmentError = error.what();
            }
        }

        if ( environment ) {
            ImGui::BeginDisabled( source != static_cast<core::i32>( scene::EnvironmentLightingSource::eEnvironmentMap ) );
            core::f32 strength{ environment->GetAmbientScale() };

            if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_ibl_strength", "IBL strength" ).c_str(), &strength, 0.0f, 10.0f, "%.2f" ) ) {
                environment->SetAmbientScale( strength );
            }

            ImGui::EndDisabled();
        }

        if ( !mEnvironmentError.empty() ) {
            ImGui::TextWrapped( "%s", mEnvironmentError.c_str() );
        }

        ImGui::EndDisabled();
    }

    auto LightingPanel::DrawAtmosphere() -> void {
        auto& scene{ *mEditorState->mActiveScene };
        auto settings{ scene.GetAtmosphereSettings() };
        bool changed{};
        ImGui::SeparatorText( MKT_LOC( "lighting_atmosphere" ).c_str() );
        ImGui::TextWrapped( MKT_LOC( "lighting_physical_sky_atmospheric_haze_and_sky_reflections_set_the_camera" ).c_str() );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "lighting_reset_earth_atmosphere", "Reset Earth atmosphere" ).c_str() ) ) {
            settings = renderer::AtmosphereSettings{};
            changed = true;
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "lighting_sun", "Sun" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "lighting_use_scene_directional_light", "Use scene directional light" ).c_str(), settings.mUseDirectionalLight );

            if ( settings.mUseDirectionalLight ) {
                ImGui::TextWrapped( MKT_LOC( "lighting_uses_the_active_directional_light_with_the_lowest_entity_id" ).c_str() );
            }

            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_fallback_elevation", "Fallback elevation" ).c_str(), &settings.mSunElevation, -90.0f, 90.0f, MKT_LOC( "lighting_degrees" ).c_str() );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_fallback_azimuth", "Fallback azimuth" ).c_str(), &settings.mSunAzimuth, -180.0f, 180.0f, MKT_LOC( "lighting_degrees" ).c_str() );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_solar_illuminance_scale", "Solar illuminance scale" ).c_str(), &settings.mSunIlluminance, 0.0f, 50.0f, "%.2f" );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_sun_angular_radius", "Sun angular radius" ).c_str(), &settings.mSunAngularRadius, 0.01f, 2.0f, MKT_LOC( "lighting_degrees" ).c_str() );
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "lighting_scattering", "Scattering" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_rayleigh_density", "Rayleigh density" ).c_str(), &settings.mRayleighScale, 0.0f, 5.0f, "%.2f" );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_mie_density", "Mie density" ).c_str(), &settings.mMieScale, 0.0f, 5.0f, "%.2f" );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_mie_anisotropy", "Mie anisotropy" ).c_str(), &settings.mMieAnisotropy, -0.95f, 0.95f, "%.2f" );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_ozone_absorption", "Ozone absorption" ).c_str(), &settings.mOzoneScale, 0.0f, 5.0f, "%.2f" );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_ground_albedo", "Ground albedo" ).c_str(), &settings.mGroundAlbedo, 0.0f, 0.95f, "%.2f" );
            ImGui::TextWrapped( MKT_LOC( "lighting_rayleigh_controls_molecular_scattering_mie_adds_aerosol_haze_and_the" ).c_str() );
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "lighting_aerial_perspective", "Aerial perspective" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "lighting_distance_haze", "Distance haze" ).c_str(), settings.mAerialPerspective );
            ImGui::BeginDisabled( !settings.mAerialPerspective );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "lighting_haze_range", "Haze range" ).c_str(), &settings.mAerialDistance, 1.0f, 256.0f, "%.1f km" );
            ImGui::EndDisabled();
            ImGui::TextWrapped( MKT_LOC( "lighting_for_small_scenes_reduce_the_range_for_more_depth_precision" ).c_str() );
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "lighting_planet_and_scene_scale", "Planet and scene scale" ).c_str() ) ) {
            changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "lighting_planet_radius", "Planet radius" ).c_str(), &settings.mPlanetRadius, 10.0f, 100.0f, 10000.0f, "%.1f km" );
            changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "lighting_atmosphere_height", "Atmosphere height" ).c_str(), &settings.mAtmosphereHeight, 1.0f, 10.0f, 200.0f, "%.1f km" );
            changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "lighting_world_units_per_kilometer", "World units per kilometer" ).c_str(), &settings.mWorldUnitsPerKilometer, 10.0f, 1.0f, 1000000.0f, "%.1f" );
            changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "lighting_ground_y", "Ground Y" ).c_str(), &settings.mGroundHeight, 1.0f, -1000000.0f, 1000000.0f, "%.1f" );
            ImGui::TextWrapped( MKT_LOC( "lighting_1000_units_per_kilometer_means_one_world_unit_is_one" ).c_str() );
        }

        if ( changed ) {
            scene.SetAtmosphereSettings( settings );
        }
    }
}// namespace mikoto::editor
