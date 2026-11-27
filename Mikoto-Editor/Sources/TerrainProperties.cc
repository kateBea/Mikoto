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

#include <Application/PropertyWidgets.hh>
#include <Application/TerrainProperties.hh>

#include <Assets/AssetRegistryService.hh>

#include <Core/LocalizationService.hh>

#include <Filesystem/FileSystem.hh>

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiWidget.hh>

namespace mikoto::editor {

    static auto SurfaceLayerRow( core::cstr label ) -> void {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::AlignTextToFramePadding();
        ImGui::TextWrapped( "%s", label );
        ImGui::TableSetColumnIndex( 1 );
        ImGui::SetNextItemWidth( -1.0f );
    }

    auto DrawTerrainSurfaceLayers( scene::LandscapeComponent &landscape, core::u32 &selectedLayer,
        eastl::array<asset::AssetHandle<asset::Texture>, 3> &previews ) -> void {
        selectedLayer = glm::min( selectedLayer, renderer::kTerrainSurfaceLayerCount - 1 );
        auto surface{ landscape.GetSurfaceSettings() };
        bool changed{};

        ImGui::SeparatorText( MKT_LOC( "terrain_surface_layers" ).c_str() );

        if ( ImGui::BeginCombo( "##TerrainLayer", surface.mLayers[selectedLayer].mName.c_str() ) ) {
            for ( core::u32 index{}; index < renderer::kTerrainSurfaceLayerCount; ++index ) {
                ImGui::PushID( core::as<int>( index ) );

                if ( ImGui::Selectable( surface.mLayers[index].mName.c_str(), index == selectedLayer ) ) {
                    selectedLayer = index;
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        auto &layer{ surface.mLayers[selectedLayer] };

        if ( ImGui::BeginTable( "TerrainLayerSettings", 2, ImGuiTableFlags_SizingStretchProp ) ) {
            ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthStretch, .42f );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch, .58f );
            char name[128]{};
            std::snprintf( name, sizeof( name ), "%s", layer.mName.c_str() );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_name" ).c_str() );

            if ( ImGui::InputText( "##TerrainLayerName", name, sizeof( name ) ) ) {
                layer.mName = name;
                changed = true;
            }

            SurfaceLayerRow( MKT_LOC( "terrain_layer_tint" ).c_str() );
            changed |= ImGui::ColorEdit3( "##TerrainLayerTint", &layer.mTint.x );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_roughness" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainLayerRoughness", &layer.mRoughness, { .04f, 1.0f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_tiling_x" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainLayerTilingX", &layer.mUVScale.x, { .001f, 10.0f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_tiling_y" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainLayerTilingY", &layer.mUVScale.y, { .001f, 10.0f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_normal_strength" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainLayerNormalStrength", &layer.mNormalStrength, { 0.0f, 4.0f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_min_height" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainLayerMinHeight", &layer.mHeightRange.x, { -1.0f, layer.mHeightRange.y }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_max_height" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainLayerMaxHeight", &layer.mHeightRange.y, { layer.mHeightRange.x, 2.0f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_slope" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainLayerSlope", &layer.mSlopeInfluence, { 0.0f, 1.0f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_layer_blend" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainBlendWidth", &surface.mBlendWidth, { .001f, .5f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_height_blend" ).c_str() );
            changed |= imgui::CheckBox( "##TerrainHeightBlend", surface.mHeightBlendEnabled );

            SurfaceLayerRow( MKT_LOC( "terrain_height_blend_width" ).c_str() );
            changed |= DrawFloatProperty( "##TerrainHeightBlendWidth", &surface.mHeightBlendWidth, { .001f, 1.0f }, "%.3f", true );

            SurfaceLayerRow( MKT_LOC( "terrain_elevation_debug" ).c_str() );
            changed |= imgui::CheckBox( "##TerrainElevationDebug", surface.mShowElevationColors );

            const auto textureSlot{ [&]( core::u32 slot, core::cstr label, asset::AssetRef<asset::Texture> &reference ) {
                ImGui::PushID( core::as<int>( slot ) );
                SurfaceLayerRow( label );
                auto &request{ previews[slot] };

                if ( request.GetRef().GetID() != reference.GetID() ) {
                    request = reference.GetPath().IsEmpty() ? asset::AssetHandle<asset::Texture>{} : asset::AssetRegistryService::Get()->Request<asset::Texture>( reference );
                }

                const auto loaded{ request.GetIfReady() };
                const auto preview{ loaded ? loaded->GetResource() : asset::AssetRegistryService::Get()->GetDummyTexture() };
                bool choose{ imgui::PushImageButton( "##LayerPreview", imgui::ImGuiService::Get()->GetTextureID( preview ), { 52, 52 } ) };
                filesystem::Path path{};

                if ( ImGui::BeginDragDropTarget() ) {
                    if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_IMAGE" ) } ) {
                        path = filesystem::Path{ static_cast<const char *>( payload->Data ) };
                    }

                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                choose |= ImGui::Button( MKT_LOC( "terrain_choose_texture" ).c_str() );
                ImGui::SameLine();

                if ( imgui::widget::IconButton( "##ClearLayerTexture", ICON_MD_CLOSE, { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() } ) ) {
                    reference = {};
                    changed = true;
                }

                if ( choose ) {
                    path = filesystem::OpenFileDialog( { { "Texture", "png,jpg,jpeg,bmp,tga,hdr,ktx2" } } );
                }

                if ( !path.IsEmpty() ) {
                    reference = asset::AssetRef<asset::Texture>{ path };
                    changed = true;
                }

                if ( !request.GetError().empty() ) {
                    ImGui::TextWrapped( "%s", request.GetError().c_str() );
                }

                ImGui::PopID();
            } };

            textureSlot( 0, MKT_LOC( "terrain_layer_albedo" ).c_str(), layer.mAlbedo );
            textureSlot( 1, MKT_LOC( "terrain_layer_normal" ).c_str(), layer.mNormal );
            textureSlot( 2, MKT_LOC( "terrain_layer_height" ).c_str(), layer.mHeight );
            ImGui::EndTable();
        }

        if ( changed ) {
            landscape.SetSurfaceSettings( surface );
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "terrain_layers_help" ).c_str() );
    }

}// namespace mikoto::editor
