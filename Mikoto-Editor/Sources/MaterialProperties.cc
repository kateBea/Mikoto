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

#include <cmath>
#include <imgui.h>

#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/AssetRegistryService.hh>

#include <Material/VolumeMaterial.hh>
#include <Material/SurfaceMaterial.hh>

#include <ImGui/ImGuiWidget.hh>

#include <Application/MaterialProperties.hh>

namespace mikoto::editor {
    using namespace mikoto::core;
    using namespace mikoto::material;

    static auto PresetCombo( const char* preview, const char* const* keys, u32 count ) -> i32 {
        i32 result{ -1 };
        if ( ImGui::BeginCombo( MKT_LOC( "material_preset" ).c_str(), preview ) ) {
            for ( u32 i{}; i < count; ++i ) {
                if ( ImGui::Selectable( MKT_LOC( keys[i] ).c_str() ) ) {
                    result = as<i32>( i );
                }
            }
            ImGui::EndCombo();
        }
        return result;
    }

    auto DrawVolumeMaterial( VolumeMaterial& material ) -> bool {
        auto settings{ material.GetSettings() };
        i32 model{ as<i32>( settings.mDensityModel ) };
        bool changed{ ImGui::Combo( MKT_LOC( "volume_density_model" ).c_str(), &model, MKT_LOC( "volume_density_models" ).c_str() ) };
        settings.mDensityModel = as<VolumeDensityModel>( model );
        changed |= ImGui::DragFloat( MKT_LOC( "volume_density" ).c_str(), &settings.mDensity, 0.01f, 0, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp );
        changed |= ImGui::DragFloat3( MKT_LOC( "volume_absorption" ).c_str(), &settings.mAbsorption.x, 0.001f, 0, 10000, "%.5f", ImGuiSliderFlags_AlwaysClamp );
        changed |= ImGui::DragFloat3( MKT_LOC( "volume_scattering" ).c_str(), &settings.mScattering.x, 0.001f, 0, 10000, "%.5f", ImGuiSliderFlags_AlwaysClamp );
        changed |= imgui::widget::CircleSlider( MKT_LOC( "volume_anisotropy" ), &settings.mAnisotropy, -0.99f, 0.99f, "%.3f" );

        if ( settings.mDensityModel == VolumeDensityModel::eExponentialHeight ) {
            changed |= ImGui::DragFloat( MKT_LOC( "volume_height_falloff" ).c_str(), &settings.mHeightFalloff, 0.001f, 0, 10000, "%.4f", ImGuiSliderFlags_AlwaysClamp );
            ImGui::TextWrapped( "%s", MKT_LOC( "volume_height_help" ).c_str() );
        }
        if ( settings.mDensityModel == VolumeDensityModel::eProceduralCloud ) {
            changed |= imgui::widget::CircleSlider( MKT_LOC( "volume_coverage" ), &settings.mCoverage, 0, 1 );
            changed |= ImGui::DragFloat( MKT_LOC( "volume_noise_scale" ).c_str(), &settings.mNoiseScale, 0.0001f, 0.000001f, 10000, "%.5f", ImGuiSliderFlags_AlwaysClamp );
            changed |= ImGui::DragFloat( MKT_LOC( "volume_erosion_scale" ).c_str(), &settings.mErosionScale, 0.0001f, 0.000001f, 10000, "%.5f", ImGuiSliderFlags_AlwaysClamp );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "volume_erosion" ), &settings.mErosion, 0, 1 );
            i32 octaves{ as<i32>( settings.mOctaves ) };
            if ( ImGui::SliderInt( MKT_LOC( "volume_octaves" ).c_str(), &octaves, 1, 8 ) ) {
                settings.mOctaves = as<u32>( octaves );
                changed = true;
            }
        }
        if ( settings.mDensityModel == VolumeDensityModel::eDensityGrid ) {
            if ( ImGui::Button( MKT_LOC( "volume_import_grid" ).c_str() ) ) {
                const auto path{ filesystem::OpenFileDialog( { { "Volume density grid", "mktvol" } } ) };
                if ( !path.IsEmpty() ) {
                    try {
                        material.ImportDensityGrid( path );
                        changed = true;
                    } catch ( const std::exception& error ) {
                        MKT_CORE_LOGGER_ERROR( "{}", error.what() );
                    }
                }
            }
            const auto dimensions{ material.GetGridDimensions() };
            ImGui::Text( "%s: %u x %u x %u", MKT_LOC( "volume_grid" ).c_str(), dimensions.x, dimensions.y, dimensions.z );
            ImGui::TextWrapped( "%s", MKT_LOC( "volume_grid_help" ).c_str() );
            if ( material.GetDensityGrid().empty() ) {
                ImGui::TextDisabled( "%s", MKT_LOC( "volume_empty_grid" ).c_str() );
            }
        }
        if ( changed ) {
            material.SetSettings( settings );
        }
        return changed;
    }

    auto DrawMaterialOptics( SurfaceMaterial& material ) -> bool {
        bool changed{};
        if ( ImGui::TreeNode("Height mapping") ) {
            auto height{material.GetHeightMapping()};
            int mode{static_cast<int>(height.mMode)};
            bool edited{ImGui::Combo("Method",&mode,"None\0Bump\0Parallax occlusion\0Displacement\0")};
            height.mMode = static_cast<HeightMappingMode>(mode);
            const auto path{material.GetTexturePath(MapType::eHeight)};
            ImGui::Button(path.IsEmpty() ? "Drop height texture here" : path.GetC_Str(),{ImGui::GetContentRegionAvail().x,0});
            if ( ImGui::BeginDragDropTarget() ) {
                if ( const auto* payload{ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE")} ) {
                    if ( payload->Data && payload->DataSize>1 && static_cast<const char*>(payload->Data)[payload->DataSize-1]==0 ) {
                        material.SetTextureReference(MapType::eHeight,filesystem::Path{static_cast<const char*>(payload->Data)});
                        changed = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if ( !path.IsEmpty() && ImGui::SmallButton("Clear height texture") ) { material.SetTextureReference(MapType::eHeight,{}); changed=true; }
            edited |= ImGui::DragFloat("Height scale",&height.mScale,0.001f,-1000,1000,"%.4f");
            edited |= ImGui::SliderFloat("Midlevel",&height.mMidlevel,0,1);
            int uv{static_cast<int>(height.mUVSet)};
            edited |= ImGui::Combo("Height UV set",&uv,"UV0\0UV1\0");
            height.mUVSet=static_cast<u32>(uv);
            if ( height.mMode==HeightMappingMode::eParallax ) {
                int steps{static_cast<int>(height.mSteps)};
                edited |= ImGui::SliderInt("Maximum steps",&steps,8,128);
                height.mSteps=static_cast<u32>(steps);
            }
            ImGui::TextWrapped("Height is linear grayscale. Displacement moves existing vertices and needs a sufficiently subdivided mesh; it does not alter physics colliders.");
            if ( edited ) { material.SetHeightMapping(height); changed=true; }
            ImGui::TreePop();
        }
        const auto model{ material.GetShadingModel() };
        if ( model == ShadingModel::eDielectric ) {
            auto settings{ material.GetDielectric() };
            static constexpr const char* keys[]{ "material_air", "material_water", "material_ice", "material_quartz", "material_glass", "material_sapphire", "material_diamond" };
            static constexpr f32 indices[]{ 1.0003f, 1.333f, 1.31f, 1.46f, 1.52f, 1.77f, 2.42f };
            const char* preview{ MKT_LOC( "material_custom" ).c_str() };
            for ( u32 i{}; i < 7; ++i ) {
                if ( std::abs( settings.mIndexOfRefraction - indices[i] ) < 1e-5f ) {
                    preview = MKT_LOC( keys[i] ).c_str();
                }
            }
            if ( const auto preset{ PresetCombo( preview, keys, 7 ) }; preset >= 0 ) {
                material.ApplyDielectricPreset( as<u32>( preset ) );
                settings = material.GetDielectric();
                changed = true;
            }

            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_ior" ), &settings.mIndexOfRefraction, 1.f, 2.5f, "%.4f" );
            auto roughness{ material.GetRoughnessFactor() };
            if ( imgui::widget::CircleSlider( MKT_LOC( "material_editor_roughness" ), &roughness, 0.f, 1.f ) ) {
                material.SetRoughnessFactor( roughness );
                changed = true;
            }
            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_transmission" ), &settings.mTransmission, 0.f, 1.f );
            if ( ImGui::CollapsingHeader( MKT_LOC( "material_optical_advanced" ).c_str() ) ) {
                changed |= imgui::CheckBox( MKT_LOC( "material_thin_walled" ).c_str(), settings.mThinWalled );
                ImGui::TextWrapped( "%s", MKT_LOC( "material_thin_walled_help" ).c_str() );
                ImGui::BeginDisabled( !settings.mThinWalled );
                changed |= imgui::widget::CircleSlider( MKT_LOC( "material_thickness" ), &settings.mThickness, 0.f, 100.f );
                ImGui::EndDisabled();
                changed |= imgui::widget::CircleSlider( MKT_LOC( "material_attenuation_distance" ), &settings.mAttenuationDistance, .001f, 1000.f );
                changed |= ImGui::ColorEdit3( MKT_LOC( "material_attenuation_color" ).c_str(), &settings.mAttenuationColor.x );
                auto absorption{ -glm::log( glm::max( settings.mAttenuationColor, float3{ 0.0001f } ) ) / settings.mAttenuationDistance };
                if ( ImGui::DragFloat3( MKT_LOC( "material_medium_absorption" ).c_str(), &absorption.x, .01f, 0.f, 9.21034f / settings.mAttenuationDistance, "%.4f" ) ) {
                    settings.mAttenuationColor = glm::exp( -absorption * settings.mAttenuationDistance );
                    changed = true;
                }
                ImGui::BeginDisabled( settings.mThinWalled );
                changed |= ImGui::DragFloat3( MKT_LOC( "material_medium_scattering" ).c_str(), &settings.mScattering.x, .1f, 0.f, 10000.f );
                changed |= imgui::widget::CircleSlider( MKT_LOC( "material_anisotropy" ), &settings.mPhaseAnisotropy, -.99f, .99f );
                ImGui::EndDisabled();
                ImGui::TextWrapped( "%s", MKT_LOC( "material_medium_help" ).c_str() );
            }
            if ( changed ) {
                material.SetDielectric( settings );
            }
        }
        else if ( model == ShadingModel::eConductor ) {
            static constexpr const char* keys[]{ "material_gold", "material_copper", "material_silver", "material_aluminum" };
            if ( const auto preset{ PresetCombo( MKT_LOC( "material_choose_preset" ).c_str(), keys, 4 ) }; preset >= 0 ) {
                material.ApplyConductorPreset( as<u32>( preset ) );
                changed = true;
            }
            auto settings{ material.GetConductor() };
            changed |= ImGui::DragFloat3( MKT_LOC( "material_complex_eta" ).c_str(), &settings.mEta.x, .01f, .0001f, 100.f );
            changed |= ImGui::DragFloat3( MKT_LOC( "material_complex_k" ).c_str(), &settings.mK.x, .01f, 0.f, 100.f );
            ImGui::TextWrapped( "%s", MKT_LOC( "material_conductor_help" ).c_str() );
            if ( changed ) {
                material.SetConductor( settings );
            }
        }
        else if ( model == ShadingModel::eSubsurfaceScattering ) {
            if ( !ImGui::CollapsingHeader( MKT_LOC( "material_bssrdf" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                return false;
            }
            static constexpr const char* keys[]{ "material_skin1", "material_skin2", "material_marble", "material_milk" };
            if ( const auto preset{ PresetCombo( MKT_LOC( "material_choose_preset" ).c_str(), keys, 4 ) }; preset >= 0 ) {
                material.ApplySubsurfacePreset( as<u32>( preset ) );
                changed = true;
            }
            auto settings{ material.GetSubsurface() };
            changed |= ImGui::DragFloat3( MKT_LOC( "material_sigma_a" ).c_str(), &settings.mAbsorption.x, .001f, 0.f, 10000.f, "%.4f" );
            changed |= ImGui::DragFloat3( MKT_LOC( "material_sigma_s" ).c_str(), &settings.mScattering.x, .01f, 0.f, 10000.f, "%.4f" );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_ior" ), &settings.mIndexOfRefraction, 1.f, 2.f, "%.3f" );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_anisotropy" ), &settings.mAnisotropy, -.99f, .99f );
            changed |= ImGui::DragFloat( MKT_LOC( "material_mm_units" ).c_str(), &settings.mUnitsPerMillimeter, .0001f, .000001f, 1000.f, "%.6f" );
            ImGui::TextWrapped( "%s", MKT_LOC( "material_bssrdf_help" ).c_str() );
            if ( changed ) {
                material.SetSubsurface( settings );
            }
        }
        return changed;
    }
}
