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

#include <cstdio>

#include <imgui.h>

#include <EASTL/vector.h>

#include <glm/glm.hpp>

#include <Application/PropertyWidgets.hh>
#include <Application/TerrainProperties.hh>

#include <Assets/AssetRegistryService.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/Utility.hh>

#include <Core/Core.hh>
#include <Core/LocalizationService.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Filesystem/FileSystem.hh>

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/Utility.hh>

#include <Layers/EditorLayer.hh>

#include <Panels/LandscapePanel.hh>

#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::editor {

    static auto TerrainRow( const char *label ) -> void {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( label );
        ImGui::TableSetColumnIndex( 1 );
        ImGui::SetNextItemWidth( -1.0f );
    }

    static auto FitTerrainToHeightmap( scene::LandscapeComponent &landscape ) -> bool {
        const auto heightmap{ landscape.GetHeightmap() };

        if ( !heightmap ) {
            return false;
        }

        auto world{ landscape.GetWorldDescription() };

        // Prefer an exact pixel-to-vertex match, including rectangular images.
        // Nonstandard section sizes remain valid and display their actual value.
        for ( core::u32 quads{ 255 }; quads > 0; --quads ) {
            if ( ( heightmap->mWidth - 1 ) % quads || ( heightmap->mHeight - 1 ) % quads ) {
                continue;
            }

            world.mQuadsPerSection = quads;
            world.mSectionsPerComponent = 1;
            world.mComponentsX = ( heightmap->mWidth - 1 ) / quads;
            world.mComponentsY = ( heightmap->mHeight - 1 ) / quads;

            if ( !renderer::IsTerrainDescriptionValid( world ) ) {
                continue;
            }

            landscape.SetComponentCount( 1, 1 );
            landscape.SetSectionsPerComponent( 1 );
            landscape.SetTerrainSection( quads );
            landscape.SetComponentCount( world.mComponentsX, world.mComponentsY );

            return true;
        }

        return false;
    }

    LandscapePanel::LandscapePanel( const LandscapePanelCreateInfo &info )
        : Panel{ "Landscape", "Panel", info.mGroup, info.mIsToggleable },
          mEditorState{ info.mState } {
        MKT_BEGIN_PROFILER();

        const eastl::string icon{ imgui::GetStringFromUnicode( 58359 ) };
        mPanelHeaderName = imgui::widget::MakeIconTitle( icon.c_str(), mPanelName );

        mPanelIsVisible = false;
    }

    auto LandscapePanel::UpdateHeightmapPreview( const eastl::shared_ptr<const renderer::TerrainHeightmap> &heightmap ) -> void {
        if ( mPreviewSource == heightmap ) {
            return;
        }

        mPreviewSource = heightmap;
        mHeightmapPreview.Reset();
        mPreviewError.clear();
        mMapSaveMessage.clear();

        if ( !heightmap ) {
            return;
        }

        try {
            // Build a small preview from the exact samples used by geometry.
            // Large heightmaps stay on the CPU; the UI needs at most 512 pixels per axis.
            const float factor{ glm::min( 1.0f, 512.0f / glm::max( heightmap->mWidth, heightmap->mHeight ) ) };
            const auto width{ glm::max( 1u, static_cast<core::u32>( heightmap->mWidth * factor ) ) };
            const auto height{ glm::max( 1u, static_cast<core::u32>( heightmap->mHeight * factor ) ) };
            eastl::vector<core::ubyte> pixels( static_cast<core::usize>( width ) * height * 4 );

            for ( core::u32 row{}; row < height; ++row ) {
                for ( core::u32 col{}; col < width; ++col ) {
                    const core::float2 uv{ static_cast<float>( col ) / glm::max( 1u, width - 1 ),
                        static_cast<float>( row ) / glm::max( 1u, height - 1 ) };
                    const auto value{ static_cast<core::ubyte>( glm::clamp( heightmap->Sample( uv ), 0.0f, 1.0f ) * 255.0f + 0.5f ) };
                    const core::usize pixel{ ( static_cast<core::usize>( row ) * width + col ) * 4 };
                    pixels[pixel] = pixels[pixel + 1] = pixels[pixel + 2] = value;
                    pixels[pixel + 3] = 255;
                }
            }

            auto image{ asset::ImageHandle::New() };
            image->mWidth = width;
            image->mHeight = height;
            image->mChannels = 4;
            image->mFormat = asset::ImageFormat::eRGBA8_UINT;
            image->mBufferSpan = memory::BufferSpanHandle::New( pixels.data(), pixels.size() );
            mHeightmapPreview = renderer::RenderSystem::Get()->GetGpuDevice()->CreateTexture(
                renderer::rhi::TextureCreateDescription{}
                    .SetName( "Landscape_HeightmapPreview" )
                    .SetImageData( image )
                    .SetWidth( width )
                    .SetHeight( height )
                    .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                    .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                    .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) );
        } catch ( const std::exception &error ) {
            mPreviewError = error.what();
        }
    }

    auto LandscapePanel::DrawBrushControls( scene::Entity &entity ) -> void {
        auto &landscape{ entity.GetComponent<scene::LandscapeComponent>() };
        auto &tool{ mEditorState->mTerrainTool };
        tool.SetTarget( mEditorState->mActiveScene, entity.GetComponent<scene::TagComponent>().GetGuid() );
        const bool runtime{ mEditorState->mActiveScene->IsPlaying() || mEditorState->mActiveScene->IsSimulating() };
        ImGui::BeginDisabled( runtime );
        if ( ImGui::BeginTable( "TerrainBrushSettings", 2, ImGuiTableFlags_SizingStretchProp ) ) {
            ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthStretch, .42f );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch, .58f );
            TerrainRow( MKT_LOC( "terrain_brush_active" ).c_str() );
            imgui::CheckBox( "##TerrainBrushActive", tool.mEnabled );
            TerrainRow( MKT_LOC( "terrain_brush_radius" ).c_str() );
            imgui::Slider( "##TerrainRadius", tool.mBrush.mRadius, { .1f, 500.0f }, "%.2f m" );
            TerrainRow( MKT_LOC( "terrain_brush_strength" ).c_str() );
            imgui::Slider( "##TerrainStrength", tool.mBrush.mStrength, { 0.0f, 1.0f }, "%.3f" );
            TerrainRow( MKT_LOC( "terrain_brush_falloff" ).c_str() );
            imgui::Slider( "##TerrainFalloff", tool.mBrush.mFalloff, { .01f, 1.0f }, "%.3f" );
            if ( tool.mBrush.mMode == TerrainEditMode::eSculpt ) {
                int operation{ core::as<int>( tool.mBrush.mSculptTool ) };
                TerrainRow( MKT_LOC( "terrain_sculpt_tool" ).c_str() );
                if ( ImGui::Combo( "##TerrainSculptTool", &operation, MKT_LOC( "terrain_sculpt_options" ).c_str() ) ) {
                    tool.mBrush.mSculptTool = static_cast<TerrainSculptTool>( operation );
                }
            }
            ImGui::EndTable();
        }
        ImGui::TextWrapped( "%s", MKT_LOC( "terrain_brush_help" ).c_str() );
        ImGui::BeginDisabled( tool.mHistoryIndex == 0 );
        if ( ImGui::Button( MKT_LOC( "terrain_undo_stroke" ).c_str() ) ) {
            tool.Undo( landscape );
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled( tool.mHistoryIndex >= tool.mHistory.size() );
        if ( ImGui::Button( MKT_LOC( "terrain_redo_stroke" ).c_str() ) ) {
            tool.Redo( landscape );
        }
        ImGui::EndDisabled();
        const auto heights{ landscape.GetHeightmap() };
        if ( heights && ( heights->mWidth > 2049 || heights->mHeight > 2049 ) ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "terrain_edit_budget" ).c_str() );
        }
        if ( tool.mBrush.mMode == TerrainEditMode::ePaint ) {
            DrawTerrainSurfaceLayers( landscape, tool.mBrush.mLayer, mLayerPreviews );
        }
        if ( ImGui::CollapsingHeader( MKT_LOC( "terrain_grass" ).c_str() ) ) {
            auto surface{ landscape.GetSurfaceSettings() };
            auto &grass{ surface.mGrass };
            bool changed{};
            if ( ImGui::BeginTable( "TerrainGrassSettings", 2, ImGuiTableFlags_SizingStretchProp ) ) {
                ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthStretch, .42f );
                ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch, .58f );
                TerrainRow( MKT_LOC( "terrain_grass_enabled" ).c_str() );
                changed |= imgui::CheckBox( "##GrassEnabled", grass.mEnabled );
                TerrainRow( MKT_LOC( "terrain_grass_mesh_mode" ).c_str() );
                changed |= imgui::CheckBox( "##GrassCustomMesh", grass.mUseCustomMesh );
                if ( grass.mUseCustomMesh ) {
                    TerrainRow( MKT_LOC( "terrain_grass_mesh" ).c_str() );
                    const auto label{ grass.mMesh.GetPath().IsEmpty() ? MKT_LOC( "terrain_grass_choose_mesh" ) : eastl::string{ grass.mMesh.GetPath().GetStem() } };
                    bool choose{ ImGui::Button( label.c_str(), ImVec2{ -ImGui::GetFrameHeight() - 4, 0 } ) };
                    if ( ImGui::IsItemHovered() && !grass.mMesh.GetPath().IsEmpty() ) {
                        ImGui::SetTooltip( "%s", grass.mMesh.GetPath().GetC_Str() );
                    }
                    filesystem::Path path{};
                    if ( ImGui::BeginDragDropTarget() ) {
                        if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_MODEL" ) } ) {
                            path = filesystem::Path{ static_cast<const char *>( payload->Data ) };
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();
                    if ( imgui::widget::IconButton( "##ClearGrassMesh", ICON_MD_CLOSE, { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() } ) ) {
                        grass.mMesh = {};
                        grass.mMeshIndex = 0;
                        changed = true;
                    }
                    if ( choose ) {
                        path = filesystem::OpenFileDialog( { { "Model", "gltf,glb,fbx,obj" } } );
                    }
                    if ( !path.IsEmpty() ) {
                        grass.mMesh = asset::AssetRef<asset::Model>{ path };
                        grass.mMeshIndex = 0;
                        changed = true;
                    }
                    if ( mGrassMeshPreview.GetRef().GetID() != grass.mMesh.GetID() ) {
                        mGrassMeshPreview = grass.mMesh.GetID() ? asset::AssetRegistryService::Get()->Request<asset::Model>( grass.mMesh ) : asset::AssetHandle<asset::Model>{};
                    }
                    TerrainRow( MKT_LOC( "terrain_grass_mesh_index" ).c_str() );
                    const auto model{ mGrassMeshPreview.GetIfReady() };
                    const int maximum{ model ? glm::max( 0, static_cast<int>( model->GetMeshNodeCount() ) - 1 ) : 8191 };
                    int meshIndex{ static_cast<int>( glm::min( grass.mMeshIndex, static_cast<core::u32>( maximum ) ) ) };
                    if ( ImGui::DragInt( "##GrassMeshIndex", &meshIndex, 1, 0, maximum ) ) {
                        grass.mMeshIndex = static_cast<core::u32>( meshIndex );
                        changed = true;
                    }
                    TerrainRow( MKT_LOC( "terrain_grass_mesh_weight" ).c_str() );
                    changed |= DrawFloatProperty( "##GrassMeshWeight", &grass.mMeshWeight, { 0.0f, 10.0f }, "%.3f", false );
                }
                TerrainRow( MKT_LOC( "terrain_grass_occlusion" ).c_str() );
                changed |= imgui::CheckBox( "##GrassOcclusion", grass.mOcclusionEnabled );
                TerrainRow( MKT_LOC( "terrain_grass_layer" ).c_str() );
                if ( ImGui::BeginCombo( "##GrassLayer", surface.mLayers[grass.mLayer].mName.c_str() ) ) {
                    for ( core::u32 layer{}; layer < renderer::kTerrainSurfaceLayerCount; ++layer ) {
                        ImGui::PushID( core::as<int>( layer ) );
                        if ( ImGui::Selectable( surface.mLayers[layer].mName.c_str(), grass.mLayer == layer ) ) {
                            grass.mLayer = layer;
                            changed = true;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                TerrainRow( MKT_LOC( grass.mUseCustomMesh ? "terrain_grass_instance_density" : "terrain_grass_density" ).c_str() );
                changed |= DrawFloatProperty( "##GrassDensity", &grass.mDensity, { .1f, 100.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( grass.mUseCustomMesh ? "terrain_grass_instance_height" : "terrain_grass_height" ).c_str() );
                changed |= DrawFloatProperty( "##GrassHeight", &grass.mHeight, { .05f, 4.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( grass.mUseCustomMesh ? "terrain_grass_instance_width" : "terrain_grass_width" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWidth", &grass.mWidth, { .002f, grass.mUseCustomMesh ? 4.0f : .3f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_bend" ).c_str() );
                changed |= DrawFloatProperty( "##GrassBend", &grass.mBend, { 0.0f, 1.5f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_clump" ).c_str() );
                changed |= DrawFloatProperty( "##GrassClumpSize", &grass.mClumpSize, { .2f, 20.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_distance" ).c_str() );
                changed |= DrawFloatProperty( "##GrassViewDistance", &grass.mViewDistance, { 5.0f, renderer::kMaxGrassViewDistance }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_lod" ).c_str() );
                changed |= DrawFloatProperty( "##GrassLodDistance", &grass.mLodDistance, { 1.0f, grass.mViewDistance }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_slope" ).c_str() );
                changed |= DrawFloatProperty( "##GrassMaxSlope", &grass.mMaxSlope, { 0.0f, 80.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_min_elevation" ).c_str() );
                changed |= DrawFloatProperty( "##GrassMinElevation", &grass.mMinElevation, { -1000.0f, 1000.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_max_elevation" ).c_str() );
                changed |= DrawFloatProperty( "##GrassMaxElevation", &grass.mMaxElevation, { -1000.0f, 10000.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_wind_speed" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWindSpeed", &grass.mWindSpeed, { 0.0f, 30.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_wind_strength" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWindStrength", &grass.mWindStrength, { 0.0f, 1.5f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_wind_x" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWindDirection.x", &grass.mWindDirection.x, { -1.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_wind_z" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWindDirection.y", &grass.mWindDirection.y, { -1.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_ao" ).c_str() );
                changed |= DrawFloatProperty( "##GrassAO", &grass.mAmbientOcclusion, { 0, 1 }, "%.2f", false );
                TerrainRow( MKT_LOC( "terrain_grass_cast_shadows" ).c_str() );
                changed |= imgui::CheckBox( "##GrassCastShadows", grass.mCastShadows );
                TerrainRow( MKT_LOC( "terrain_grass_contact_shadows" ).c_str() );
                changed |= imgui::CheckBox( "##GrassContactShadows", grass.mContactShadows );
                TerrainRow( MKT_LOC( "terrain_grass_shadow_density" ).c_str() );
                changed |= DrawFloatProperty( "##GrassShadowDensity", &grass.mShadowDensity, { 0, 2 }, "%.2f", false );
                TerrainRow( MKT_LOC( "terrain_grass_self_shadow" ).c_str() );
                changed |= DrawFloatProperty( "##GrassSelfShadow", &grass.mSelfShadow, { 0, 1 }, "%.2f", false );
                TerrainRow( MKT_LOC( "terrain_grass_roughness" ).c_str() );
                changed |= DrawFloatProperty( "##GrassRoughness", &grass.mRoughness, { .25f, 1 }, "%.2f", false );
                TerrainRow( MKT_LOC( "terrain_grass_clump_strength" ).c_str() );
                changed |= DrawFloatProperty( "##GrassClumpStrength", &grass.mClumpStrength, { 0.0f, 4.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_height_variation" ).c_str() );
                changed |= DrawFloatProperty( "##GrassHeightVariation", &grass.mHeightVariation, { 0.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_far_lod" ).c_str() );
                changed |= DrawFloatProperty( "##GrassFarLodDistance", &grass.mFarLodDistance, { grass.mLodDistance, grass.mViewDistance }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_min_density" ).c_str() );
                changed |= DrawFloatProperty( "##GrassMinDensity", &grass.mMinDensity, { .01f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_width_compensation" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWidthCompensation", &grass.mWidthCompensation, { 0.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_wind_frequency" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWindFrequency", &grass.mWindFrequency, { .001f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_flutter" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWindFlutter", &grass.mWindFlutter, { 0.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_wind_distance" ).c_str() );
                changed |= DrawFloatProperty( "##GrassWindDistance", &grass.mWindDistance, { 1.0f, renderer::kMaxGrassViewDistance }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_translucency" ).c_str() );
                changed |= DrawFloatProperty( "##GrassTranslucency", &grass.mTranslucency, { 0.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_translucency_power" ).c_str() );
                changed |= DrawFloatProperty( "##GrassTranslucencyPower", &grass.mTranslucencyPower, { 1.0f, 8.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_rounding" ).c_str() );
                changed |= DrawFloatProperty( "##GrassNormalRounding", &grass.mNormalRounding, { 0.0f, .5f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_dryness" ).c_str() );
                changed |= DrawFloatProperty( "##GrassDryAmount", &grass.mDryAmount, { 0.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_color_variation" ).c_str() );
                changed |= DrawFloatProperty( "##GrassColorVariation", &grass.mColorVariation, { 0.0f, 1.0f }, "%.3f", false );
                TerrainRow( MKT_LOC( "terrain_grass_root" ).c_str() );
                changed |= ImGui::ColorEdit3( "##GrassRoot", &grass.mRootColor.x );
                TerrainRow( MKT_LOC( "terrain_grass_tip" ).c_str() );
                changed |= ImGui::ColorEdit3( "##GrassTip", &grass.mTipColor.x );
                TerrainRow( MKT_LOC( "terrain_grass_mid" ).c_str() );
                changed |= ImGui::ColorEdit3( "##GrassMid", &grass.mMidColor.x );
                TerrainRow( MKT_LOC( "terrain_grass_dry" ).c_str() );
                changed |= ImGui::ColorEdit3( "##GrassDry", &grass.mDryColor.x );

                // Each entry shares the placement grid; weights select a model
                // for each root rather than adding overlapping full fields.
                if ( grass.mUseCustomMesh ) {
                    mGrassMeshVariantPreviews.resize( grass.mMeshVariants.size() );
                    for ( core::usize index{}; index < grass.mMeshVariants.size(); ) {
                        ImGui::PushID( static_cast<int>( index ) );
                        auto &variant{ grass.mMeshVariants[index] };
                        auto &preview{ mGrassMeshVariantPreviews[index] };
                        TerrainRow( MKT_LOC( "terrain_grass_mesh_variety" ).c_str() );
                        const auto label{ variant.mMesh.GetPath().IsEmpty() ? MKT_LOC( "terrain_grass_choose_mesh" ) : eastl::string{ variant.mMesh.GetPath().GetStem() } };
                        const bool choose{ ImGui::Button( label.c_str(), ImVec2{ -ImGui::GetFrameHeight() - 4, 0 } ) };
                        if ( ImGui::IsItemHovered() && !variant.mMesh.GetPath().IsEmpty() ) {
                            ImGui::SetTooltip( "%s", variant.mMesh.GetPath().GetC_Str() );
                        }
                        filesystem::Path path{};
                        if ( ImGui::BeginDragDropTarget() ) {
                            if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_MODEL" ) } ) {
                                path = filesystem::Path{ static_cast<const char *>( payload->Data ) };
                            }
                            ImGui::EndDragDropTarget();
                        }
                        ImGui::SameLine();
                        if ( imgui::widget::IconButton( "##RemoveGrassVariety", ICON_MD_CLOSE, { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() } ) ) {
                            grass.mMeshVariants.erase( grass.mMeshVariants.begin() + index );
                            mGrassMeshVariantPreviews.erase( mGrassMeshVariantPreviews.begin() + index );
                            changed = true;
                            ImGui::PopID();
                            continue;
                        }
                        if ( choose ) {
                            path = filesystem::OpenFileDialog( { { "Model", "gltf,glb,fbx,obj" } } );
                        }
                        if ( !path.IsEmpty() ) {
                            variant.mMesh = asset::AssetRef<asset::Model>{ path };
                            variant.mMeshIndex = 0;
                            changed = true;
                        }
                        if ( preview.GetRef().GetID() != variant.mMesh.GetID() ) {
                            preview = variant.mMesh.GetID() ? asset::AssetRegistryService::Get()->Request<asset::Model>( variant.mMesh ) : asset::AssetHandle<asset::Model>{};
                        }
                        TerrainRow( MKT_LOC( "terrain_grass_mesh_index" ).c_str() );
                        const auto model{ preview.GetIfReady() };
                        const int maximum{ model ? glm::max( 0, static_cast<int>( model->GetMeshNodeCount() ) - 1 ) : 8191 };
                        int meshIndex{ static_cast<int>( glm::min( variant.mMeshIndex, static_cast<core::u32>( maximum ) ) ) };
                        if ( ImGui::DragInt( "##GrassVariantIndex", &meshIndex, 1, 0, maximum ) ) {
                            variant.mMeshIndex = static_cast<core::u32>( meshIndex );
                            changed = true;
                        }
                        TerrainRow( MKT_LOC( "terrain_grass_mesh_weight" ).c_str() );
                        changed |= DrawFloatProperty( "##GrassVariantWeight", &variant.mWeight, { 0.0f, 10.0f }, "%.3f", false );
                        TerrainRow( MKT_LOC( "terrain_grass_mesh_height_scale" ).c_str() );
                        changed |= DrawFloatProperty( "##GrassVariantHeight", &variant.mHeightScale, { .1f, 4.0f }, "%.2f", false );
                        TerrainRow( MKT_LOC( "terrain_grass_mesh_width_scale" ).c_str() );
                        changed |= DrawFloatProperty( "##GrassVariantWidth", &variant.mWidthScale, { .1f, 4.0f }, "%.2f", false );
                        if ( !preview.GetError().empty() ) {
                            ImGui::TextWrapped( "%s", preview.GetError().c_str() );
                        }
                        if ( model && variant.mMeshIndex < model->GetMeshNodeCount() ) {
                            const auto &mesh{ model->GetMeshNode( variant.mMeshIndex ).GetRenderData() };
                            if ( model->IsSkinned() || !mesh || mesh->GetIndices().size() > 8192 ) {
                                ImGui::TextWrapped( "%s", MKT_LOC( "terrain_grass_mesh_invalid" ).c_str() );
                            }
                        }
                        ImGui::PopID();
                        ++index;
                    }
                    TerrainRow( MKT_LOC( "terrain_grass_mesh_variety" ).c_str() );
                    ImGui::BeginDisabled( grass.mMeshVariants.size() >= 7 );
                    if ( ImGui::Button( MKT_LOC( "terrain_grass_add_mesh" ).c_str() ) ) {
                        grass.mMeshVariants.push_back( {} );
                        changed = true;
                    }
                    ImGui::EndDisabled();
                }
                ImGui::EndTable();
            }
            if ( changed ) {
                landscape.SetSurfaceSettings( surface );
            }
            ImGui::TextWrapped( "%s", MKT_LOC( "terrain_grass_help" ).c_str() );
            if ( grass.mUseCustomMesh ) {
                ImGui::TextWrapped( "%s", MKT_LOC( "terrain_grass_mesh_help" ).c_str() );
                ImGui::TextWrapped( "%s", MKT_LOC( "terrain_grass_mesh_mix_help" ).c_str() );
                if ( !mGrassMeshPreview.GetError().empty() ) {
                    ImGui::TextWrapped( "%s", mGrassMeshPreview.GetError().c_str() );
                }
                if ( const auto model{ mGrassMeshPreview.GetIfReady() }; model && grass.mMeshIndex < model->GetMeshNodeCount() ) {
                    const auto &mesh{ model->GetMeshNode( grass.mMeshIndex ).GetRenderData() };
                    if ( model->IsSkinned() || !mesh || mesh->GetIndices().size() > 8192 ) {
                        ImGui::TextWrapped( "%s", MKT_LOC( "terrain_grass_mesh_invalid" ).c_str() );
                    }
                }
            }
        }
        ImGui::EndDisabled();
    }

    auto LandscapePanel::OnRender( float ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 680.0f, 650.0f }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            ImGui::End();
            return;
        }

        auto *scene{ mEditorState->mActiveScene };
        auto *entity{ mEditorState->mSelectedEntity };

        if ( !scene || !entity || !entity->HasComponent<scene::LandscapeComponent>() ) {
            UpdateHeightmapPreview( {} );
            ImGui::TextWrapped( "%s", MKT_LOC( "landscape_select_terrain_to_edit" ).c_str() );
            ImGui::BeginDisabled( !scene );

            if ( ImGui::Button( "Create terrain", { -1.0f, 0.0f } ) ) {
                if ( auto *terrain{ scene->CreateEntity( scene::EntityCreateInfo{
                         .mName = "Terrain",
                         .mEntityType = scene::EntityType::eTerrain } ) } ) {
                    mEditorState->SelectEntity( terrain );
                }
            }

            ImGui::EndDisabled();
            ImGui::End();
            return;
        }

        auto &landscape{ entity->GetComponent<scene::LandscapeComponent>() };
        const auto id{ entity->GetComponent<scene::TagComponent>().GetGuid() };

        if ( mEditedScene != scene || mEditedEntity != id ) {
            mEditedScene = scene;
            mEditedEntity = id;
            mImportPath = landscape.GetHeightmapReference().GetPath();
            mEditError.clear();
            mPreviewSelected = false;
        }

        ImGui::Text( "Landscape: %s", entity->GetComponent<scene::TagComponent>().GetTag().data() );
        auto &tool{ mEditorState->mTerrainTool };
        if ( ImGui::BeginTable( "TerrainMode", 2, ImGuiTableFlags_SizingStretchProp ) ) {
            ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthStretch, .42f );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch, .58f );
            TerrainRow( MKT_LOC( "terrain_edit_mode" ).c_str() );
            const core::cstr modes[]{ MKT_LOC( "terrain_mode_generate" ).c_str(),
                MKT_LOC( "terrain_mode_sculpt" ).c_str(), MKT_LOC( "terrain_mode_paint" ).c_str() };
            int selected{ core::as<int>( tool.mBrush.mMode ) };
            if ( ImGui::Combo( "##TerrainEditMode", &selected, modes, 3 ) ) {
                tool.EndStroke( landscape );
                tool.mBrush.mMode = static_cast<TerrainEditMode>( selected );
                tool.mEnabled = tool.mBrush.mMode != TerrainEditMode::eGenerate;
                mSelectGeneratedNoise = tool.mBrush.mMode == TerrainEditMode::eGenerate;
            }
            ImGui::EndTable();
        }
        if ( tool.mBrush.mMode != TerrainEditMode::eGenerate ) {
            DrawBrushControls( *entity );
            ImGui::End();
            return;
        }


        if ( !ImGui::BeginTable( "##LandscapeLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV ) ) {
            ImGui::End();
            return;
        }

        ImGui::TableSetupColumn( "Preview", ImGuiTableColumnFlags_WidthStretch, 0.30f );
        ImGui::TableSetupColumn( "Settings", ImGuiTableColumnFlags_WidthStretch, 0.70f );
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Heightmap / Noise" );
        UpdateHeightmapPreview( landscape.GetHeightmap() );

        if ( mHeightmapPreview ) {
            const float width{ glm::max( 1.0f, ImGui::GetContentRegionAvail().x - 4.0f ) };
            const float ratio{ static_cast<float>( mHeightmapPreview->GetHeight() ) / mHeightmapPreview->GetWidth() };
            // Preserve aspect ratio without making tall sources consume the whole panel.
            const float shownWidth{ glm::min( width, 300.0f / ratio ) };
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { 2.0f, 2.0f } );

            if ( imgui::PushImageButton( "##TerrainHeightmapPreview", imgui::ImGuiService::Get()->GetTextureID( mHeightmapPreview ),
                     { shownWidth, shownWidth * ratio } ) ) {
                mPreviewSelected = !mPreviewSelected;
            }

            ImGui::PopStyleVar();

            if ( mPreviewSelected || ImGui::IsItemHovered() ) {
                ImGui::GetWindowDrawList()->AddRect( ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                    ImGui::GetColorU32( ImGuiCol_HeaderActive ), ImGui::GetStyle().FrameRounding, 0, 2.0f );
            }

            if ( const auto request{ landscape.GetNoiseGeneration() } ) {
                const auto heightmap{ request->GetHeightmap() };
                ImGui::Spacing();
                ImGui::BeginDisabled( !heightmap );

                if ( ImGui::Button( "Save heightmap...", { -1.0f, 0.0f } ) ) {
                    const auto path{ filesystem::SaveFileDialog( "TerrainHeightmap.hdr", { { "HDR heightmap", "hdr" } } ) };

                    if ( !path.IsEmpty() ) {
                        mMapSaveMessage.clear();
                        mPreviewError.clear();

                        if ( !path.IsExtension( "hdr" ) ) {
                            mPreviewError = "Save the generated heightmap with the .hdr extension.";
                        }
                        else {
                            // Export the complete GPU-generated float image, not
                            // the reduced 8-bit texture used by the preview button.
                            const auto &image{ heightmap->mImage };

                            try {
                                asset::WriteImage( path, image->mBufferSpan->GetData(), image->mWidth, image->mHeight, image->mFormat );
                                mMapSaveMessage = string::Format( "Saved: {}", path.GetAbsolute() );
                            } catch ( const std::exception &error ) {
                                mPreviewError = error.what();
                            }
                        }
                    }
                }

                ImGui::EndDisabled();

                if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) ) {
                    ImGui::SetTooltip( "Save the complete generated map as HDR for heightmap import. Available when generation finishes." );
                }

                if ( !mMapSaveMessage.empty() ) {
                    ImGui::TextWrapped( "%s", mMapSaveMessage.c_str() );
                }
            }

            ImGui::Text( "%u x %u", mPreviewSource->mWidth, mPreviewSource->mHeight );
            ImGui::TextWrapped( "Red channel supplies height. Preview clips values outside 0..1; HDR terrain heights remain unchanged." );
        }
        else {
            ImGui::TextWrapped( "Import a heightmap or use Generated Noise to preview it here." );
        }

        if ( const auto request{ landscape.GetNoiseGeneration() }; request && !request->GetHeightmap() && request->GetError().empty() ) {
            ImGui::TextWrapped( "Generating noise..." );
        }

        if ( !mPreviewError.empty() ) {
            ImGui::TextWrapped( "%s", mPreviewError.c_str() );
        }

        ImGui::SeparatorText( MKT_LOC( "render_debug_view" ).c_str() );
        eastl::array debugViews{ MKT_LOC( "render_debug_none" ).c_str(),
            MKT_LOC( "render_debug_vertices" ).c_str(),
            MKT_LOC( "render_debug_grid" ).c_str(),
            MKT_LOC( "render_debug_vertices_grid" ).c_str() };
        int debugView{ ( landscape.AreVerticesVisible() ? 1 : 0 ) | ( landscape.IsGridVisible() ? 2 : 0 ) };
        ImGui::SetNextItemWidth( -1.0f );

        if ( ImGui::Combo( "##LandscapeDebugView", &debugView, debugViews.data(), debugViews.size() ) ) {
            landscape.SetShowVertices( ( debugView & 1 ) != 0 );
            landscape.SetShowGrid( ( debugView & 2 ) != 0 );
        }

        const auto grid{ landscape.GetWorldDescription() };
        ImGui::Text( "%u vertices", grid.GetResolutionX() * grid.GetResolutionY() );
        ImGui::Text( "%u triangles", ( grid.GetResolutionX() - 1 ) * ( grid.GetResolutionY() - 1 ) * 2 );

        ImGui::TableSetColumnIndex( 1 );
        ImGui::SeparatorText( MKT_LOC( "terrain_height_source" ).c_str() );

        bool importTab{};
        bool generatedTab{};
        bool noiseChanged{};
        auto noise{ landscape.GetNoiseSettings() };

        if ( ImGui::BeginTabBar( "##TerrainSource" ) ) {
            if ( ImGui::BeginTabItem( "Create New" ) ) {
                ImGui::TextWrapped( "Edit the selected landscape below. Create Flat clears its heightmap." );
                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Import from File" ) ) {
                importTab = true;
                ImGui::TextWrapped( "PNG (8/16-bit), JPG, BMP, TGA or HDR. Height = base elevation + red channel x height scale." );
                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Generated Noise", nullptr, mSelectGeneratedNoise ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None ) ) {
                generatedTab = true;
                ImGui::TextWrapped( "Generate a height texture from fBm and Perlin. Editing parameters updates the terrain and the preview automatically." );
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
            mSelectGeneratedNoise = false;
        }

        const auto accept{ [this]( bool success ) {
            mEditError = success ? "" : "Invalid terrain size. Use positive component counts and at most 4,194,304 vertices per landscape.";
        } };
        auto world{ landscape.GetWorldDescription() };

        if ( ImGui::BeginTable( "##TerrainSettings", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp ) ) {
            ImGui::TableSetupColumn( "Property", ImGuiTableColumnFlags_WidthStretch, 0.42f );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch, 0.58f );

            if ( generatedTab ) {
                TerrainRow( "fBm Frequency" );
                noiseChanged |= imgui::widget::CircleSlider( "##NoiseFrequency", &noise.mFBM.mFrequency, 0.01f, 64.0f, "%.2f" );
                TerrainRow( "fBm Amplitude" );
                noiseChanged |= imgui::widget::CircleSlider( "##NoiseAmplitude", &noise.mFBM.mAmplitude, 0.0f, 4.0f, "%.2f" );
                TerrainRow( "Persistence" );
                noiseChanged |= imgui::widget::CircleSlider( "##NoisePersistence", &noise.mFBM.mPersistence, 0.0f, 1.0f, "%.2f" );
                TerrainRow( "Lacunarity" );
                noiseChanged |= imgui::widget::CircleSlider( "##NoiseLacunarity", &noise.mFBM.mLacunarity, 1.0f, 4.0f, "%.2f" );
                TerrainRow( "Octaves" );
                int octaves{ static_cast<int>( noise.mFBM.mOctaves ) };

                if ( ImGui::DragInt( "##NoiseOctaves", &octaves, 1.0f, 1, 12, "%d", ImGuiSliderFlags_AlwaysClamp ) ) {
                    noise.mFBM.mOctaves = static_cast<core::u32>( octaves );
                    noiseChanged = true;
                }

                TerrainRow( "Perlin Frequency" );
                noiseChanged |= imgui::widget::CircleSlider( "##PerlinFrequency", &noise.mPerlinFrequency, 0.01f, 64.0f, "%.2f" );
                TerrainRow( "Perlin Amplitude" );
                noiseChanged |= imgui::widget::CircleSlider( "##PerlinAmplitude", &noise.mPerlinAmplitude, 0.0f, 4.0f, "%.2f" );
                TerrainRow( "Perlin Blend" );
                noiseChanged |= imgui::widget::CircleSlider( "##PerlinBlend", &noise.mPerlinBlend, 0.0f, 1.0f, "%.2f" );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetTooltip( "0: fBm only. 1: Perlin only. Values between blend both layers." );
                }

                TerrainRow( "Noise Offset X" );
                noiseChanged |= imgui::widget::CircleSlider( "##NoiseOffsetX", &noise.mOffset.x, -256.0f, 256.0f, "%.2f" );
                TerrainRow( "Noise Offset Y" );
                noiseChanged |= imgui::widget::CircleSlider( "##NoiseOffsetY", &noise.mOffset.y, -256.0f, 256.0f, "%.2f" );
                TerrainRow( "Generated Resolution" );
                ImGui::TextUnformatted( "1024 x 1024" );
            }

            TerrainRow( "Enable Edit Layers" );
            ImGui::BeginDisabled();
            bool layers{ world.mEnableEditLayers };
            imgui::CheckBox( "##EditLayers", layers );
            ImGui::EndDisabled();

            if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) ) {
                ImGui::SetTooltip( "%s", MKT_LOC( "terrain_edit_layers_pending" ).c_str() );
            }

            if ( importTab ) {
                TerrainRow( "Heightmap File" );
                auto path{ mImportPath.GetAbsolute() };
                ImGui::InputText( "##HeightmapPath", path.data(), path.size() + 1, ImGuiInputTextFlags_ReadOnly );

                if ( ImGui::Button( "Browse...", { -1.0f, 0.0f } ) ) {
                    const auto selected{ filesystem::OpenFileDialog( { { "Heightmap images", "png,jpg,jpeg,bmp,tga,hdr" } } ) };

                    if ( !selected.IsEmpty() ) {
                        mImportPath = selected;
                    }
                }

                TerrainRow( "Heightmap Resolution" );

                if ( const auto image{ landscape.GetHeightmap() } ) {
                    ImGui::Text( "%u x %u (loaded)", image->mWidth, image->mHeight );
                }
                else {
                    ImGui::TextDisabled( "No heightmap loaded" );
                }
            }

            auto &transform{ entity->GetComponent<scene::TransformComponent>() };
            auto location{ transform.GetTranslation() };
            auto rotation{ transform.GetRotation() };
            auto scale{ transform.GetScale() };

            TerrainRow( "Location" );

            if ( ImGui::DragFloat3( "##TerrainLocation", &location.x, 0.1f ) ) {
                transform.SetTranslation( location );
            }

            TerrainRow( "Rotation" );

            if ( ImGui::DragFloat3( "##TerrainRotation", &rotation.x, 0.1f ) ) {
                transform.SetRotation( rotation );
            }

            TerrainRow( "Scale" );

            if ( ImGui::DragFloat3( "##TerrainScale", &scale.x, 0.01f, 0.001f, 10000.0f ) ) {
                transform.SetScale( scale );
            }

            TerrainRow( "Origin (local XZ)" );

            if ( ImGui::DragFloat2( "##TerrainOrigin", &world.mOrigin.x, 0.1f ) ) {
                landscape.SetOrigin( world.mOrigin );
                accept( landscape.GetWorldDescription().mOrigin == world.mOrigin );
            }

            TerrainRow( "Vertex Spacing" );

            if ( imgui::widget::CircleSlider( "##TerrainSpacing", &world.mVertexSpacing, 0.001f, 10000.0f, "%.3f" ) ) {
                landscape.SetVertexSpacing( world.mVertexSpacing );
                accept( landscape.GetWorldDescription().mVertexSpacing == world.mVertexSpacing );
            }

            TerrainRow( "Height Scale" );

            if ( imgui::widget::CircleSlider( "##TerrainHeight", &world.mHeightScale, 0.0f, 100000.0f, "%.2f" ) ) {
                landscape.SetHeightScale( world.mHeightScale );
                accept( landscape.GetWorldDescription().mHeightScale == world.mHeightScale );
            }

            TerrainRow( "Base Elevation" );

            if ( imgui::widget::CircleSlider( "##TerrainElevation", &world.mBaseElevation, -100000.0f, 100000.0f, "%.2f" ) ) {
                landscape.SetBaseElevation( world.mBaseElevation );
                accept( landscape.GetWorldDescription().mBaseElevation == world.mBaseElevation );
            }

            TerrainRow( MKT_LOC( "landscape_low_color" ).c_str() );

            if ( ImGui::ColorEdit3( "##TerrainLowColor", &world.mLowColor.x ) ) {
                landscape.SetLowColor( world.mLowColor );
            }

            TerrainRow( MKT_LOC( "landscape_middle_color" ).c_str() );

            if ( ImGui::ColorEdit3( "##TerrainMiddleColor", &world.mMiddleColor.x ) ) {
                landscape.SetMiddleColor( world.mMiddleColor );
            }

            TerrainRow( MKT_LOC( "landscape_high_color" ).c_str() );

            if ( ImGui::ColorEdit3( "##TerrainHighColor", &world.mHighColor.x ) ) {
                landscape.SetHighColor( world.mHighColor );
            }

            TerrainRow( "Section Size" );
            const auto sectionLabel{ string::Format( "{} x {} Quads", world.mQuadsPerSection, world.mQuadsPerSection ) };

            if ( ImGui::BeginCombo( "##TerrainSection", sectionLabel.c_str() ) ) {
                for ( const core::u32 quads: { 7u, 15u, 31u, 63u, 127u, 255u } ) {
                    const auto label{ string::Format( "{} x {} Quads", quads, quads ) };

                    if ( ImGui::Selectable( label.c_str(), quads == world.mQuadsPerSection ) ) {
                        landscape.SetTerrainSection( quads );
                        accept( landscape.GetWorldDescription().mQuadsPerSection == quads );
                    }
                }

                ImGui::EndCombo();
            }

            TerrainRow( "Sections Per Component" );
            int sections{ static_cast<int>( world.mSectionsPerComponent ) - 1 };

            if ( ImGui::Combo( "##TerrainSections", &sections,
                     "1 x 1 Section\0"
                     "2 x 2 Sections\0" ) ) {
                const auto sectionCount{ core::as<core::u32>( sections + 1 ) };
                landscape.SetSectionsPerComponent( sectionCount );
                accept( landscape.GetWorldDescription().mSectionsPerComponent == sectionCount );
            }

            TerrainRow( "Number of Components" );
            int counts[2]{ static_cast<int>( world.mComponentsX ), static_cast<int>( world.mComponentsY ) };

            if ( ImGui::DragInt2( "##TerrainComponents", counts, 1.0f, 1, 256, "%d", ImGuiSliderFlags_AlwaysClamp ) ) {
                const auto x{ core::as<core::u32>( counts[0] ) };
                const auto y{ core::as<core::u32>( counts[1] ) };
                landscape.SetComponentCount( x, y );

                const auto applied{ landscape.GetWorldDescription() };
                accept( applied.mComponentsX == x && applied.mComponentsY == y );
            }

            world = landscape.GetWorldDescription();
            TerrainRow( "Overall Resolution" );
            int resolution[2]{ static_cast<int>( world.GetResolutionX() ), static_cast<int>( world.GetResolutionY() ) };
            ImGui::InputInt2( "##TerrainResolution", resolution, ImGuiInputTextFlags_ReadOnly );
            TerrainRow( "Total Components" );
            int count{ static_cast<int>( world.GetTotalComponents() ) };
            ImGui::InputInt( "##TerrainComponentTotal", &count, 0, 0, ImGuiInputTextFlags_ReadOnly );
            TerrainRow( "World Size (local XZ)" );
            const auto size{ world.GetWorldSize() };
            ImGui::Text( "%.2f x %.2f", size.x, size.y );
            ImGui::EndTable();
        }

        ImGui::Spacing();

        if ( generatedTab ) {
            const bool generate{ ImGui::Button( "Generate / Apply", { -1.0f, 0.0f } ) };

            if ( noiseChanged || generate ) {
                if ( noise.IsValid() ) {
                    landscape.SetGeneratedNoise( noise );
                    mEditError.clear();
                }
                else {
                    mEditError = "Invalid noise parameters.";
                }

                mImportPath = {};
            }
        }
        else if ( importTab ) {
            const float half{ ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x ) * 0.5f };
            ImGui::BeginDisabled( !landscape.GetHeightmap() );

            if ( ImGui::Button( "Fit to Data", { half, 0.0f } ) ) {
                mEditError = FitTerrainToHeightmap( landscape ) ? "" : "An exact fit exceeds the grid limits. The current grid still samples the full image.";
            }

            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled( mImportPath.IsEmpty() );

            if ( ImGui::Button( "Import / Reload", { -1.0f, 0.0f } ) ) {
                landscape.SetHeightmap( asset::AssetRef<asset::Texture>{ mImportPath } );
                mEditError.clear();
            }

            ImGui::EndDisabled();
        }
        else if ( ImGui::Button( "Create Flat", { -1.0f, 0.0f } ) ) {
            landscape.ClearHeightmap();
            mImportPath = {};
            mEditError.clear();
        }

        if ( !mEditError.empty() ) {
            ImGui::TextWrapped( "%s", mEditError.c_str() );
        }

        if ( !landscape.GetHeightmapError().empty() ) {
            ImGui::TextWrapped( "%s", landscape.GetHeightmapError().c_str() );
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "terrain_manage_help" ).c_str() );
        ImGui::EndTable();
        ImGui::End();
    }
}// namespace mikoto::editor
