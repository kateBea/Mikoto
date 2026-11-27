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

#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <sstream>

#include <imgui.h>

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <Application/MaterialProperties.hh>
#include <Application/PropertyWidgets.hh>

#include <Assets/AssetRegistryService.hh>

#include <Core/LocalizationService.hh>

#include <Filesystem/FileSystem.hh>

#include <ImGui/ImGuiWidget.hh>

#include <Logging/Logger.hh>

#include <Material/SurfaceMaterial.hh>
#include <Material/VolumeMaterial.hh>

#include <Renderer/Clouds/CloudDensityBaker.hh>

namespace mikoto::editor {
    using namespace mikoto::core;
    using namespace mikoto::material;

    auto DrawPostProcessBloom( material::PostProcessMaterial &material ) -> bool {
        if ( material.GetCategory() != PostProcessMaterialCategory::eBuiltIn ) {
            return false;
        }

        ImGui::SeparatorText( MKT_LOC( "renderer_bloom" ).c_str() );

        auto settings{ material.GetBloomSettings() };
        bool changed{ imgui::CheckBox(
            imgui::widget::LocalizedLabel( "post_process_bloom_enabled", "PostProcessBloom" ).c_str(), settings.mEnabled ) };

        ImGui::BeginDisabled( !settings.mEnabled );
        if ( ImGui::BeginTable( "##PostProcessBloomParameters", 2, ImGuiTableFlags_SizingStretchSame ) ) {
            const auto slider{ []( core::cstr key, core::f32 &value, core::float2 range ) -> bool {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( MKT_LOC( key ).c_str() );

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth( -1.0f );
                ImGui::PushID( key );
                const bool changed{ DrawFloatProperty( "##Value", &value, range, "%.2f", false ) };
                ImGui::PopID();

                return changed;
            } };

            changed |= slider( "renderer_threshold", settings.mThreshold, { 0.0f, 100.0f } );
            changed |= slider( "renderer_intensity", settings.mIntensity, { 0.0f, 10.0f } );
            changed |= slider( "renderer_scatter", settings.mScatter, { 0.0f, 1.0f } );
            changed |= slider( "renderer_radius", settings.mRadius, { 0.5f, 3.0f } );
            ImGui::EndTable();
        }
        ImGui::EndDisabled();

        if ( changed ) {
            material.SetBloomSettings( settings );
        }

        return changed;
    }

    auto DrawPostProcessChromaticAberration( material::PostProcessMaterial &material ) -> bool {
        if ( material.GetCategory() != PostProcessMaterialCategory::eBuiltIn ) {
            return false;
        }

        ImGui::SeparatorText( MKT_LOC( "renderer_chromatic_aberration" ).c_str() );
        ImGui::PushID( "PostProcessChromaticAberration" );

        auto settings{ material.GetChromaticAberrationSettings() };
        bool changed{ imgui::CheckBox(
            imgui::widget::LocalizedLabel( "renderer_enabled", "ChromaticAberrationEnabled" ).c_str(), settings.mEnabled ) };

        ImGui::SameLine();
        if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "renderer_defaults", "ChromaticAberrationDefaults" ).c_str() ) ) {
            settings = renderer::ChromaticAberrationSettings{ .mEnabled = settings.mEnabled };
            changed = true;
        }

        using renderer::ChromaticAberrationSettings;
        ImGui::BeginDisabled( !settings.mEnabled );

        if ( ImGui::BeginTable( "##Parameters", 2, ImGuiTableFlags_SizingStretchSame ) ) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "renderer_channel_offset_px" ).c_str() );
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth( -1.0f );
            changed |= DrawFloatProperty( "##Offset", &settings.mIntensity, { 0.0f, ChromaticAberrationSettings::kMaxIntensity }, "%.2f", false );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "renderer_direction" ).c_str() );
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth( -1.0f );
            changed |= DrawFloatProperty( "##Direction", &settings.mDirection, { -ChromaticAberrationSettings::kMaxDirection, ChromaticAberrationSettings::kMaxDirection }, MKT_LOC( "renderer_deg" ).c_str(), false );

            ImGui::EndTable();
        }

        ImGui::EndDisabled();
        ImGui::PopID();

        if ( changed ) {
            material.SetChromaticAberrationSettings( settings );
        }

        return changed;
    }

    MKT_NODISCARD static auto PresetCombo( const char *preview, const char *const *keys, u32 count ) -> i32 {
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

    static auto ReadCloudGeometry( const renderer::CloudAuthoringLayer &layer ) -> renderer::CloudLayerGrid {
        const filesystem::Path path{ layer.mAssetPath };
        eastl::vector<core::float3> positions{};
        eastl::vector<core::u32> indices{};
        const bool points{ layer.mSource == renderer::CloudLayerSource::ePointCloud };
        if ( points && path.IsExtension( ".xyz" ) ) {
            std::ifstream file{ path.GetPathTyped<std::filesystem::path>() };
            if ( !file ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open cloud point source." );
            }
            std::string line{};
            while ( std::getline( file, line ) ) {
                std::istringstream values{ line };
                values >> std::ws;
                if ( values.eof() || values.peek() == '#' ) {
                    continue;
                }
                core::float3 p{};
                if ( !( values >> p.x >> p.y >> p.z ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Point source requires one XYZ point per line." );
                }
                positions.push_back( p );
                if ( positions.size() > 2000000 ) {
                    MKT_THROW_RUNTIME_ERROR( "Cloud point source exceeds the two-million-point budget." );
                }
            }
        }
        else {
            // Import bind-pose geometry on the authoring worker, without assets,
            // scene registration, GPU allocations or material/texture loading.
            Assimp::Importer importer{};
            const auto *scene{ importer.ReadFile( path.GetC_Str(), aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_JoinIdenticalVertices ) };
            if ( !scene ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot read cloud mesh/point source." );
            }
            for ( core::u32 meshIndex{}; meshIndex < scene->mNumMeshes; ++meshIndex ) {
                const auto &mesh{ *scene->mMeshes[meshIndex] };
                if ( positions.size() + mesh.mNumVertices > 2000000 ) {
                    MKT_THROW_RUNTIME_ERROR( "Cloud geometry exceeds the two-million-vertex budget." );
                }
                const core::u32 first{ static_cast<core::u32>( positions.size() ) };
                for ( core::u32 i{}; i < mesh.mNumVertices; ++i ) {
                    positions.push_back( { mesh.mVertices[i].x, mesh.mVertices[i].y, mesh.mVertices[i].z } );
                }
                if ( points ) {
                    continue;
                }
                for ( core::u32 i{}; i < mesh.mNumFaces; ++i ) {
                    const auto &face{ mesh.mFaces[i] };
                    if ( face.mNumIndices != 3 ) {
                        continue;
                    }
                    if ( indices.size() + 3 > 6000000 ) {
                        MKT_THROW_RUNTIME_ERROR( "Cloud geometry exceeds the triangle budget." );
                    }
                    for ( core::u32 corner{}; corner < 3; ++corner ) {
                        indices.push_back( first + face.mIndices[corner] );
                    }
                }
            }
        }
        if ( !points && indices.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "Mesh cloud sources require closed triangle geometry." );
        }
        return renderer::CloudDensityBaker::Voxelize( positions, indices, core::uint3{ layer.mFluid.mResolution },
            layer.mPointRadius, layer.mFluid.mDetailType, layer.mFluid.mProfileDepth );
    }

    static auto DrawCloudImportOptions( renderer::CloudVolumeImportSettings &settings,
        const eastl::vector<eastl::string> &grids, bool slices ) -> bool {
        bool changed{};
        i32 resolution{ as<i32>( settings.mMaxResolution ) };
        if ( imgui::widget::CircleInt( "Maximum import axis", &resolution, 16, 256 ) ) {
            settings.mMaxResolution = as<u32>( resolution );
            changed = true;
        }
        if ( slices ) {
            i32 layout{ as<i32>( settings.mSliceLayout ) };
            if ( ImGui::Combo( "Slice channels", &layout, "Profile / type / density (RGB)\0Distance / profile / type / density (RGBA)\0" ) ) {
                settings.mSliceLayout = as<renderer::CloudSliceLayout>( layout );
                changed = true;
            }
        }
        else {
            const auto channel{ [&]( const char *label, eastl::string &selection ) {
                if ( ImGui::BeginCombo( label, selection.empty() ? "Automatic" : selection.c_str() ) ) {
                    if ( ImGui::Selectable( "Automatic", selection.empty() ) ) {
                        selection.clear();
                        changed = true;
                    }
                    for ( const auto &name: grids ) {
                        if ( ImGui::Selectable( name.c_str(), selection == name ) ) {
                            selection = name;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            } };
            channel( "Profile grid", settings.mProfileGrid );
            channel( "Detail grid", settings.mDetailGrid );
            channel( "Density grid", settings.mDensityGrid );
            changed |= DrawFloatProperty( "Imported detail default", &settings.mDetailType, { 0, 1 }, "%.2f", true );
            changed |= DrawFloatProperty( "Imported profile depth", &settings.mProfileDepth, { .001f, 1 }, "%.3f", true );
        }
        return changed;
    }

    auto DrawVolumeMaterial( VolumeMaterial &material ) -> bool {
        auto settings{ material.GetSettings() };
        i32 model{ as<i32>( settings.mDensityModel ) };
        bool changed{ ImGui::Combo( MKT_LOC( "volume_density_model" ).c_str(), &model, MKT_LOC( "volume_density_models" ).c_str() ) };
        settings.mDensityModel = as<VolumeDensityModel>( model );
        changed |= DrawFloatProperty( MKT_LOC( "volume_density" ).c_str(), &settings.mDensity, { 0, 10000 }, "%.3f", true );
        changed |= DrawFloatVectorProperty( MKT_LOC( "volume_absorption" ).c_str(), &settings.mAbsorption.x, 3, { 0, 10000 }, "%.5f", true );
        changed |= DrawFloatVectorProperty( MKT_LOC( "volume_scattering" ).c_str(), &settings.mScattering.x, 3, { 0, 10000 }, "%.5f", true );
        changed |= DrawFloatProperty( MKT_LOC( "volume_anisotropy" ), &settings.mAnisotropy, { -0.99f, 0.99f }, "%.3f", true );

        if ( settings.mDensityModel == VolumeDensityModel::eExponentialHeight ) {
            changed |= DrawFloatProperty( MKT_LOC( "volume_height_falloff" ).c_str(), &settings.mHeightFalloff, { 0, 10000 }, "%.4f", true );
            ImGui::TextWrapped( "%s", MKT_LOC( "volume_height_help" ).c_str() );
        }
        if ( settings.mDensityModel == VolumeDensityModel::eProceduralCloud ) {
            i32 form{ as<i32>( settings.mCloudForm ) };
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "cloud_form" ).c_str() );
            ImGui::SameLine();
            ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
            if ( ImGui::Combo( "##CloudForm", &form, MKT_LOC( "cloud_form_options" ).c_str() ) ) {
                settings.mCloudForm = as<renderer::CloudForm>( form );
                changed = true;
            }
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( "Pattern seed" );
            ImGui::SameLine();
            ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
            changed |= ImGui::InputScalar( "##CloudPatternSeed", ImGuiDataType_U32, &settings.mCloudSeed );
            if ( ImGui::IsItemHovered() ) {
                ImGui::SetTooltip( "Use the same nonzero seed to reproduce a cloud pattern. Zero uses the entity seed." );
            }
            i32 formations{ as<i32>( settings.mCloudFormationCount ) };
            if ( imgui::widget::CircleInt( "Cloud formations", &formations, 1, 64 ) ) {
                settings.mCloudFormationCount = as<u32>( formations );
                changed = true;
            }
            changed |= DrawFloatProperty( "Formation scale", &settings.mCloudFormationScale, { .2f, 3 }, "%.2f", true );
            changed |= DrawFloatProperty( "Formation variation", &settings.mCloudFormationVariation, { 0, 1 }, "%.2f", true );
            changed |= DrawFloatProperty( MKT_LOC( "cloud_profile_depth" ), &settings.mCloudProfileDepth, { .001f, 1.0f }, "%.3f", true );
            changed |= DrawFloatProperty( MKT_LOC( "volume_coverage" ), &settings.mCoverage, { 0, 1 }, "%.3f", true );
            changed |= DrawFloatProperty( MKT_LOC( "cloud_weather_scale" ), &settings.mWeatherScale, { .000001f, .01f }, "%.6f /m", true );
            changed |= DrawFloatProperty( MKT_LOC( "cloud_coverage_variation" ), &settings.mCoverageVariation, { 0, 1 }, "%.2f", true );
            changed |= DrawFloatProperty( MKT_LOC( "cloud_type_variation" ), &settings.mCloudTypeVariation, { 0, 1 }, "%.2f", true );
            changed |= DrawFloatProperty( MKT_LOC( "cloud_height_variation" ), &settings.mCloudHeightVariation, { 0, 1 }, "%.2f", true );
            changed |= DrawFloatProperty( MKT_LOC( "cloud_wind_shear" ), &settings.mWindShear, { 0, 2000 }, "%.1f m", true );
            changed |= DrawFloatProperty( MKT_LOC( "volume_noise_scale" ).c_str(), &settings.mNoiseScale, { 0.000001f, 10000 }, "%.5f", true );
            changed |= DrawFloatProperty( MKT_LOC( "volume_erosion_scale" ).c_str(), &settings.mErosionScale, { 0.000001f, 10000 }, "%.5f", true );
            changed |= DrawFloatProperty( MKT_LOC( "volume_erosion" ), &settings.mErosion, { 0, 1 }, "%.3f", true );
            i32 octaves{ as<i32>( settings.mOctaves ) };
            if ( ImGui::SliderInt( MKT_LOC( "volume_octaves" ).c_str(), &octaves, 1, 8 ) ) {
                settings.mOctaves = as<u32>( octaves );
                changed = true;
            }
        }
        if ( settings.mDensityModel == VolumeDensityModel::eProceduralCloud || settings.mDensityModel == VolumeDensityModel::eDensityGrid ) {
            if ( material.GetCloudGrid().empty() || settings.mDensityModel == VolumeDensityModel::eProceduralCloud ) {
                changed |= DrawFloatProperty( "Wispy / billowy", &settings.mCloudDetailType, { 0, 1 }, "%.2f", true );
            }
            if ( settings.mDensityModel == VolumeDensityModel::eDensityGrid ) {
                changed |= DrawFloatProperty( "Detail frequency", &settings.mErosionScale, { .00001f, 1.0f }, "%.5f", true );
                changed |= DrawFloatProperty( "Detail erosion", &settings.mErosion, { 0, 1 }, "%.3f", true );
            }
        }
        if ( settings.mDensityModel == VolumeDensityModel::eDensityGrid ) {
            static std::future<void> volumeExport{};
            if ( volumeExport.valid() && volumeExport.wait_for( std::chrono::seconds{ 0 } ) == std::future_status::ready ) {
                try {
                    volumeExport.get();
                    MKT_CORE_LOGGER_INFO( "Cloud VDB export completed." );
                } catch ( const std::exception &error ) {
                    MKT_CORE_LOGGER_ERROR( "Cloud export: {}", error.what() );
                }
            }
            static std::future<renderer::CloudImportedVolume> volumeImport{};
            static core::Ref<VolumeMaterial> importTarget{};
            static filesystem::Path importPath{};
            static renderer::CloudVolumeImportSettings importSettings{};
            static eastl::vector<eastl::string> gridNames{};
            if ( volumeImport.valid() && importTarget.GetPtr() == &material &&
                 volumeImport.wait_for( std::chrono::seconds{ 0 } ) == std::future_status::ready ) {
                try {
                    auto imported{ volumeImport.get() };
                    material.SetCloudGrid( imported.mGrid.mDimensions, imported.mGrid.mVoxels );
                    settings.mDensity = imported.mDensityMultiplier;
                    settings.mDensityModel = VolumeDensityModel::eDensityGrid;
                    changed = true;
                    MKT_CORE_LOGGER_INFO( "Imported cloud domain: {} x {} x {} world units.",
                        imported.mWorldMaximum.x - imported.mWorldMinimum.x, imported.mWorldMaximum.y - imported.mWorldMinimum.y,
                        imported.mWorldMaximum.z - imported.mWorldMinimum.z );
                } catch ( const std::exception &error ) {
                    MKT_CORE_LOGGER_ERROR( "Cloud import: {}", error.what() );
                }
                importTarget.Reset();
            }
            ImGui::BeginDisabled( volumeImport.valid() );
            if ( ImGui::Button( MKT_LOC( "volume_import_grid" ).c_str() ) ) {
                const auto path{ filesystem::OpenFileDialog( { { "Cloud volume / TGA slice", "vdb;tga;mktvol" } } ) };
                if ( !path.IsEmpty() ) {
                    importPath = path;
                    gridNames.clear();
                    if ( path.IsExtension( ".vdb" ) ) {
                        try {
                            gridNames = renderer::CloudVolumeIO::ListVdbGrids( path );
                        } catch ( const std::exception &error ) {
                            MKT_CORE_LOGGER_ERROR( "{}", error.what() );
                        }
                    }
                }
            }
            if ( !importPath.IsEmpty() ) {
                ImGui::TextWrapped( "%s", importPath.GetC_Str() );
                DrawCloudImportOptions( importSettings, gridNames, importPath.IsExtension( ".tga" ) );
                if ( ImGui::Button( "Import selected volume" ) ) {
                    importTarget = core::Ref<VolumeMaterial>{ &material };
                    volumeImport = std::async( std::launch::async, [path = importPath, options = importSettings]() {
                        if ( path.IsExtension( ".vdb" ) ) {
                            return renderer::CloudVolumeIO::ImportVdb( path, options );
                        }
                        if ( path.IsExtension( ".tga" ) ) {
                            return renderer::CloudVolumeIO::ImportSlices( path, options );
                        }
                        auto source{ core::Ref<VolumeMaterial>::New() };
                        source->ImportDensityGrid( path );
                        renderer::CloudImportedVolume result{};
                        result.mDensityMultiplier = source->GetSettings().mDensity;
                        result.mGrid.mDimensions = source->GetGridDimensions();
                        result.mGrid.mVoxels = source->GetCloudGrid().empty() ? renderer::CloudDensityBaker::BuildVolume(
                                                                                    source->GetGridDimensions(), source->GetDensityGrid() )
                                                                              : source->GetCloudGrid();
                        return result;
                    } );
                }
            }
            ImGui::EndDisabled();
            if ( volumeImport.valid() ) {
                ImGui::TextDisabled( "Volume import pending..." );
            }
            ImGui::BeginDisabled( volumeExport.valid() );
            if ( !material.GetCloudGrid().empty() && ImGui::Button( "Export VDB..." ) ) {
                const auto path{ filesystem::SaveFileDialog( "cloud.vdb", { { "OpenVDB volume", "vdb" } } ) };
                if ( !path.IsEmpty() ) {
                    try {
                        renderer::CloudImportedVolume output{};
                        output.mGrid = { material.GetGridDimensions(), material.GetCloudGrid() };
                        output.mWorldMaximum = material.GetCloudAuthoring().mDomainAspect;
                        output.mDensityMultiplier = settings.mDensity;
                        volumeExport = std::async( std::launch::async, [path, snapshot = eastl::move( output )]() {
                            renderer::CloudVolumeIO::ExportVdb( path, snapshot );
                        } );
                    } catch ( const std::exception &error ) {
                        MKT_CORE_LOGGER_ERROR( "Cloud export: {}", error.what() );
                    }
                }
            }
            ImGui::EndDisabled();
            if ( volumeExport.valid() ) {
                ImGui::TextDisabled( "Volume export pending..." );
            }
            const auto dimensions{ material.GetGridDimensions() };
            ImGui::Text( "%s: %u x %u x %u", MKT_LOC( "volume_grid" ).c_str(), dimensions.x, dimensions.y, dimensions.z );
            ImGui::TextWrapped( "%s", MKT_LOC( material.GetCloudGrid().empty() ? "volume_grid_help" : "cloud_grid_help" ).c_str() );
            if ( material.GetDensityGrid().empty() ) {
                ImGui::TextDisabled( "%s", MKT_LOC( "volume_empty_grid" ).c_str() );
            }
        }
        static std::future<renderer::CloudDensityBake> cloudBake{};
        static core::Ref<VolumeMaterial> cloudBakeTarget{};
        if ( cloudBake.valid() && cloudBakeTarget.GetPtr() == &material &&
             cloudBake.wait_for( std::chrono::seconds{ 0 } ) == std::future_status::ready ) {
            try {
                auto result{ cloudBake.get() };
                material.SetCloudGrid( result.mDimensions, result.mVoxels );
                settings.mDensityModel = VolumeDensityModel::eDensityGrid;
                changed = true;
            } catch ( const std::exception &error ) {
                MKT_CORE_LOGGER_ERROR( "Cloud authoring: {}", error.what() );
            }
            cloudBakeTarget.Reset();
        }

        if ( ImGui::CollapsingHeader( "Cloud authoring" ) ) {
            ImGui::PushID( &material );
            auto recipe{ material.GetCloudAuthoring() };
            bool recipeChanged{};
            static i32 selection{};
            ImGui::TextWrapped( "Grow fluid shapes, assemble transformed layers, cut holes and bake a cloudscape. Save the material to retain the recipe and baked fields." );
            ImGui::BeginDisabled( cloudBake.valid() );
            if ( ImGui::Button( "Cumulus sky recipe" ) ) {
                recipe = renderer::CloudDensityBaker::CreateCumulusSkyRecipe();
                recipeChanged = true;
                selection = 0;
            }
            auto size{ recipe.mDimensions == core::uint3{} ? core::uint3{ recipe.mResolution } : recipe.mDimensions };
            const char *axes[]{ "Output voxels X", "Output voxels Y", "Output voxels Z" };
            for ( core::u32 axis{}; axis < 3; ++axis ) {
                i32 value{ as<i32>( size[axis] ) };
                if ( imgui::widget::CircleInt( axes[axis], &value, 16, 256 ) ) {
                    size[axis] = as<u32>( value );
                    recipe.mDimensions = size;
                    recipeChanged = true;
                }
            }
            recipeChanged |= DrawFloatVectorProperty( "Domain proportions", &recipe.mDomainAspect.x, 3, { .001f, 1000 }, "%.3f", true );
            recipeChanged |= DrawFloatProperty( "Profile depth", &recipe.mProfileDepth, { .001f, 1 }, "%.3f", true );
            ImGui::BeginDisabled( recipe.mLayers.size() >= 32 );
            if ( ImGui::Button( "Add layer" ) ) {
                recipe.mLayers.push_back( {} );
                selection = as<i32>( recipe.mLayers.size() ) - 1;
                recipeChanged = true;
            }
            ImGui::EndDisabled();
            if ( !recipe.mLayers.empty() ) {
                selection = glm::clamp( selection, 0, as<i32>( recipe.mLayers.size() ) - 1 );
                for ( i32 index{}; index < as<i32>( recipe.mLayers.size() ); ++index ) {
                    ImGui::PushID( index );
                    const auto label{ string::Format( "Layer {}{}", index + 1, recipe.mLayers[index].mEnabled ? "" : " (disabled)" ) };
                    if ( ImGui::Selectable( label.c_str(), selection == index ) ) {
                        selection = index;
                    }
                    ImGui::PopID();
                }
                ImGui::BeginDisabled( selection == 0 );
                if ( ImGui::Button( "Move up" ) ) {
                    eastl::swap( recipe.mLayers[selection], recipe.mLayers[selection - 1] );
                    --selection;
                    recipeChanged = true;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled( selection + 1 >= as<i32>( recipe.mLayers.size() ) );
                if ( ImGui::Button( "Move down" ) ) {
                    eastl::swap( recipe.mLayers[selection], recipe.mLayers[selection + 1] );
                    ++selection;
                    recipeChanged = true;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if ( ImGui::Button( "Remove" ) ) {
                    recipe.mLayers.erase( recipe.mLayers.begin() + selection );
                    selection = glm::max( selection - 1, 0 );
                    recipeChanged = true;
                }
            }
            if ( !recipe.mLayers.empty() ) {
                auto &layer{ recipe.mLayers[selection] };
                recipeChanged |= imgui::CheckBox( "Layer enabled", layer.mEnabled );
                i32 source{ as<i32>( layer.mSource ) }, operation{ as<i32>( layer.mOperation ) };
                if ( ImGui::Combo( "Source", &source, "Fluid simulation\0Ellipsoid\0Box\0Imported grid\0Closed mesh\0Point cloud\0" ) ) {
                    layer.mSource = as<renderer::CloudLayerSource>( source );
                    recipeChanged = true;
                }
                if ( ImGui::Combo( "Operation", &operation, "Union\0Subtract\0Intersect\0Replace\0" ) ) {
                    layer.mOperation = as<renderer::CloudLayerOperation>( operation );
                    recipeChanged = true;
                }
                recipeChanged |= DrawFloatVectorProperty( "Offset", &layer.mOffset.x, 3, { -2, 2 }, "%.3f", true );
                recipeChanged |= DrawFloatVectorProperty( "Scale", &layer.mScale.x, 3, { .01f, 4 }, "%.3f", true );
                recipeChanged |= DrawFloatVectorProperty( "Rotation", &layer.mRotation.x, 3, { -180, 180 }, "%.1f", true );
                recipeChanged |= DrawFloatProperty( "Opacity", &layer.mOpacity, { 0, 1 }, "%.3f", true );
                recipeChanged |= DrawFloatProperty( "Erosion / cutout", &layer.mErosion, { 0, 1 }, "%.3f", true );
                recipeChanged |= DrawFloatProperty( "Edge softness", &layer.mSoftness, { .001f, 1 }, "%.3f", true );
                if ( layer.mSource == renderer::CloudLayerSource::eGrid || layer.mSource == renderer::CloudLayerSource::eMesh ||
                     layer.mSource == renderer::CloudLayerSource::ePointCloud ) {
                    const char *hint{ layer.mSource == renderer::CloudLayerSource::eMesh ? "Choose a closed, watertight mesh." : layer.mSource == renderer::CloudLayerSource::ePointCloud ? "Choose a point cloud (.xyz or mesh vertices)."
                                                                                                                                                                                          : "Choose a VDB, numbered TGA slice, .mktvol grid or baked volume material." };
                    ImGui::TextWrapped( "%s", layer.mAssetPath.empty() ? hint : layer.mAssetPath.c_str() );
                    if ( ImGui::Button( "Choose cloud source..." ) ) {
                        const auto path{ layer.mSource == renderer::CloudLayerSource::eGrid ? filesystem::OpenFileDialog( { { "Cloud grid / material", "vdb;tga;mktvol;mktmtl" } } ) : filesystem::OpenFileDialog( { { "Geometry / points", "gltf;glb;fbx;obj;ply;xyz" } } ) };
                        if ( !path.IsEmpty() ) {
                            layer.mAssetPath = path.ToAbsolute().GetC_Str();
                            recipeChanged = true;
                        }
                    }
                }
                if ( layer.mSource == renderer::CloudLayerSource::eGrid && !layer.mAssetPath.empty() ) {
                    const filesystem::Path path{ layer.mAssetPath };
                    static eastl::string inspectedPath{};
                    static eastl::vector<eastl::string> inspectedGrids{};
                    if ( path.IsExtension( ".vdb" ) && inspectedPath != layer.mAssetPath ) {
                        inspectedPath = layer.mAssetPath;
                        inspectedGrids.clear();
                        try {
                            inspectedGrids = renderer::CloudVolumeIO::ListVdbGrids( path );
                        } catch ( const std::exception &error ) {
                            MKT_CORE_LOGGER_ERROR( "{}", error.what() );
                        }
                    }
                    if ( path.IsExtension( ".vdb" ) || path.IsExtension( ".tga" ) ) {
                        recipeChanged |= DrawCloudImportOptions( layer.mImport, inspectedGrids, path.IsExtension( ".tga" ) );
                    }
                }
                if ( layer.mSource == renderer::CloudLayerSource::eMesh || layer.mSource == renderer::CloudLayerSource::ePointCloud ) {
                    i32 resolution{ as<i32>( layer.mFluid.mResolution ) };
                    if ( imgui::widget::CircleInt( "Source voxels", &resolution, 16, 256 ) ) {
                        layer.mFluid.mResolution = as<u32>( resolution );
                        recipeChanged = true;
                    }
                    if ( layer.mSource == renderer::CloudLayerSource::ePointCloud ) {
                        recipeChanged |= DrawFloatProperty( "Point radius", &layer.mPointRadius, { .001f, .5f }, "%.3f", true );
                    }
                }
                auto &fluid{ layer.mFluid };
                if ( layer.mSource != renderer::CloudLayerSource::eGrid ) {
                    recipeChanged |= DrawFloatProperty( "Wispy / billowy", &fluid.mDetailType, { 0, 1 }, "%.2f", true );
                }
                if ( layer.mSource == renderer::CloudLayerSource::eFluid ) {
                    i32 form{ as<i32>( fluid.mCloudForm ) };
                    if ( ImGui::Combo( "Cloud form", &form, MKT_LOC( "cloud_form_options" ).c_str() ) ) {
                        fluid.mCloudForm = as<renderer::CloudForm>( form );
                        recipeChanged = true;
                    }
                    i32 cells{ as<i32>( fluid.mResolution ) }, steps{ as<i32>( fluid.mSteps ) }, pressure{ as<i32>( fluid.mPressureIterations ) };
                    if ( imgui::widget::CircleInt( "Fluid resolution", &cells, 16, 96 ) ) {
                        fluid.mResolution = as<u32>( cells );
                        recipeChanged = true;
                    }
                    if ( imgui::widget::CircleInt( "Bake steps", &steps, 1, 128 ) ) {
                        fluid.mSteps = as<u32>( steps );
                        recipeChanged = true;
                    }
                    if ( imgui::widget::CircleInt( "Pressure iterations", &pressure, 8, 128 ) ) {
                        fluid.mPressureIterations = as<u32>( pressure );
                        recipeChanged = true;
                    }
                    recipeChanged |= ImGui::InputScalar( "Bake seed", ImGuiDataType_U32, &fluid.mSeed );
                    recipeChanged |= DrawFloatProperty( "Buoyancy", &fluid.mBuoyancy, { 0, 4 }, "%.2f", true );
                    recipeChanged |= DrawFloatProperty( "Vorticity", &fluid.mVorticity, { 0, 4 }, "%.2f", true );
                    recipeChanged |= DrawFloatProperty( "Humidity", &fluid.mHumidity, { 0, 1 }, "%.2f", true );
                    recipeChanged |= DrawFloatProperty( "Cooling", &fluid.mCooling, { 0, 4 }, "%.2f", true );
                    recipeChanged |= DrawFloatProperty( "Condensation", &fluid.mCondensation, { 0, 4 }, "%.2f", true );
                    recipeChanged |= DrawFloatProperty( "Evaporation", &fluid.mEvaporation, { 0, 4 }, "%.2f", true );
                    recipeChanged |= DrawFloatProperty( "Wind shear", &fluid.mWindShear, { -8, 8 }, "%.2f", true );
                }
            }
            if ( recipeChanged ) {
                try {
                    material.SetCloudAuthoring( recipe );
                    changed = true;
                } catch ( const std::exception &error ) {
                    MKT_CORE_LOGGER_ERROR( "Cloud recipe: {}", error.what() );
                    recipe = material.GetCloudAuthoring();
                }
            }
            ImGui::BeginDisabled( recipe.mLayers.empty() );
            if ( ImGui::Button( "Bake cloudscape" ) ) {
                cloudBakeTarget = core::Ref<VolumeMaterial>{ &material };
                cloudBake = std::async( std::launch::async, [recipe = eastl::move( recipe )]() {
                    eastl::vector<renderer::CloudLayerGrid> grids( recipe.mLayers.size() );
                    for ( usize index{}; index < recipe.mLayers.size(); ++index ) {
                        const auto &layer{ recipe.mLayers[index] };
                        if ( !layer.mEnabled ) {
                            continue;
                        }
                        if ( layer.mSource == renderer::CloudLayerSource::eMesh || layer.mSource == renderer::CloudLayerSource::ePointCloud ) {
                            grids[index] = ReadCloudGeometry( layer );
                            continue;
                        }
                        if ( layer.mSource != renderer::CloudLayerSource::eGrid ) {
                            continue;
                        }
                        auto source{ core::Ref<VolumeMaterial>::New() };
                        const filesystem::Path path{ layer.mAssetPath };
                        if ( path.IsExtension( ".mktvol" ) || path.IsExtension( ".vdb" ) || path.IsExtension( ".tga" ) ) {
                            source->ImportDensityGrid( path, layer.mImport );
                        }
                        else {
                            source->Deserialize( path );
                        }
                        grids[index].mDimensions = source->GetGridDimensions();
                        grids[index].mVoxels = source->GetCloudGrid().empty() ? renderer::CloudDensityBaker::BuildVolume( source->GetGridDimensions(), source->GetDensityGrid(), layer.mFluid.mDetailType ) : source->GetCloudGrid();
                    }
                    return renderer::CloudDensityBaker::Compose( recipe, grids );
                } );
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            if ( cloudBake.valid() ) {
                ImGui::TextDisabled( "Bake pending: %s", cloudBakeTarget->GetName().c_str() );
            }
            ImGui::PopID();
        }

        if ( changed ) {
            material.SetSettings( settings );
        }
        return changed;
    }

    auto DrawMaterialOptics( SurfaceMaterial &material ) -> bool {
        bool changed{};
        if ( ImGui::CollapsingHeader( MKT_LOC( "material_checkerboard" ).c_str() ) ) {
            auto checker{ material.GetCheckerboard() };
            bool edited{};

            if ( ImGui::BeginTable( "##Checkerboard", 2, ImGuiTableFlags_SizingStretchSame ) ) {
                const auto row{ []( core::cstr key ) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted( MKT_LOC( key ).c_str() );
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth( -1.0f );
                } };

                row( "material_checkerboard_enabled" );
                edited |= imgui::CheckBox( "##CheckerboardEnabled", checker.mEnabled );
                ImGui::BeginDisabled( !checker.mEnabled );
                row( "material_checkerboard_cell_size" );
                edited |= DrawFloatProperty( "##CheckerboardCellSize", &checker.mCellSize, { 0.01f, 100.0f }, "%.2f", false );
                row( "material_checkerboard_color_a" );
                edited |= ImGui::ColorEdit3( "##CheckerboardColorA", &checker.mColorA.x );
                row( "material_checkerboard_color_b" );
                edited |= ImGui::ColorEdit3( "##CheckerboardColorB", &checker.mColorB.x );
                row( "material_checkerboard_offset" );
                edited |= DrawFloatVectorProperty( "##CheckerboardOffset", &checker.mOffset.x, 3, { 0, 0 }, "%.3f", false );
                ImGui::EndDisabled();
                ImGui::EndTable();
            }

            if ( edited ) {
                material.SetCheckerboard( checker );
                changed = true;
            }
        }

        if ( ImGui::CollapsingHeader( "Foliage wind" ) ) {
            auto wind{ material.GetFoliageWind() };
            bool edited{};
            ImGui::PushID( "FoliageWind" );
            edited |= imgui::CheckBox( "Enabled", wind.mEnabled );
            ImGui::BeginDisabled( !wind.mEnabled );
            edited |= DrawFloatProperty( "Tree height (m)", &wind.mHeight, { .01f, 100.0f }, "%.2f", true );
            edited |= DrawFloatProperty( "Root height (m)", &wind.mRootHeight, { -100, 100 }, "%.3f", false );
            edited |= DrawFloatProperty( "Trunk bend", &wind.mTrunkBend, { 0, .25f }, "%.3f", true );
            edited |= DrawFloatProperty( "Branch motion (m)", &wind.mBranchMotion, { 0, 2 }, "%.3f", true );
            edited |= DrawFloatProperty( "Leaf flutter (m)", &wind.mLeafFlutter, { 0, .5f }, "%.3f", true );
            edited |= DrawFloatProperty( "Frequency", &wind.mFrequency, { 0, 10 }, "%.2f", true );
            ImGui::EndDisabled();
            ImGui::PopID();

            if ( edited ) {
                material.SetFoliageWind( wind );
                changed = true;
            }
        }

        if ( ImGui::CollapsingHeader( "Height mapping" ) ) {
            ImGui::PushID( "Height mapping" );
            auto height{ material.GetHeightMapping() };
            int mode{ static_cast<int>( height.mMode ) };
            bool edited{ ImGui::Combo( "Method", &mode, "None\0Bump\0Parallax occlusion\0Displacement\0" ) };
            height.mMode = static_cast<HeightMappingMode>( mode );
            const auto path{ material.GetTexturePath( MapType::eHeight ) };
            ImGui::Button( path.IsEmpty() ? "Drop height texture here" : path.GetC_Str(), { ImGui::GetContentRegionAvail().x, 0 } );
            if ( ImGui::BeginDragDropTarget() ) {
                if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_IMAGE" ) } ) {
                    if ( payload->Data && payload->DataSize > 1 && static_cast<const char *>( payload->Data )[payload->DataSize - 1] == 0 ) {
                        material.SetTextureReference( MapType::eHeight, filesystem::Path{ static_cast<const char *>( payload->Data ) } );
                        changed = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if ( !path.IsEmpty() && ImGui::SmallButton( "Clear height texture" ) ) {
                material.SetTextureReference( MapType::eHeight, {} );
                changed = true;
            }
            edited |= DrawFloatProperty( "Height scale", &height.mScale, { -1000, 1000 }, "%.4f", false );
            edited |= DrawFloatProperty( "Midlevel", &height.mMidlevel, { 0, 1 }, "%.3f", false );
            int uv{ static_cast<int>( height.mUVSet ) };
            edited |= ImGui::Combo( "Height UV set", &uv, "UV0\0UV1\0" );
            height.mUVSet = static_cast<u32>( uv );
            if ( height.mMode == HeightMappingMode::eParallax ) {
                int steps{ static_cast<int>( height.mSteps ) };
                edited |= ImGui::SliderInt( "Maximum steps", &steps, 8, 128 );
                height.mSteps = static_cast<u32>( steps );
            }
            ImGui::TextWrapped( "Height is linear grayscale. Displacement moves existing vertices and needs a sufficiently subdivided mesh; it does not alter physics colliders." );
            if ( edited ) {
                material.SetHeightMapping( height );
                changed = true;
            }
            ImGui::PopID();
        }
        const auto model{ material.GetShadingModel() };
        if ( model == ShadingModel::eDielectric ) {
            auto settings{ material.GetDielectric() };
            static constexpr const char *keys[]{ "material_air", "material_water", "material_ice", "material_quartz", "material_glass", "material_sapphire", "material_diamond" };
            static constexpr f32 indices[]{ 1.0003f, 1.333f, 1.31f, 1.46f, 1.52f, 1.77f, 2.42f };
            const char *preview{ MKT_LOC( "material_custom" ).c_str() };
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

            changed |= DrawFloatProperty( MKT_LOC( "material_ior" ), &settings.mIndexOfRefraction, { 1.f, 2.5f }, "%.4f", true );
            auto roughness{ material.GetRoughnessFactor() };
            if ( DrawFloatProperty( MKT_LOC( "material_editor_roughness" ), &roughness, { 0.f, 1.f }, "%.3f", true ) ) {
                material.SetRoughnessFactor( roughness );
                changed = true;
            }
            changed |= DrawFloatProperty( MKT_LOC( "material_transmission" ), &settings.mTransmission, { 0.f, 1.f }, "%.3f", true );
            if ( ImGui::CollapsingHeader( MKT_LOC( "material_optical_advanced" ).c_str() ) ) {
                changed |= imgui::CheckBox( MKT_LOC( "material_thin_walled" ).c_str(), settings.mThinWalled );
                ImGui::TextWrapped( "%s", MKT_LOC( "material_thin_walled_help" ).c_str() );
                ImGui::BeginDisabled( !settings.mThinWalled );
                changed |= DrawFloatProperty( MKT_LOC( "material_thickness" ), &settings.mThickness, { 0.f, 100.f }, "%.3f", true );
                ImGui::EndDisabled();
                changed |= DrawFloatProperty( MKT_LOC( "material_attenuation_distance" ), &settings.mAttenuationDistance, { .001f, 1000.f }, "%.3f", true );
                changed |= ImGui::ColorEdit3( MKT_LOC( "material_attenuation_color" ).c_str(), &settings.mAttenuationColor.x );
                auto absorption{ -glm::log( glm::max( settings.mAttenuationColor, float3{ 0.0001f } ) ) / settings.mAttenuationDistance };
                if ( DrawFloatVectorProperty( MKT_LOC( "material_medium_absorption" ).c_str(), &absorption.x, 3, { 0.f, 9.21034f / settings.mAttenuationDistance }, "%.4f", false ) ) {
                    settings.mAttenuationColor = glm::exp( -absorption * settings.mAttenuationDistance );
                    changed = true;
                }
                ImGui::BeginDisabled( settings.mThinWalled );
                changed |= DrawFloatVectorProperty( MKT_LOC( "material_medium_scattering" ).c_str(), &settings.mScattering.x, 3, { 0.f, 10000.f }, "%.3f", false );
                changed |= DrawFloatProperty( MKT_LOC( "material_anisotropy" ), &settings.mPhaseAnisotropy, { -.99f, .99f }, "%.3f", true );
                ImGui::EndDisabled();
                ImGui::TextWrapped( "%s", MKT_LOC( "material_medium_help" ).c_str() );
            }
            if ( changed ) {
                material.SetDielectric( settings );
            }
        }
        else if ( model == ShadingModel::eConductor ) {
            static constexpr const char *keys[]{ "material_gold", "material_copper", "material_silver", "material_aluminum" };
            if ( const auto preset{ PresetCombo( MKT_LOC( "material_choose_preset" ).c_str(), keys, 4 ) }; preset >= 0 ) {
                material.ApplyConductorPreset( as<u32>( preset ) );
                changed = true;
            }
            auto settings{ material.GetConductor() };
            changed |= DrawFloatVectorProperty( MKT_LOC( "material_complex_eta" ).c_str(), &settings.mEta.x, 3, { .0001f, 100.f }, "%.3f", false );
            changed |= DrawFloatVectorProperty( MKT_LOC( "material_complex_k" ).c_str(), &settings.mK.x, 3, { 0.f, 100.f }, "%.3f", false );
            ImGui::TextWrapped( "%s", MKT_LOC( "material_conductor_help" ).c_str() );
            if ( changed ) {
                material.SetConductor( settings );
            }
        }
        else if ( model == ShadingModel::eSubsurfaceScattering ) {
            if ( !ImGui::CollapsingHeader( MKT_LOC( "material_bssrdf" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                return false;
            }
            static constexpr const char *keys[]{ "material_skin1", "material_skin2", "material_marble", "material_milk" };
            if ( const auto preset{ PresetCombo( MKT_LOC( "material_choose_preset" ).c_str(), keys, 4 ) }; preset >= 0 ) {
                material.ApplySubsurfacePreset( as<u32>( preset ) );
                changed = true;
            }
            auto settings{ material.GetSubsurface() };
            changed |= DrawFloatVectorProperty( MKT_LOC( "material_sigma_a" ).c_str(), &settings.mAbsorption.x, 3, { 0.f, 10000.f }, "%.4f", false );
            changed |= DrawFloatVectorProperty( MKT_LOC( "material_sigma_s" ).c_str(), &settings.mScattering.x, 3, { 0.f, 10000.f }, "%.4f", false );
            changed |= DrawFloatProperty( MKT_LOC( "material_ior" ), &settings.mIndexOfRefraction, { 1.f, 2.f }, "%.3f", true );
            changed |= DrawFloatProperty( MKT_LOC( "material_anisotropy" ), &settings.mAnisotropy, { -.99f, .99f }, "%.3f", true );
            changed |= DrawFloatProperty( MKT_LOC( "material_mm_units" ).c_str(), &settings.mUnitsPerMillimeter, { .000001f, 1000.f }, "%.6f", false );
            ImGui::TextWrapped( "%s", MKT_LOC( "material_bssrdf_help" ).c_str() );
            if ( changed ) {
                material.SetSubsurface( settings );
            }
        }
        return changed;
    }
}// namespace mikoto::editor
