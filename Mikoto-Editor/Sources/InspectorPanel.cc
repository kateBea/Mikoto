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

#include <array>
#include <cstring>
#include <exception>
#include <tuple>

#include <imgui.h>
#include <imgui_internal.h>

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/algorithm.h>
#include <EASTL/string_view.h>
#include <EASTL/type_traits.h>

#include <glm/gtc/type_ptr.hpp>

#include <nlohmann/json.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Memory/Allocator.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/AssetModel.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/Utility.hh>

#include <Animation/Animator.hh>
#include <Animation/Skeleton.hh>
#include <Animation/AnimationSystem.hh>
#include <Animation/SkinnedAnimation.hh>

#include <Audio/AudioService.hh>

#include <Physics/PhysicsWorld.hh>

#include <Video/VideoService.hh>

#include <Text/Utility.hh>
#include <Text/TextLayout.hh>

#include <Renderer/Core/MeshRenderData.hh>
#include <Renderer/RayTracing/RayTracing.hh>
#include <Renderer/Particle/ParticleEmitter.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Utility.hh>
#include <Scene/Component.hh>

#include <Scripting/ScriptingService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsFontAwesome6.h>
#include <ImGui/IconsMaterialDesign.h>

#include <Application/MaterialProperties.hh>
#include <Application/ParticleProperties.hh>

#include <Panels/AudioPanel.hh>
#include <Panels/ComponentMenu.hh>
#include <Panels/InspectorPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;
    using namespace filesystem;


    template<typename ComponentType>
    static auto DrawComponent(
        InspectorPanel& panel,
        const eastl::string_view componentLabel,
        scene::Entity& entity,
        const InspectorPanel::ComponentUIFunc& uiFunc,
        const bool hasRemoveButton = true ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        constexpr ImGuiTreeNodeFlags treeNodeFlags{
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton
        };

        if ( entity.HasComponent<ComponentType>() ) {
            ImGui::PushID( static_cast<core::i32>( entt::type_hash<ComponentType>::value() ) );
            bool removeComponent{ false };

            ImGui::PushStyleColor( ImGuiCol_Header, ImGui::GetStyleColorVec4( ImGuiCol_TableHeaderBg ) );
            ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4( ImGuiCol_ButtonHovered ) );

            const bool componentNodeOpen{ ImGui::TreeNodeEx( "##Component", treeNodeFlags, "%s", componentLabel.data() ) };
            const ImVec2 headerMin{ ImGui::GetItemRectMin() };
            const ImVec2 headerMax{ ImGui::GetItemRectMax() };

            imgui::SetCursorHandOnLastItemHovered();

            ImGui::PopStyleColor( 2 );

            if ( hasRemoveButton ) {
                constexpr float inset{ 2.0f };
                const float buttonSize{ headerMax.y - headerMin.y - inset * 2.0f };
                ImGui::SameLine();
                ImGui::SetCursorScreenPos( { eastl::min( headerMax.x, ImGui::GetWindowDrawList()->GetClipRectMax().x ) - buttonSize, headerMin.y + inset } );
                const bool openOptions{ imgui::widget::IconButton( "Options", ICON_MD_SETTINGS, { buttonSize, buttonSize } ) };

                if ( openOptions ) {
                    ImGui::OpenPopup( "##DrawComponent_ButtonPopUp" );
                }

                imgui::SetCursorHandOnLastItemHovered();

                if ( ImGui::BeginPopup( "##DrawComponent_ButtonPopUp" ) ) {
                    if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "inspector_remove_component", "Remove Component" ).c_str() ) ) {
                        removeComponent = true;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }

            if ( componentNodeOpen ) {
                ImGui::PushStyleVar( ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.5f );
                ImGui::TreePush( "##Body" );
                ( panel.*uiFunc )( entity );
                ImGui::TreePop();
                ImGui::PopStyleVar();
            }

            if ( removeComponent ) {
                entity.RemoveComponent<ComponentType>();
            }

            ImGui::PopID();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
    }

    static auto DrawTextureProperties( const renderer::rhi::ITexture& texture ) -> void {
        // Auto-sized tooltip windows need content-sized columns, rather than
        // stretch columns whose width depends on the window being measured.
        if ( !ImGui::BeginTable( "TextureProperties", 2, ImGuiTableFlags_SizingFixedFit ) ) {
            return;
        }

        const auto row{ []( const char* label, const eastl::string& value ) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled( "%s", label );
            ImGui::TableNextColumn();
            ImGui::TextUnformatted( value.c_str() );
        } };
        const auto samples{ 1u << core::as<core::u32>( texture.GetSampleCount() ) };
        row( MKT_LOC( "inspector_dimensions" ).c_str(), string::Format( "{} x {}", texture.GetWidth(), texture.GetHeight() ) );
        row( MKT_LOC( "inspector_format" ).c_str(), renderer::rhi::GetFormatInfo( texture.GetFormat() ).mName );
        row( MKT_LOC( "inspector_mip_levels" ).c_str(), string::Format( "{}", texture.GetMipLevelCount() ) );
        // Storage counts compressed blocks, all mips and cube faces. Driver
        // allocation alignment is backend-dependent, so label this an estimate.
        row( MKT_LOC( "inspector_gpu_data_estimated" ).c_str(), string::Format( "{:.2f} MiB", texture.GetStorageSizeBytes() * core::as<core::f64>( samples ) / ( 1024.0 * 1024.0 ) ) );
        row( "MSAA", samples == 1 ? eastl::string{ MKT_LOC( "inspector_off" ).c_str() } : string::Format( "{}x", samples ) );
        ImGui::EndTable();
    }

    static auto ShowTextureHoverTooltip( const renderer::rhi::ITexture* texture ) -> void {
        MKT_BEGIN_PROFILER();
        // A tooltip can own an auto-sized platform viewport. Measure its parent
        // before BeginTooltip so the preview cannot resize its own size limit.
        const ImVec2 available{ ImGui::GetWindowViewport()->WorkSize };

        if ( !imgui::widget::BeginItemTooltip() ) {
            return;
        }

        if ( texture ) {
            // Fit the longest edge while preserving rectangular texture proportions.
            const core::f32 edge{ eastl::min( ImGui::GetFontSize() * 16.0f,
                eastl::min( available.x * 0.5f, available.y * 0.4f ) ) };
            const core::f32 longest{ core::as<core::f32>( eastl::max( texture->GetWidth(), texture->GetHeight() ) ) };

            if ( longest > 0.0f ) {
                ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( texture ),
                    { edge * texture->GetWidth() / longest, edge * texture->GetHeight() / longest } );
            }

            ImGui::Separator();
            DrawTextureProperties( *texture );
        }
        else {
            ImGui::TextUnformatted( MKT_LOC( "inspector_click_me_to_load_a_texture" ).c_str() );
        }

        imgui::widget::EndItemTooltip();
    }

    static auto UpdateMaterialTexture( material::SurfaceMaterial& standardMat, material::MapType mapType ) -> void {
        MKT_BEGIN_PROFILER();
        const std::initializer_list<filesystem::FileDialogPair> filters{
            { MKT_LOC( "inspector_textures" ).c_str(), "jpg,jpeg,png" },
            { "JPG", "jpg" },
            { "JPEG", "jpeg" },
            { "PNG", "png" }
        };

        const filesystem::Path path{ filesystem::OpenFileDialog( filters ) };

        if ( !path.IsEmpty() ) {
            auto texture{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D ) };

            if ( texture.IsReady() ) {
                standardMat.SetTexture( mapType, texture );
            }
        }
    }

    static auto LoadMaterialTexture( material::SkyboxMaterial& skyboxMat, material::SkyboxFace face ) -> void {
        MKT_BEGIN_PROFILER();
        const std::initializer_list<filesystem::FileDialogPair> filters{
            { MKT_LOC( "inspector_textures" ).c_str(), "jpg,jpeg,png" },
            { "JPG", "jpg" },
            { "JPEG", "jpeg" },
            { "PNG", "png" }
        };

        const filesystem::Path path{ filesystem::OpenFileDialog( filters ) };

        if ( !path.IsEmpty() ) {
            auto textureHandle{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D ) };

            if ( textureHandle.IsReady() ) {
                skyboxMat.SetFace( face, textureHandle );
            }
        }
    }

    static auto LoadMaterialTexture( material::SkyboxMaterial& skyboxMat ) -> void {
        MKT_BEGIN_PROFILER();
        const std::initializer_list<filesystem::FileDialogPair> filters{
            { MKT_LOC( "inspector_textures" ).c_str(), "jpg,jpeg,png,hdr,ktx2" },
            { "JPG", "jpg" },
            { "JPEG", "jpeg" },
            { "PNG", "png" },
            { "HDR", "hdr" }
        };

        const filesystem::Path path{ filesystem::OpenFileDialog( filters ) };

        if ( !path.IsEmpty() ) {
            auto textureHandle{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D ) };

            if ( textureHandle.IsReady() ) {
                skyboxMat.SetEquirectangular( textureHandle );
            }
        }
    }

    static auto DisplayTextureEditTreeNode( const eastl::string_view title, material::SurfaceMaterial& standardMat, const eastl::function<void( material::SurfaceMaterial& standardMat )>& func ) -> void {
        MKT_BEGIN_PROFILER();
        constexpr ImGuiTreeNodeFlags treeNodeFlags{
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowOverlap
        };

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const eastl::string nodeLabel{ string::Format( "##{}:{}", "DisplayTextureEditTreeNode", title.data() ) };

        if ( ImGui::TreeNodeEx( nodeLabel.c_str(), treeNodeFlags, "%s", title.data() ) ) {
            // The header uses NoTreePushOnOpen, so explicitly scope its contents.
            // TreePop must only undo this matching TreePush.
            ImGui::TreePush( "##Body" );
            func( standardMat );
            ImGui::TreePop();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
    }

    static auto DrawMaterialTextureSlot( material::SurfaceMaterial& material, material::MapType type, const char* label ) -> void {
        ImGui::PushID( core::as<core::i32>( type ) );
        ImGui::TextUnformatted( label );
        imgui::ImGuiScopedStyleVar border{ ImGuiStyleVar_FrameBorderSize, 0.0f };
        imgui::ImGuiScopedStyleVar cellPadding{ ImGuiStyleVar_CellPadding, ImVec2{ 0.0f, 3.0f } };
        const float spacing{ ImGui::GetStyle().ItemSpacing.x };
        const float previewSize{ ImGui::GetFrameHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y };

        // Keep texture actions in their own row; factors below can use the full panel width.
        if ( ImGui::BeginTable( "TextureSlot", 3, ImGuiTableFlags_SizingStretchProp ) ) {
            ImGui::TableSetupColumn( "Preview", ImGuiTableColumnFlags_WidthFixed, previewSize + spacing );
            ImGui::TableSetupColumn( "Texture", ImGuiTableColumnFlags_WidthStretch );
            ImGui::TableSetupColumn( "Clear", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight() + spacing );
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            auto texture{ material.GetTexture( type ) };
            const auto path{ material.GetTexturePath( type ) };
            const bool assigned{ texture || !path.IsEmpty() };
            const auto preview{ texture ? texture : asset::AssetRegistryService::Get()->GetDummyTexture() };
            bool choose{};
            {
                imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_FramePadding, ImVec2{ 2.0f, 2.0f } };
                choose = imgui::PushImageButton( "Preview", imgui::ImGuiService::Get()->GetTextureID( preview ),
                    ImVec2{ previewSize - 4.0f, previewSize - 4.0f } );
            }

            const auto acceptTextureDrop{ [&]() {
                if ( ImGui::BeginDragDropTarget() ) {
                    if ( const auto* payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_IMAGE" ) } ) {
                        const filesystem::Path droppedPath{ static_cast<cstr>( payload->Data ) };
                        auto loaded{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( droppedPath, renderer::rhi::TextureDimension::eTexture2D ) };

                        if ( loaded.IsReady() ) {
                            material.SetTexture( type, loaded );
                        }
                    }

                    ImGui::EndDragDropTarget();
                }
            } };
            acceptTextureDrop();
            ShowTextureHoverTooltip( texture.GetPtr() );

            ImGui::TableSetColumnIndex( 1 );
            const eastl::string caption{ path.IsEmpty()
                                             ? MKT_LOC( assigned ? "material_editor_replace" : "material_editor_choose_texture" )
                                             : path.GetFilename() };
            const auto buttonLabel{ string::Format( "{}###Choose", caption ) };
            choose |= ImGui::Button( buttonLabel.c_str(), ImVec2{ -1.0f, 0.0f } );
            acceptTextureDrop();

            if ( !path.IsEmpty() ) {
                imgui::widget::ItemTooltip( "%s", path.GetC_Str() );
            }

            bool use{ material.IsTextureUsed( type ) };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_use_texture", "UseTexture" ).c_str(), use ) ) {
                material.UseTexture( type, use );
            }

            ImGui::TableSetColumnIndex( 2 );
            ImGui::SetCursorPosX( ImGui::GetCursorPosX() + spacing );
            ImGui::BeginDisabled( !assigned );

            if ( imgui::widget::IconButton( "Clear", ICON_MD_CLOSE, { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() } ) ) {
                material.RemoveTexture( type );
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "inspector_remove_texture" ).c_str() );
            ImGui::EndDisabled();
            ImGui::EndTable();

            if ( choose ) {
                UpdateMaterialTexture( material, type );
            }
        }

        ImGui::PopID();
    }

    static auto DrawMaterialFactor( const char* key, float& value, float maximum = 1.0f ) -> bool {
        ImGui::PushID( key );
        ImGui::TextUnformatted( MKT_LOC( key ).c_str() );
        ImGui::SetNextItemWidth( -1.0f );
        const bool changed{ ImGui::SliderFloat( "##Value", &value, 0.0f, maximum, "%.2f" ) };
        ImGui::PopID();
        return changed;
    }

    static auto EditDiffuseProperties( material::SurfaceMaterial& material ) -> void {
        DrawMaterialTextureSlot( material, material::MapType::eDiffuse, MKT_LOC( "inspector_diffuse" ).c_str() );
        ImGui::TextUnformatted( MKT_LOC( "inspector_diffuse_factor" ).c_str() );
        auto color{ material.GetDiffuseFactor() };
        ImGui::SetNextItemWidth( -1.0f );

        if ( ImGui::ColorEdit4( "##DiffuseFactor", glm::value_ptr( color ), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview ) ) {
            material.SetDiffuseFactor( color );
        }
    }

    static auto EditBaseColorProperties( material::SurfaceMaterial& material ) -> void {
        DrawMaterialTextureSlot( material, material::MapType::eBaseColor, MKT_LOC( "inspector_albedo" ).c_str() );
        ImGui::TextUnformatted( MKT_LOC( "inspector_color" ).c_str() );
        auto color{ material.GetBaseColorFactor() };
        ImGui::SetNextItemWidth( -1.0f );

        if ( ImGui::ColorEdit4( "##BaseColorFactor", glm::value_ptr( color ), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview ) ) {
            material.SetBaseColorFactor( color );
        }

        float cutOff{ material.GetAlphaMaskCutoff() };

        if ( DrawMaterialFactor( "inspector_alpha_cut_off", cutOff ) ) {
            material.SetAlphaMaskCutoff( cutOff );
        }

        const auto alpha{ material.GetAlphaMask() };
        std::array<std::string, core::as<core::usize>( material::AlphaMode::eCount )> alphaChoices{
            MKT_LOC( "inspector_opaque" ).c_str(),
            MKT_LOC( "inspector_mask" ).c_str(),
            MKT_LOC( "inspector_blend" ).c_str()
        };
        ImGui::PushID( "AlphaMode" );
        ImGui::SetNextItemWidth( -1.0f );
        const auto newAlpha{ imgui::Combo( alphaChoices, alpha ) };

        if ( newAlpha != alpha ) {
            material.SetAlphaMask( newAlpha );
        }

        ImGui::PopID();

        const auto workflow{ material.GetWorkflow() };
        std::array<std::string, core::as<core::usize>( material::Workflow::eCount )> workflowChoices{
            MKT_LOC( "inspector_metallic_roughness" ).c_str(),
            MKT_LOC( "inspector_specular_glossiness" ).c_str()
        };
        ImGui::PushID( "Workflow" );
        ImGui::SetNextItemWidth( -1.0f );
        const auto newWorkflow{ imgui::Combo( workflowChoices, workflow ) };

        if ( newWorkflow != workflow ) {
            material.SetWorkflow( newWorkflow );
        }

        ImGui::PopID();

        ImGui::Spacing();
        EditDiffuseProperties( material );

        if ( material.GetWorkflow() == material::Workflow::eSpecularGlossiness ) {
            DrawMaterialTextureSlot( material, material::MapType::eSpecularGlossiness, MKT_LOC( "inspector_specular_glossiness" ).c_str() );
        }
    }

    static auto EditMetallicProperties( material::SurfaceMaterial& material ) -> void {
        DrawMaterialTextureSlot( material, material::MapType::eMetallic, MKT_LOC( "inspector_metallic" ).c_str() );
        float factor{ material.GetMetallicFactor() };

        if ( DrawMaterialFactor( "inspector_metal_factor", factor ) ) {
            material.SetMetallicFactor( factor );
        }

        ImGui::Spacing();
        DrawMaterialTextureSlot( material, material::MapType::eMetallicRoughness, MKT_LOC( "inspector_metallic_roughness" ).c_str() );
    }

    static auto EditNormalTexture( material::SurfaceMaterial& material, material::MapType type, const char* label ) -> void {
        DrawMaterialTextureSlot( material, type, label );
    }

    static auto EditNormalsProperties( material::SurfaceMaterial& material ) -> void {
        float strength{ material.GetNormalScale() };

        if ( ImGui::DragFloat( "Normal strength", &strength, 0.01f, 0.0f, 10.0f ) ) {
            material.SetNormalScale( strength );
        }

        ImGui::PushID( "BaseNormal" );
        EditNormalTexture( material, material::MapType::eNormal, MKT_LOC( "inspector_normal" ).c_str() );
        ImGui::PopID();

        if ( material.GetShadingModel() == material::ShadingModel::eClearCoat ) {
            ImGui::Separator();
            ImGui::PushID( "ClearCoatNormal" );
            EditNormalTexture( material, material::MapType::eClearCoatNormal, MKT_LOC( "inspector_clear_coat_normal" ).c_str() );
            float scale{ material.GetClearCoatNormalScale() };

            if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_normal_scale", "Normal scale" ).c_str(), &scale, 0.01f, -10.0f, 10.0f ) ) {
                material.SetClearCoatNormalScale( scale );
            }

            int uv{ material.GetClearCoatNormalTextureSet() };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_uv_set", "UV set" ).c_str(), &uv, "UV0\0UV1\0" ) ) {
                material.SetClearCoatNormalTextureSet( uv );
            }

            ImGui::PopID();
        }
    }

    static auto EditEmissionProperties( material::SurfaceMaterial& material ) -> void {
        DrawMaterialTextureSlot( material, material::MapType::eEmissive, MKT_LOC( "inspector_emission" ).c_str() );

        if ( ImGui::BeginTable( "##EmissionProperties", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings ) ) {
            ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "inspector_factors" ).c_str() );
            ImGui::TableSetColumnIndex( 1 );
            auto factors{ material.GetEmissiveFactor() };
            ImGui::SetNextItemWidth( -1.0f );

            if ( ImGui::ColorEdit3( "##EmissiveFactor", glm::value_ptr( factors ) ) ) {
                material.SetEmissiveFactor( factors );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "inspector_strength" ).c_str() );
            ImGui::TableSetColumnIndex( 1 );
            float strength{ material.GetEmissiveStrength() };
            ImGui::SetNextItemWidth( -1.0f );

            if ( ImGui::SliderFloat( "##EmissiveStrength", &strength, 0.0f, 10.0f, "%.2f" ) ) {
                material.SetEmissiveStrength( strength );
            }

            ImGui::EndTable();
        }

        bool bloom{ material.IsBloomy() };

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_enable_bloom", "Bloom" ).c_str(), bloom ) ) {
            material.EnableBloom( bloom );
        }
    }

    static auto EditRoughnessProperties( material::SurfaceMaterial& material ) -> void {
        DrawMaterialTextureSlot( material, material::MapType::eRoughness, MKT_LOC( "inspector_roughness" ).c_str() );
        float factor{ material.GetRoughnessFactor() };

        if ( DrawMaterialFactor( "inspector_roughness", factor ) ) {
            material.SetRoughnessFactor( factor );
        }
    }

    static auto EditAmbientOcclusionProperties( material::SurfaceMaterial& material ) -> void {
        DrawMaterialTextureSlot( material, material::MapType::eAmbientOcclusion, MKT_LOC( "inspector_ambient_occlusion" ).c_str() );
        float factor{ material.GetAoFactor() };

        if ( DrawMaterialFactor( "inspector_ao_factor", factor, 10.0f ) ) {
            material.SetAoFactor( factor );
        }
    }

    static auto EditMaterial( scene::MaterialComponent& component, bool sandbox ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto* material{ core::checked_cast<material::SurfaceMaterial*>( component.GetMaterial().GetPtr() ) };

        if ( material == nullptr ) {
            return;
        }

        // Share a label column across the header and UV properties, including translated labels.
        const auto& style{ ImGui::GetStyle() };
        const float actionSize{ ImGui::GetFrameHeight() };
        const float rowWidth{ glm::max( 1.0f, ImGui::GetContentRegionAvail().x - style.ItemSpacing.x ) };
        const ImVec2 cellPadding{ style.ItemSpacing.x * 0.5f, style.CellPadding.y };
        float labelWidth{};

        for ( const char* label: { MKT_LOC( "inspector_material_name" ).c_str(), MKT_LOC( "inspector_shading_model" ).c_str(), "UV tiling", "UV offset" } ) {
            labelWidth = glm::max( labelWidth, ImGui::CalcTextSize( label ).x );
        }

        const material::ShadingModel currentShadingModel{ material->GetShadingModel() };
        auto newShadingModel{ currentShadingModel };
        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, cellPadding );

        if ( ImGui::BeginTable( "##MaterialHeader", 2,
                 ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX,
                 { rowWidth, 0.0f } ) ) {
            ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, labelWidth );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "inspector_material_name" ).c_str() );

            ImGui::TableNextColumn();
            // The name yields space to both actions while preserving the right gutter.
            ImGui::SetNextItemWidth( glm::max( 1.0f, ImGui::GetContentRegionAvail().x - 2.0f * ( actionSize + style.ItemSpacing.x ) ) );
            ( void )InputText( material->GetName(), true );

            ImGui::SameLine();

            if ( imgui::widget::IconButton( "MakeUniqueMaterial", ICON_MD_CONTENT_COPY, { actionSize, actionSize } ) ) {
                component.SetMaterial( material::SurfaceMaterialHandle::New( *material ) );
                component.SetIsInternal( false );
                material = core::checked_cast<material::SurfaceMaterial*>( component.GetMaterial().GetPtr() );
            }

            imgui::SetCursorHandOnLastItemHovered();
            imgui::widget::TooltipText( MKT_LOC( "inspector_create_an_independent_material_for_this_entity_otherwise_edits_affect" ) );

            ImGui::SameLine();
            // Imported materials become saveable after making an independent copy.
            ImGui::BeginDisabled( component.IsInternal() || sandbox );

            if ( imgui::widget::IconButton( "SaveMaterial", ICON_MD_SAVE_ALT, { actionSize, actionSize } ) ) {
                const std::initializer_list<filesystem::FileDialogPair> filters{
                    { MKT_LOC( "inspector_material" ).c_str(), filesystem::kMikotoMaterialExtension.data() }
                };
                filesystem::Path materialPath{ SaveFileDialog( material->GetName(), filters ) };

                if ( !materialPath.IsEmpty() ) {
                    ( void )asset::AssetRegistryService::Get()->SerializeMaterial( material::MaterialHandle::Create( material ), materialPath );
                }
            }

            imgui::SetCursorHandOnLastItemHovered();
            ImGui::EndDisabled();
            imgui::widget::TooltipText( MKT_LOC( "inspector_save_material" ) );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "inspector_shading_model" ).c_str() );

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth( -1.0f );
            eastl::array<std::string, core::as<core::usize>( material::ShadingModel::eCount )> choices{
                MKT_LOC( "inspector_default_pbr" ).c_str(),
                MKT_LOC( "inspector_clear_coat" ).c_str(),
                MKT_LOC( "inspector_toon_shading" ).c_str(),
                MKT_LOC( "inspector_flat_shading" ).c_str(),
                MKT_LOC( "inspector_cell_shading" ).c_str(),
                MKT_LOC( "inspector_subsurface_scattering" ).c_str(),
                MKT_LOC( "material_dielectric" ).c_str(),
                MKT_LOC( "material_conductor" ).c_str(),
                MKT_LOC( "material_unlit" ).c_str()
            };
            newShadingModel = Combo( choices, currentShadingModel );

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        if ( newShadingModel != currentShadingModel ) {
            material->SetShadingModel( newShadingModel );
        }

        ImGui::SeparatorText( MKT_LOC( "inspector_properties" ).c_str() );

        {
            auto settings{ material->GetSurfaceShading() };
            const auto model{ material->GetShadingModel() };
            bool edited{};

            if ( model == material::ShadingModel::eClearCoat ) {
                edited |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_coat_weight", "Coat weight" ).c_str(), &settings.mCoatWeight, 0.0f, 1.0f );
                edited |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_coat_roughness", "Coat roughness" ).c_str(), &settings.mCoatRoughness, 0.045f, 1.0f );
            }
            else if ( model == material::ShadingModel::eToonShading ) {
                edited |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_lighting_threshold", "Lighting threshold" ).c_str(), &settings.mToonThreshold, 0.0f, 1.0f );
                edited |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_threshold_softness", "Threshold softness" ).c_str(), &settings.mToonSoftness, 0.001f, 1.0f );
            }
            else if ( model == material::ShadingModel::eCellShading ) {
                edited |= ImGui::SliderInt( imgui::widget::LocalizedLabel( "inspector_lighting_bands", "Lighting bands" ).c_str(), &settings.mCellBands, 2, 16 );
            }
            else if ( model == material::ShadingModel::eFlatShading ) {
                ImGui::TextWrapped( MKT_LOC( "inspector_uses_triangle_normals_normal_maps_are_ignored" ).c_str() );
            }

            if ( model == material::ShadingModel::eToonShading || model == material::ShadingModel::eCellShading ) {
                edited |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_contour_width_px", "Contour width (px)" ).c_str(), &settings.mContourWidth, 0.0f, 12.0f );

                if ( ImGui::IsItemHovered() ) {
                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_opaque_scene_silhouettes_zero_disables_contours_not_used_by_material" ).c_str() );
                }

                edited |= ImGui::ColorEdit3( imgui::widget::LocalizedLabel( "inspector_contour_color", "Contour color" ).c_str(), &settings.mContourColor.x );
            }

            if ( edited ) {
                material->SetSurfaceShading( settings );
            }
        }

        auto uvTransform{ material->GetUvTransform() };
        bool uvChanged{};
        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, cellPadding );

        if ( ImGui::BeginTable( "##MaterialUV", 2,
                 ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX,
                 { rowWidth, 0.0f } ) ) {
            ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, labelWidth );
            ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( "UV tiling" );
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth( -1.0f );
            uvChanged |= ImGui::DragFloat2( "##UVTiling", &uvTransform.x, 0.01f );

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( "UV offset" );
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth( -1.0f );
            uvChanged |= ImGui::DragFloat2( "##UVOffset", &uvTransform.z, 0.01f );

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        if ( uvChanged ) {
            material->SetUvTransform( uvTransform );
        }

        bool doubleSided{ material->IsDoubleSided() };

        if ( imgui::CheckBox( "Double sided", doubleSided ) ) {
            material->SetIsDoubleSided( doubleSided );
        }

        DrawMaterialOptics( *material );

        if ( newShadingModel == material::ShadingModel::eSubsurfaceScattering &&
             ImGui::CollapsingHeader( MKT_LOC( "material_raster_diffusion" ).c_str() ) ) {
            auto settings{ material->GetSubsurface() };
            bool changed{ ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_scatter_scale", "Scatter scale" ).c_str(), &settings.mScatterScale, 0.0f, 10.0f ) };

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_screen_space_sigma_rgb_profile_width_x_scatter_scale_x" ).c_str() );
            }

            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_subsurface_weight", "Subsurface weight" ).c_str(), &settings.mStrength, 0.0f, 1.0f );
            changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "inspector_near_sigma_rgb", "Near sigma RGB" ).c_str(), &settings.mNear.x, 0.005f, 0.001f, 10.0f );
            changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "inspector_far_sigma_rgb", "Far sigma RGB" ).c_str(), &settings.mFar.x, 0.005f, 0.001f, 10.0f );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_near_far_balance", "Near / far balance" ).c_str(), &settings.mNearWeight, 0.0f, 1.0f );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_0_wide_far_profile_1_tight_near_profile" ).c_str() );
            }

            int radius{ ( settings.mSamples - 1 ) / 2 };

            if ( ImGui::SliderInt( imgui::widget::LocalizedLabel( "inspector_sample_radius_px", "Sample radius (px)" ).c_str(), &radius, 3, 32 ) ) {
                settings.mSamples = radius * 2 + 1;
                changed = true;
            }

            ImGui::TextDisabled( MKT_LOC( "inspector_taps_per_axis" ).c_str(), settings.mSamples );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_depth_edge_threshold", "Depth edge threshold" ).c_str(), &settings.mDepthRejection, 0.0f, 0.2f, "%.3f" );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_relative_view_depth_change_before_rejecting_blur_samples_zero_disables" ).c_str() );
            }

            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_transmission_weight", "Transmission weight" ).c_str(), &settings.mTranslucency, 0.0f, 1.0f );
            changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_transmission_thickness", "Transmission thickness" ).c_str(), &settings.mThickness, 0.01f, 0.0f, 100.0f );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_artist_authored_thickness_in_profile_units_larger_values_absorb_more" ).c_str() );
            }

            changed |= ImGui::ColorEdit3( imgui::widget::LocalizedLabel( "inspector_scattering_weight_rgb", "Scattering weight RGB" ).c_str(), &settings.mColor.x, ImGuiColorEditFlags_Float );
            changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_scattering_suppression", "Scattering suppression" ).c_str(), &settings.mOpacity, 0.0f, 1.0f );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_0_full_scattering_influence_1_ordinary_surface_lighting_does_not" ).c_str() );
            }

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_reset_scattering", "Reset scattering" ).c_str() ) ) {
                settings = material::SubsurfaceSettings{};
                changed = true;
            }

            ImGui::TextWrapped( MKT_LOC( "inspector_diffuse_only_horizontal_and_vertical_blur_specular_ambient_and_emission" ).c_str() );

            if ( changed ) {
                material->SetSubsurface( settings );
            }

            ImGui::TextWrapped( MKT_LOC( "inspector_raster_opaque_masked_surfaces_including_msaa_path_tracing_uses_its" ).c_str() );
        }

        switch ( newShadingModel ) {
            case material::ShadingModel::eDielectric:
            case material::ShadingModel::eConductor:
            case material::ShadingModel::eUnlit:
            case material::ShadingModel::eSubsurfaceScattering:
            case material::ShadingModel::eDefaultPbr:
            case material::ShadingModel::eClearCoat:
            case material::ShadingModel::eToonShading:
            case material::ShadingModel::eFlatShading:
            case material::ShadingModel::eCellShading:
                DisplayTextureEditTreeNode( MKT_LOC( "inspector_base_color" ).c_str(), *material, EditBaseColorProperties );

                if ( newShadingModel != material::ShadingModel::eDielectric && newShadingModel != material::ShadingModel::eConductor && newShadingModel != material::ShadingModel::eUnlit ) {
                    DisplayTextureEditTreeNode( MKT_LOC( "inspector_metal_ness" ).c_str(), *material, EditMetallicProperties );
                }

                if ( newShadingModel != material::ShadingModel::eUnlit ) {
                    DisplayTextureEditTreeNode( MKT_LOC( "inspector_roughness" ).c_str(), *material, EditRoughnessProperties );
                    DisplayTextureEditTreeNode( MKT_LOC( "inspector_ambient_occlusion" ).c_str(), *material, EditAmbientOcclusionProperties );
                    DisplayTextureEditTreeNode( MKT_LOC( "inspector_normals" ).c_str(), *material, EditNormalsProperties );
                }
                DisplayTextureEditTreeNode( MKT_LOC( "inspector_emission" ).c_str(), *material, EditEmissionProperties );
                break;
            default:;
        }
    }

    static auto DrawComponentButton( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        imgui::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        imgui::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        const ImVec2 buttonSize{ ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() + 8.0f };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_add_component", "Add component" ).c_str(), buttonSize ) ) {
            ImGui::OpenPopup( "AddComponentButtonPopup" );
        }

        if ( ImGui::BeginPopup( "AddComponentButtonPopup" ) ) {
            DrawAddComponentMenu( entity );
            ImGui::EndPopup();
        }
    }

    static auto MeshInfoRow( core::cstr labelKey, const eastl::string& value ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextWrapped( "%s", MKT_LOC( labelKey ).c_str() );

        ImGui::TableNextColumn();
        const core::f32 width{ ImGui::CalcTextSize( value.c_str() ).x };
        const core::f32 available{ ImGui::GetContentRegionAvail().x };

        if ( width <= available ) {
            ImGui::SetCursorPosX( ImGui::GetCursorPosX() + available - width );
            ImGui::TextUnformatted( value.c_str() );
        }
        else {
            ImGui::TextWrapped( "%s", value.c_str() );
        }
    }

    static auto BeginMeshInfoTable( core::cstr id ) -> bool {
        if ( !ImGui::BeginTable( id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings ) ) {
            return false;
        }

        ImGui::TableSetupColumn( "##Label", ImGuiTableColumnFlags_WidthStretch, 3.0f );
        ImGui::TableSetupColumn( "##Value", ImGuiTableColumnFlags_WidthStretch, 2.0f );
        return true;
    }

    static auto DrawImportedMeshTransform( const core::float4x4& transform ) -> void {
        if ( ImGui::BeginTable( "ImportedTransform", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame ) ) {
            for ( core::usize row{}; row < 4; ++row ) {
                ImGui::TableNextRow();

                for ( core::usize column{}; column < 4; ++column ) {
                    ImGui::TableNextColumn();
                    ImGui::Text( "%.3f", transform[column][row] );
                }
            }

            ImGui::EndTable();
        }
    }

    static auto DrawVec3Transform( const std::string_view label, glm::vec3& data, const float resetValue = 0.0f ) -> bool {
        MKT_BEGIN_PROFILER();
        const glm::vec3 original{ data };
        ImGui::PushID( label.data(), label.data() + label.size() );
        ImGui::BeginGroup();

        const float available{ ImGui::GetContentRegionAvail().x };
        const float startX{ ImGui::GetCursorPosX() };
        const float labelWidth{ glm::min( 100.0f, available * 0.27f ) };
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( label.data(), label.data() + label.size() );
        // Narrow inspectors put the label above the controls instead of clipping Z.
        if ( available >= 340.0f ) {
            ImGui::SameLine( startX + labelWidth );
        }

        constexpr float innerGap{ 2.0f };
        constexpr float axisGap{ 6.0f };
        const float buttonWidth{ ImGui::GetFontSize() + 6.0f };
        const float axisWidth{ ( ImGui::GetContentRegionAvail().x - axisGap * 2.0f ) / 3.0f };
        const float inputWidth{ glm::max( 1.0f, axisWidth - buttonWidth - innerGap ) };
        constexpr const char* axes[]{ "X", "Y", "Z" };
        const ImVec4 colors[]{ { 0.48f, 0.16f, 0.16f, 1.0f }, { 0.18f, 0.38f, 0.16f, 1.0f }, { 0.16f, 0.24f, 0.46f, 1.0f } };
        const ImVec4 hoveredColors[]{ { 0.58f, 0.22f, 0.22f, 1.0f }, { 0.24f, 0.48f, 0.22f, 1.0f }, { 0.22f, 0.31f, 0.56f, 1.0f } };

        for ( int axis{}; axis < 3; ++axis ) {
            if ( axis != 0 ) {
                ImGui::SameLine( 0.0f, axisGap );
            }

            ImGui::PushID( axis );
            ImGui::PushStyleColor( ImGuiCol_Button, colors[axis] );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, hoveredColors[axis] );
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, colors[axis] );

            // Leave the compact button's full width available for centering its axis letter.
            // https://github.com/ocornut/imgui/blob/v1.92.9b-docking/imgui_widgets.cpp (ButtonEx)
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 0.0f, ImGui::GetStyle().FramePadding.y } );

            if ( ImGui::Button( axes[axis], { buttonWidth, ImGui::GetFrameHeight() } ) ) {
                data[axis] = resetValue;
            }

            ImGui::PopStyleVar();
            imgui::SetCursorHandOnLastItemHovered();
            ImGui::PopStyleColor( 3 );
            ImGui::SameLine( 0.0f, innerGap );
            ImGui::SetNextItemWidth( inputWidth );
            ImGui::DragFloat( "##value", &data[axis], 0.1f, 0.0f, 0.0f, "%.2f" );
            ImGui::PopID();
        }

        ImGui::EndGroup();
        ImGui::PopID();
        return data != original;
    }

    static auto DrawVisibilityCheckBox( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( entity == nullptr ) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        scene::TagComponent& tagComponent{ entity->GetComponent<scene::TagComponent>() };

        bool isActive{ tagComponent.IsActive() };

        if ( imgui::CheckBox( "##TagCompCheckbox", isActive ) ) {
            tagComponent.SetActive( isActive );
        }
    }

    static auto BeginLightPropertiesTable( core::cstr id ) -> bool {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX };

        if ( !ImGui::BeginTable( id, 2, tableFlags ) ) {
            return false;
        }

        ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed );
        ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
        return true;
    }

    static auto LightPropertyRow( core::cstr labelKey, core::cstr helpKey = nullptr ) -> void {
        ImGui::Spacing();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( MKT_LOC( labelKey ).c_str() );

        if ( helpKey ) {
            ImGui::SameLine();
            imgui::widget::MakeHelpPopUp( MKT_LOC( helpKey ).c_str() );
        }

        ImGui::TableSetColumnIndex( 1 );
        ImGui::SetNextItemWidth( -1.0f );
    }

    static auto DrawPointLightOptions( scene::LightComponent& lightComponent ) -> void {
        MKT_BEGIN_PROFILER();

        if ( BeginLightPropertiesTable( "PointLightMainTable" ) ) {
            auto& pointLightData{ lightComponent.Get<renderer::PointLight>() };

            LightPropertyRow( "inspector_color" );
            glm::vec3 diffuseComponent{ pointLightData.GetColor() };

            if ( imgui::ColorEdit3( "##PointLightColor", diffuseComponent ) ) {
                pointLightData.SetColor( diffuseComponent );
            }

            LightPropertyRow( "inspector_intensity" );
            float intensity{ pointLightData.GetIntensity() };

            if ( imgui::Slider( "##PointLightIntensity", intensity, { 1.0f, 1000.0f } ) ) {
                pointLightData.SetIntensity( intensity );
            }

            LightPropertyRow( "inspector_source_radius", "inspector_source_radius_help" );
            float sourceRadius{ pointLightData.GetSourceRadius() };

            if ( ImGui::DragFloat( "##PointSourceRadius", &sourceRadius, 0.01f, 0.0f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp ) ) {
                pointLightData.SetSourceRadius( sourceRadius );
            }

            LightPropertyRow( "inspector_radius" );
            float radius{ pointLightData.GetRadius() };

            if ( imgui::Slider( "##PointLightRadius", radius, { 1.0f, 500.0f } ) ) {
                pointLightData.SetRadius( radius );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            LightPropertyRow( "inspector_cast_shadows" );
            bool castShadows{ pointLightData.IsShadowCaster() };

            if ( imgui::CheckBox( "##PointLightShadows", castShadows ) ) {
                pointLightData.SetIsShadowCaster( castShadows );
            }

            ImGui::EndTable();
        }
    }

    static auto DrawSpotLightLightOptions( scene::LightComponent& lightComponent ) -> void {
        MKT_BEGIN_PROFILER();

        if ( BeginLightPropertiesTable( "SpotLightEditTable" ) ) {
            auto& spotLightData{ lightComponent.Get<renderer::SpotLight>() };

            LightPropertyRow( "inspector_direction", "inspector_the_spot_position_is_determined_by_the_objects_position" );
            glm::vec3 direction{ spotLightData.GetDirection() };

            if ( DragFloat3( "##SpotLightDirection", "%.2f", direction, 0.01f, -math::constants::kPi, math::constants::kPi ) ) {
                spotLightData.SetDirection( direction );
            }

            LightPropertyRow( "inspector_color" );
            core::float3 color{ spotLightData.GetColor() };

            if ( imgui::ColorEdit3( "##SpotLightColor", color ) ) {
                spotLightData.SetColor( color );
            }

            imgui::SetCursorHandOnLastItemHovered();

            LightPropertyRow( "inspector_intensity" );
            float intensity{ spotLightData.GetIntensity() };

            if ( imgui::Slider( "##SpotLightIntensity", intensity, { 1.0f, 1000.0f } ) ) {
                spotLightData.SetIntensity( intensity );
            }

            imgui::SetCursorHandOnLastItemHovered();

            LightPropertyRow( "inspector_attenuation_radius" );
            float radius{ spotLightData.GetRadius() };

            if ( imgui::Slider( "##SpotLightRadius", radius, { 1.0f, 500.0f } ) ) {
                spotLightData.SetRadius( radius );
            }

            imgui::SetCursorHandOnLastItemHovered();

            LightPropertyRow( "inspector_angle", "inspector_outer_cone_half_angle_in_degrees_full_cone_width_is" );
            float angle{ spotLightData.GetAngle() };

            if ( imgui::Slider( "##SpotLightAngle", angle, { 1.0f, renderer::SpotLight::GetMaxAngle() } ) ) {
                spotLightData.SetAngle( angle );
            }

            imgui::SetCursorHandOnLastItemHovered();

            LightPropertyRow( "inspector_softness", "inspector_width_of_the_fade_inside_the_outer_cone_in_degrees" );
            float softness{ spotLightData.GetSoftness() };

            if ( imgui::Slider( "##SpotLightSoftness", softness, { 0.0f, renderer::SpotLight::GetMaxSoftness() } ) ) {
                spotLightData.SetSoftness( softness );
            }

            LightPropertyRow( "inspector_cast_shadows" );
            bool castShadows{ spotLightData.IsShadowCaster() };

            if ( imgui::CheckBox( "##SpotLightShadows", castShadows ) ) {
                spotLightData.SetIsShadowCaster( castShadows );
            }

            ImGui::EndTable();
        }
    }

    static auto DrawDirectionalLightOptions( scene::LightComponent& lightComponent ) -> void {
        MKT_BEGIN_PROFILER();

        if ( BeginLightPropertiesTable( "DirectionalLightEditTable" ) ) {
            auto& direLightData{ lightComponent.Get<renderer::DirectionalLight>() };

            LightPropertyRow( "inspector_intensity" );
            float intensity{ direLightData.GetIntensity() };

            if ( imgui::Slider( "##Intensity", intensity, { 1.0f, 1000.0f } ) ) {
                direLightData.SetIntensity( intensity );
            }

            LightPropertyRow( "inspector_color" );
            glm::vec4 diffuse{ direLightData.GetColor(), 1.0f };

            if ( imgui::ColorEdit4( "##DirectionalLightDiffuse", diffuse ) ) {
                direLightData.SetColor( diffuse );
            }

            LightPropertyRow( "inspector_angular_radius", "inspector_angular_radius_help" );
            float angularRadius{ direLightData.GetAngularRadius() };

            if ( ImGui::DragFloat( "##DirectionalAngularRadius", &angularRadius, 0.01f, 0.0f, 10.0f, "%.3f deg", ImGuiSliderFlags_AlwaysClamp ) ) {
                direLightData.SetAngularRadius( angularRadius );
            }

            LightPropertyRow( "inspector_direction", "inspector_light_position_help" );
            constexpr float PI{ std::numbers::pi_v<float> };
            glm::vec3 direction{ direLightData.GetDirection() };

            if ( DragFloat3( "##DirectionalLightDirection", "%.2f", direction, 0.01f, -PI, PI ) ) {
                direLightData.SetDirection( direction );
            }

            LightPropertyRow( "inspector_cast_shadows" );
            bool castShadows{ direLightData.IsShadowCaster() };

            if ( imgui::CheckBox( "##DirectionalLightShadows", castShadows ) ) {
                direLightData.SetIsShadowCaster( castShadows );
            }

            ImGui::EndTable();
        }
    }

    static auto DrawScriptProperties( scene::ScriptComponent& component ) -> void {
        auto script{ component.GetHandle() };

        if ( !script || script->GetMetaData().mFieldLookup.empty() ) {
            return;
        }

        auto values{ nlohmann::json::parse( component.GetProperties().c_str(), nullptr, false ) };

        if ( !values.is_object() ) {
            values = nlohmann::json::object();
        }

        eastl::vector<const scripting::ScriptFieldMetadata*> fields{};

        for ( const auto& entry: script->GetMetaData().mFieldLookup ) {
            const auto& field{ entry.second };

            fields.push_back( &field );
        }

        eastl::sort( fields.begin(), fields.end(), []( const auto* a, const auto* b ) {
            return a->mName < b->mName;
        } );
        ImGui::SeparatorText( "Properties" );
        bool changed{};

        for ( const auto* field: fields ) {
            const auto* name{ field->mName.c_str() };
            auto value{ values.contains( name ) ? values[name] : nlohmann::json::parse( field->mDefaultJson.c_str(), nullptr, false ) };
            ImGui::PushID( name );
            ImGui::BeginDisabled( field->mReadOnly );
            bool edited{};
            using Type = scripting::ScriptValueType;

            if ( field->mType == Type::eBool ) {
                bool v{ value.is_boolean() && value.get<bool>() };
                edited = imgui::CheckBox( name, v );

                if ( edited ) {
                    value = v;
                }
            }
            else if ( field->mType == Type::eInt32 || field->mType == Type::eUInt32 || field->mType == Type::eEntity ) {
                if ( field->mType == Type::eInt32 ) {
                    core::i32 v{ value.is_number_integer() ? value.get<core::i32>() : 0 };
                    edited = ImGui::InputScalar( name, ImGuiDataType_S32, &v );

                    if ( edited ) {
                        value = v;
                    }
                }
                else {
                    core::u64 v{ value.is_number_unsigned() ? value.get<core::u64>() : 0 };
                    edited = ImGui::InputScalar( name, ImGuiDataType_U64, &v );

                    if ( field->mType == Type::eEntity && ImGui::BeginDragDropTarget() ) {
                        if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_ENTITY" ) }; payload && payload->DataSize == sizeof( core::u64 ) ) {
                            v = *static_cast<const core::u64*>( payload->Data );
                            edited = true;
                        }

                        ImGui::EndDragDropTarget();
                    }

                    if ( edited ) {
                        value = field->mType == Type::eUInt32 ? eastl::min( v, core::u64{ UINT32_MAX } ) : v;
                    }
                }
            }
            else if ( field->mType == Type::eFloat32 ) {
                float v{ value.is_number() ? value.get<float>() : 0.0f };
                edited = imgui::widget::CircleSlider( name, &v, -100.0f, 100.0f, "%.3f", false );

                if ( edited ) {
                    value = v;
                }
                else {
                    edited = false;
                }
            }
            else if ( field->mType == Type::eFloat64 ) {
                double v{ value.is_number() ? value.get<double>() : 0.0 };
                edited = ImGui::InputDouble( name, &v );

                if ( edited ) {
                    value = v;
                }
                else {
                    edited = false;
                }
            }
            else if ( field->mType >= Type::eVec2 && field->mType <= Type::eVec4 ) {
                const int count{ 2 + static_cast<int>( field->mType ) - static_cast<int>( Type::eVec2 ) };
                float v[4]{};

                for ( int i{}; i < count; ++i ) {
                    if ( value.is_array() && i < value.size() && value[i].is_number() ) {
                        v[i] = value[i].get<float>();
                    }
                }

                edited = field->mAssetKind == "Color" ? ( count == 3 ? ImGui::ColorEdit3( name, v ) : ImGui::ColorEdit4( name, v ) ) : ImGui::DragScalarN( name, ImGuiDataType_Float, v, count, 0.01f );

                if ( edited ) {
                    value = nlohmann::json::array();

                    for ( int i{}; i < count; ++i ) {
                        value.push_back( v[i] );
                    }
                }
            }
            else {
                const std::string text{ value.is_string() ? value.get<std::string>() : "" };
                eastl::vector<char> buffer( eastl::max<size_t>( text.size() + 256, 4096 ), 0 );
                std::memcpy( buffer.data(), text.data(), text.size() );
                edited = ImGui::InputText( name, buffer.data(), buffer.size() );

                if ( edited ) {
                    value = std::string{ buffer.data() };
                }

                if ( field->mType == Type::eAsset ) {
                    ImGui::SameLine();

                    if ( ImGui::Button( "..." ) ) {
                        const auto path{ filesystem::OpenFileDialog( { { "Asset", "*" } } ) };

                        if ( !path.IsEmpty() ) {
                            value = std::string{ path.GetC_Str() };
                            edited = true;
                        }
                    }

                    ImGui::SameLine();

                    if ( ImGui::SmallButton( "X" ) ) {
                        value = nullptr;
                        edited = true;
                    }

                    ImGui::TextDisabled( "%s asset", field->mAssetKind.c_str() );
                }
            }

            if ( edited ) {
                values[name] = value;
                changed = true;
            }

            ImGui::EndDisabled();
            ImGui::PopID();
        }

        if ( changed ) {
            component.SetProperties( values.dump().c_str() );
        }
    }

    static auto DrawJointPicker( core::cstr label, core::i32& joint, const ozz::animation::Skeleton& skeleton ) -> void {
        MKT_BEGIN_PROFILER();
        const auto names{ skeleton.joint_names() };
        const core::cstr preview{ joint >= 0 && joint < skeleton.num_joints() ? names[joint] : MKT_LOC( "animator_inspector_choose_joint" ).c_str() };

        if ( ImGui::BeginCombo( label, preview ) ) {
            for ( core::i32 i{}; i < skeleton.num_joints(); ++i ) {
                ImGui::PushID( i );

                if ( ImGui::Selectable( names[i], joint == i ) ) {
                    joint = i;
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }
    }

    static auto DrawIKCommon( core::float3& target, core::float3& pole, core::f32& weight, core::f32& twist ) -> void {
        MKT_BEGIN_PROFILER();
        ImGui::DragFloat3( imgui::widget::LocalizedLabel( "animator_inspector_target_model", "Target (model)" ).c_str(), &target.x, 0.01f );
        ImGui::DragFloat3( imgui::widget::LocalizedLabel( "animator_inspector_pole_direction", "Pole direction" ).c_str(), &pole.x, 0.01f );
        ImGui::SliderFloat( imgui::widget::LocalizedLabel( "animator_inspector_weight", "Weight" ).c_str(), &weight, 0.0f, 1.0f );
        ImGui::SliderAngle( imgui::widget::LocalizedLabel( "animator_inspector_twist", "Twist" ).c_str(), &twist, -180.0f, 180.0f );
    }

    static auto DrawBoneNode( const ozz::animation::Skeleton& skeleton, core::i32 joint,
        const std::array<core::i32, ozz::animation::Skeleton::kMaxJoints>& firstChildren,
        const std::array<core::i32, ozz::animation::Skeleton::kMaxJoints>& nextSiblings ) -> void {
        const auto names{ skeleton.joint_names() };
        const core::i32 parent{ skeleton.joint_parents()[joint] };
        const bool leaf{ firstChildren[joint] == ozz::animation::Skeleton::kNoParent };
        ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
                                  ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick };

        if ( leaf ) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        else if ( parent == ozz::animation::Skeleton::kNoParent ) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        ImGui::PushID( joint );
        const bool open{ ImGui::TreeNodeEx( "##Bone", flags, "%s [%d]", names[joint], joint ) };

        if ( imgui::widget::BeginItemTooltip() ) {
            ImGui::TextUnformatted( names[joint] );
            ImGui::Text( MKT_LOC( "animator_inspector_joint_index" ).c_str(), joint );

            if ( parent == ozz::animation::Skeleton::kNoParent ) {
                ImGui::TextUnformatted( MKT_LOC( "animator_inspector_root_joint" ).c_str() );
            }
            else {
                ImGui::Text( MKT_LOC( "animator_inspector_parent_joint" ).c_str(), names[parent], parent );
            }

            imgui::widget::EndItemTooltip();
        }

        if ( open && !leaf ) {
            for ( core::i32 child{ firstChildren[joint] }; child != ozz::animation::Skeleton::kNoParent; child = nextSiblings[child] ) {
                DrawBoneNode( skeleton, child, firstChildren, nextSiblings );
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    static auto DrawSkeletonHierarchy( const animation::Skeleton& skeleton ) -> void {
        MKT_BEGIN_PROFILER();
        const auto* runtime{ skeleton.GetOzzSkeleton() };

        if ( !runtime || runtime->num_joints() == 0 ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "animator_inspector_no_bones" ).c_str() );
            return;
        }

        std::array<core::i32, ozz::animation::Skeleton::kMaxJoints> firstChildren{};
        std::array<core::i32, ozz::animation::Skeleton::kMaxJoints> nextSiblings{};
        firstChildren.fill( ozz::animation::Skeleton::kNoParent );
        nextSiblings.fill( ozz::animation::Skeleton::kNoParent );
        core::i32 firstRoot{ ozz::animation::Skeleton::kNoParent };
        const auto parents{ runtime->joint_parents() };

        // Link children once per open viewer, in joint order, instead of scanning
        // the entire skeleton again for every expanded bone.
        for ( core::i32 joint{ runtime->num_joints() - 1 }; joint >= 0; --joint ) {
            const core::i32 parent{ parents[joint] };

            if ( parent == ozz::animation::Skeleton::kNoParent ) {
                nextSiblings[joint] = firstRoot;
                firstRoot = joint;
            }
            else {
                nextSiblings[joint] = firstChildren[parent];
                firstChildren[parent] = joint;
            }
        }

        ImGui::PushID( &skeleton );

        for ( core::i32 root{ firstRoot }; root != ozz::animation::Skeleton::kNoParent; root = nextSiblings[root] ) {
            DrawBoneNode( *runtime, root, firstChildren, nextSiblings );
        }

        ImGui::PopID();
    }

    static auto DrawAnimatorSettings( animation::Animator& animator ) -> void {
        MKT_BEGIN_PROFILER();
        const auto current{ animator.GetCurrentAnimation() };
        const bool hasClips{ !animator.GetAnimationList().empty() };
        ImGui::BeginDisabled( !hasClips );

        if ( ImGui::BeginCombo( imgui::widget::LocalizedLabel( "animator_inspector_animation", "Animation" ).c_str(), current ? current->GetName().c_str() : ( hasClips ? MKT_LOC( "animator_inspector_choose_clip" ).c_str() : MKT_LOC( "animator_inspector_no_animation_clips" ).c_str() ) ) ) {
            // Read names directly from the model: no temporary strings or vector
            // allocations every frame just to display the animation dropdown.
            for ( const auto& entry: animator.GetAnimationList() ) {
                const auto& name{ entry.first };
                const auto& clip{ entry.second };

                if ( ImGui::Selectable( name.c_str(), current == clip.get() ) ) {
                    if ( animator.IsPlaying() ) {
                        animator.PlayAnimation( name );
                    }
                    else {
                        animator.SetCurrentAnimation( name );
                    }
                }
            }

            ImGui::EndCombo();
        }

        core::f32 duration{ animator.GetTransitionDuration() };

        if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "animator_inspector_default_blend", "Default blend" ).c_str(), &duration, 0.01f, 0.0f, 30.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp ) ) {
            animator.SetTransitionDuration( duration );
        }

        if ( ImGui::IsItemHovered() ) {
            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "animator_inspector_crossfade_time_when_changing_clips_controller_edges_have_their_own" ).c_str() );
        }

        bool playing{ animator.IsPlaying() };

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "animator_inspector_play_selected_animation", "Play selected animation" ).c_str(), playing ) ) {
            if ( playing ) {
                animator.PlayCurrentAnimation();
            }
            else {
                animator.StopCurrentAnimation();
            }
        }

        ImGui::EndDisabled();

        if ( !hasClips ) {
            ImGui::TextWrapped( MKT_LOC( "animator_inspector_this_model_has_a_skeleton_but_no_animation_clips_its" ).c_str() );
        }

        const auto model{ animator.GetModel() };

        if ( model.IsEmpty() || !model->HasArmature() ) {
            return;
        }

        const auto& rigs{ model->GetRigs() };

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "animator_inspector_bone_hierarchy", "BoneHierarchy" ).c_str() ) ) {
            ImGui::PushID( model.GetPtr() );

            for ( core::u32 rig{}; rig < rigs.size(); ++rig ) {
                const auto* skeleton{ model->GetSkeleton( rig ) };

                if ( !skeleton || !skeleton->GetOzzSkeleton() ) {
                    continue;
                }

                ImGui::PushID( static_cast<core::i32>( rig ) );
                const ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
                                                ( rigs.size() == 1 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None ) };

                if ( ImGui::TreeNodeEx( "##Rig", flags, "%s", rigs[rig].mName.c_str() ) ) {
                    ImGui::Text( MKT_LOC( "inspector_pose_nodes" ).c_str(), skeleton->GetOzzSkeleton()->num_joints() );
                    DrawSkeletonHierarchy( *skeleton );
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            ImGui::PopID();
        }

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "animator_inspector_inverse_kinematics", "Inverse kinematics" ).c_str() ) ) {
            return;
        }

        ImGui::PushID( &animator );
        const ImGuiID selection{ ImGui::GetID( "SkeletonSelection" ) };
        core::u32 rig{ static_cast<core::u32>( ImGui::GetStateStorage()->GetInt( selection, 0 ) ) };

        if ( rig >= rigs.size() ) {
            rig = 0;
        }

        if ( ImGui::BeginCombo( imgui::widget::LocalizedLabel( "animator_inspector_skeleton", "Skeleton" ).c_str(), rigs[rig].mName.c_str() ) ) {
            for ( core::u32 index{}; index < rigs.size(); ++index ) {
                ImGui::PushID( static_cast<core::i32>( index ) );

                if ( ImGui::Selectable( rigs[index].mName.c_str(), index == rig ) ) {
                    rig = index;
                    ImGui::GetStateStorage()->SetInt( selection, static_cast<core::i32>( rig ) );
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        ImGui::PopID();
        ImGui::TextDisabled( MKT_LOC( "animator_inspector_playback_controls_all_rigs_ik_edits_apply_to_the_selected" ).c_str() );
        const auto& skeleton{ *model->GetSkeleton( rig )->GetOzzSkeleton() };
        ImGui::TextWrapped( MKT_LOC( "animator_inspector_ik_evaluates_after_animation_blending_targets_use_skeleton_model_space" ).c_str() );
        auto& chains{ animator.GetTwoBoneConstraints( rig ) };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_inspector_add_two_bone_chain", "Add two-bone chain" ).c_str() ) && chains.size() < 32 ) {
            chains.emplace_back();
        }

        ImGui::PushID( "TwoBone" );

        for ( core::usize i{}; i < chains.size(); ) {
            auto& chain{ chains[i] };
            ImGui::PushID( static_cast<core::i32>( i ) );
            bool remove{};

            if ( ImGui::TreeNodeEx( "##Chain", ImGuiTreeNodeFlags_DefaultOpen, MKT_LOC( "animator_inspector_two_bone_chain" ).c_str(), static_cast<core::i32>( i + 1 ) ) ) {
                imgui::CheckBox( imgui::widget::LocalizedLabel( "animator_inspector_enabled", "Enabled" ).c_str(), chain.mEnabled );
                DrawJointPicker( MKT_LOC( "animator_inspector_start" ).c_str(), chain.mStart, skeleton );
                DrawJointPicker( MKT_LOC( "animator_inspector_middle" ).c_str(), chain.mMiddle, skeleton );
                DrawJointPicker( MKT_LOC( "animator_inspector_end" ).c_str(), chain.mEnd, skeleton );
                DrawIKCommon( chain.mTarget, chain.mPole, chain.mWeight, chain.mTwist );
                ImGui::DragFloat3( imgui::widget::LocalizedLabel( "animator_inspector_middle_bend_axis", "Middle bend axis" ).c_str(), &chain.mMiddleAxis.x, 0.01f );
                ImGui::SliderFloat( imgui::widget::LocalizedLabel( "animator_inspector_soften", "Soften" ).c_str(), &chain.mSoften, 0.0f, 1.0f );
                ImGui::TextDisabled( chain.mReached ? MKT_LOC( "animator_inspector_target_reached" ).c_str() : MKT_LOC( "animator_inspector_target_not_reached_chain_inactive" ).c_str() );
                remove = ImGui::Button( imgui::widget::LocalizedLabel( "animator_inspector_remove_chain", "Remove chain" ).c_str() );
                ImGui::TreePop();
            }

            ImGui::PopID();

            if ( remove ) {
                chains.erase( chains.begin() + i );
            }
            else {
                ++i;
            }
        }

        ImGui::PopID();
        auto& aims{ animator.GetAimConstraints( rig ) };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "animator_inspector_add_aim_constraint", "Add aim constraint" ).c_str() ) && aims.size() < 32 ) {
            aims.emplace_back();
        }

        ImGui::PushID( "Aim" );

        for ( core::usize i{}; i < aims.size(); ) {
            auto& aim{ aims[i] };
            ImGui::PushID( static_cast<core::i32>( i ) );
            bool remove{};

            if ( ImGui::TreeNodeEx( "##Aim", ImGuiTreeNodeFlags_DefaultOpen, MKT_LOC( "animator_inspector_aim_constraint" ).c_str(), static_cast<core::i32>( i + 1 ) ) ) {
                imgui::CheckBox( imgui::widget::LocalizedLabel( "animator_inspector_enabled", "Enabled" ).c_str(), aim.mEnabled );
                DrawJointPicker( MKT_LOC( "animator_inspector_joint" ).c_str(), aim.mJoint, skeleton );
                DrawIKCommon( aim.mTarget, aim.mPole, aim.mWeight, aim.mTwist );
                ImGui::DragFloat3( imgui::widget::LocalizedLabel( "animator_inspector_forward_local", "Forward (local)" ).c_str(), &aim.mForward.x, 0.01f );
                ImGui::DragFloat3( imgui::widget::LocalizedLabel( "animator_inspector_up_local", "Up (local)" ).c_str(), &aim.mUp.x, 0.01f );
                ImGui::DragFloat3( imgui::widget::LocalizedLabel( "animator_inspector_offset_local", "Offset (local)" ).c_str(), &aim.mOffset.x, 0.01f );
                ImGui::TextDisabled( aim.mReached ? MKT_LOC( "animator_inspector_target_reached" ).c_str() : MKT_LOC( "animator_inspector_target_not_reached_constraint_inactive" ).c_str() );
                remove = ImGui::Button( imgui::widget::LocalizedLabel( "animator_inspector_remove_constraint", "Remove constraint" ).c_str() );
                ImGui::TreePop();
            }

            ImGui::PopID();

            if ( remove ) {
                aims.erase( aims.begin() + i );
            }
            else {
                ++i;
            }
        }

        ImGui::PopID();
    }

    static auto PhysicsFieldLabel( const char* name ) -> eastl::string {
        const eastl::string key{ eastl::string_view{ name } == "PinnedVertices" ? "inspector_pinned_vertices" : name };
        const auto& translation{ MKT_LOC( key ) };

        if ( translation != key ) {
            return translation;
        }

        eastl::string label{};

        for ( const char* c{ name }; *c; ++c ) {
            if ( c != name && *c >= 'A' && *c <= 'Z' && c[-1] >= 'a' && c[-1] <= 'z' ) {
                label += ' ';
            }

            label += *c;
        }

        return label;
    }

    static auto EditPhysicsEntity( const char* name, core::u64& id, scene::Scene* scene ) -> bool {
        const auto label{ string::Format( "{}###{}", PhysicsFieldLabel( name ), name ) };
        auto* selected{ scene ? scene->FindByID( id ) : nullptr };
        bool changed{};

        if ( ImGui::BeginCombo( label.c_str(), selected ? selected->GetComponent<scene::TagComponent>().GetTag().c_str() : id ? "Missing entity"
                                                                                                                              : "World / none" ) ) {
            if ( ImGui::Selectable( "World / none", id == 0 ) ) {
                id = 0;
                changed = true;
            }

            if ( scene ) {
                for ( auto&& entry: scene->GetRegistry().view<scene::TagComponent>().each() ) {
                    const auto entity{ std::get<0>( entry ) };
                    auto& tag{ std::get<1>( entry ) };

                    ImGui::PushID( static_cast<int>( entity ) );

                    if ( ImGui::Selectable( tag.GetTag().c_str(), id == tag.GetGuid() ) ) {
                        id = tag.GetGuid();
                        changed = true;
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndCombo();
        }

        return changed;
    }

    template<typename T>
    static auto EditPhysicsField( const char* name, T& value, scene::Scene* scene ) -> bool {
        const auto label{ PhysicsFieldLabel( name ) };
        const auto widgetLabel{ string::Format( "{}###{}", label, name ) };

        if constexpr ( std::is_same_v<T, bool> ) {
            return imgui::CheckBox( widgetLabel.c_str(), value );
        }
        else if constexpr ( std::is_enum_v<T> ) {
            const char* choices{};

            if constexpr ( std::is_same_v<T, scene::JointMotorMode> ) {
                choices = "Off\0Velocity\0Position\0";
            }

            if constexpr ( std::is_same_v<T, scene::JointAxisMotion> ) {
                choices = "Fixed\0Free\0Limited\0";
            }

            if constexpr ( std::is_same_v<T, scene::CompoundColliderMode> ) {
                choices = "Static\0Mutable\0";
            }

            if constexpr ( std::is_same_v<T, scene::CharacterControllerType> ) {
                choices = "Virtual\0Rigid body\0";
            }

            if constexpr ( std::is_same_v<T, scene::VehicleType> ) {
                choices = "Wheeled\0Motorcycle\0Tracked\0Six DOF\0";
            }

            int index{ static_cast<int>( value ) };

            if ( choices && ImGui::Combo( widgetLabel.c_str(), &index, choices ) ) {
                value = static_cast<T>( index );
                return true;
            }

            return false;
        }
        else if constexpr ( std::is_same_v<T, core::u64> ) {
            return EditPhysicsEntity( name, value, scene );
        }
        else if constexpr ( std::is_integral_v<T> ) {
            return ImGui::InputScalar( widgetLabel.c_str(), ImGuiDataType_U32, &value );
        }
        else if constexpr ( std::is_floating_point_v<T> ) {
            return ImGui::DragFloat( widgetLabel.c_str(), &value, .01f );
        }
        else if constexpr ( std::is_same_v<T, core::float3> ) {
            return ImGui::DragFloat3( widgetLabel.c_str(), &value.x, .01f );
        }
        else if constexpr ( std::is_same_v<T, core::float2> ) {
            return ImGui::DragFloat2( widgetLabel.c_str(), &value.x, .01f );
        }
        else if constexpr ( std::is_same_v<T, core::quat> ) {
            auto angles{ glm::degrees( glm::eulerAngles( value ) ) };

            if ( ImGui::DragFloat3( widgetLabel.c_str(), &angles.x, .5f ) ) {
                value = core::quat{ glm::radians( angles ) };
                return true;
            }

            return false;
        }
        else if constexpr ( std::is_same_v<T, eastl::string> || requires { value.GetPath(); } ) {
            eastl::string text{};

            if constexpr ( std::is_same_v<T, eastl::string> ) {
                text = value;
            }
            else {
                text = value.GetPath().GetC_Str();
            }

            char buffer[1024]{};
            std::snprintf( buffer, sizeof( buffer ), "%s", text.c_str() );
            bool changed{ ImGui::InputText( widgetLabel.c_str(), buffer, sizeof( buffer ), ImGuiInputTextFlags_EnterReturnsTrue ) };

            if constexpr ( requires { value.GetPath(); } ) {
                ImGui::SameLine();
                ImGui::PushID( name );

                if ( ImGui::SmallButton( "..." ) ) {
                    const auto path{ filesystem::OpenFileDialog( { { "Asset", "*" } } ) };

                    if ( !path.IsEmpty() ) {
                        std::snprintf( buffer, sizeof( buffer ), "%s", path.GetC_Str() );
                        changed = true;
                    }
                }

                ImGui::PopID();

                if ( changed ) {
                    value = T{ filesystem::Path{ buffer } };
                }
            }
            else {
                if ( changed ) {
                    value = buffer;
                }
            }

            return changed;
        }
        else {
            bool changed{};

            if ( ImGui::TreeNode( name, "%s", label.c_str() ) ) {
                if constexpr ( requires { value.begin(); value.end(); } ) {
                    for ( size_t i{}; i < value.size(); ++i ) {
                        ImGui::PushID( static_cast<int>( i ) );
                        const auto item{ string::Format( "Item {}", i ) };

                        if constexpr ( std::is_same_v<typename T::value_type, core::u64> ) {
                            changed |= EditPhysicsEntity( item.c_str(), value[i], scene );
                        }
                        else {
                            changed |= EditPhysicsField( item.c_str(), value[i], scene );
                        }

                        ImGui::PopID();
                    }

                    if constexpr ( requires { value.emplace_back(); value.pop_back(); } ) {
                        constexpr core::usize kMaxItems{ 4096 };

                        ImGui::BeginDisabled( value.size() >= kMaxItems );
                        if ( ImGui::SmallButton( "+" ) ) {
                            value.emplace_back();
                            changed = true;
                        }
                        ImGui::EndDisabled();

                        ImGui::SameLine();

                        ImGui::BeginDisabled( value.empty() );
                        if ( ImGui::SmallButton( "-" ) ) {
                            value.pop_back();
                            changed = true;
                        }
                        ImGui::EndDisabled();
                    }
                }
                else {
                    scene::VisitPhysicsFields( value, [&]( const char* field, auto& data ) {
                        const bool edited{ EditPhysicsField( field, data, scene ) };
                        changed |= edited;
                        return edited;
                    } );
                }

                ImGui::TreePop();
            }

            return changed;
        }
    }

    static auto DrawCameraDepthOfFieldOptions( scene::SceneCamera& camera ) -> void {
        MKT_BEGIN_PROFILER();
        ImGui::Spacing();

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "inspector_cinematic_depth_of_field_cameradof", "Cinematic depth of field##CameraDoF" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        ImGui::PushID( "CameraDepthOfField" );
        auto settings{ camera.GetDepthOfFieldSettings() };
        const bool perspective{ camera.GetProjectionType() == scene::ProjectionType::ePerspective };

        ImGui::BeginDisabled( !perspective );
        bool changed{ imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_enable_depth_of_field", "Enable depth of field" ).c_str(), settings.mEnabled ) };
        ImGui::SameLine();

        if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "inspector_reset", "Reset" ).c_str() ) ) {
            settings = scene::DepthOfFieldSettings{};
            changed = true;
        }

        if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort ) ) {
            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_restore_the_default_lens_and_focus_settings_and_disable_depth" ).c_str() );
        }

        ImGui::Spacing();
        changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_link_lens_to_field_of_view", "Link lens to field of view" ).c_str(), settings.mUsePhysicalLens );
        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_derive_field_of_view_from_focal_length_and_filmback_width" ).c_str() );

        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2{ 4.0f, 5.0f } );
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings };

        if ( ImGui::BeginTable( "Options", 2, tableFlags ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "inspector_property" ).c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.5f );
            ImGui::TableSetupColumn( MKT_LOC( "inspector_value" ).c_str(), ImGuiTableColumnFlags_WidthStretch );

            const auto drawValue = []( core::cstr label, core::cstr tooltip, core::f32& value, core::f32 speed, core::f32 minimum, core::f32 maximum, core::cstr format ) -> bool {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( label );

                if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled ) ) {
                    mikoto::imgui::widget::ItemTooltip( "%s", tooltip );
                }

                ImGui::TableSetColumnIndex( 1 );
                ImGui::SetNextItemWidth( -FLT_MIN );
                ImGui::PushID( label );
                const bool edited{ ImGui::DragFloat( "##Value", &value, speed, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp ) };

                if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled ) ) {
                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_ctrl_click_to_enter_an_exact_value" ).c_str(), tooltip );
                }

                ImGui::PopID();
                return edited;
            };

            changed |= drawValue( MKT_LOC( "inspector_filmback_width" ).c_str(), MKT_LOC( "inspector_sensor_width_in_millimeters_sensor_height_follows_the_viewport_aspect" ).c_str(),
                settings.mSensorWidth, 0.1f, 1.0f, 100.0f, "%.2f mm" );
            changed |= drawValue( MKT_LOC( "inspector_focal_length" ).c_str(), MKT_LOC( "inspector_lens_focal_length_in_millimeters_when_linked_longer_focal_lengths" ).c_str(),
                settings.mFocalLength, 1.0f, 1.0f, 300.0f, "%.1f mm" );
            changed |= drawValue( MKT_LOC( "inspector_max_aperture" ).c_str(), MKT_LOC( "inspector_minimum_f_stop_the_widest_opening_supported_by_the_lens" ).c_str(),
                settings.mMaxAperture, 0.05f, 0.7f, 64.0f, "f/%.2f" );
            changed |= drawValue( MKT_LOC( "inspector_aperture" ).c_str(), MKT_LOC( "inspector_lens_opening_expressed_as_an_f_stop_lower_values_produce" ).c_str(),
                settings.mAperture, 0.05f, settings.mMaxAperture, 64.0f, "f/%.2f" );
            changed |= drawValue( MKT_LOC( "inspector_focus_distance" ).c_str(), MKT_LOC( "inspector_distance_from_the_camera_to_the_sharp_focus_plane_in" ).c_str(),
                settings.mFocusDistance, 0.05f, settings.mFocalLength * 0.001f + 0.001f, 100000.0f, "%.3f m" );
            changed |= drawValue( MKT_LOC( "inspector_maximum_blur" ).c_str(), MKT_LOC( "inspector_limits_the_blur_radius_in_pixels_higher_values_allow_softer" ).c_str(),
                settings.mMaxBlur, 0.25f, 0.0f, 64.0f, "%.1f px" );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( MKT_LOC( "inspector_diaphragm_blades" ).c_str() );
            ImGui::TableSetColumnIndex( 1 );
            ImGui::SetNextItemWidth( -FLT_MIN );
            int blades{ static_cast<int>( settings.mBladeCount ) };

            if ( ImGui::SliderInt( "##DiaphragmBlades", &blades, 3, 16 ) ) {
                settings.mBladeCount = static_cast<core::u32>( blades );
                changed = true;
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_number_of_diaphragm_blades_fewer_blades_make_polygonal_bokeh_more" ).c_str() );

            changed |= drawValue( MKT_LOC( "inspector_blade_roundness" ).c_str(), MKT_LOC( "inspector_roundness_at_maximum_aperture_zero_is_polygonal_one_is_circular" ).c_str(),
                settings.mBladeCurvature, 0.01f, 0.0f, 1.0f, "%.2f" );
            changed |= drawValue( MKT_LOC( "inspector_blade_rotation" ).c_str(), MKT_LOC( "inspector_rotates_the_diaphragm_and_the_resulting_bokeh" ).c_str(),
                settings.mBladeRotation, 1.0f, -180.0f, 180.0f, MKT_LOC( "inspector_deg" ).c_str() );
            ImGui::EndTable();
        }

        ImGui::PopStyleVar();
        ImGui::EndDisabled();

        if ( !perspective ) {
            ImGui::TextWrapped( MKT_LOC( "inspector_depth_of_field_requires_a_perspective_camera" ).c_str() );
        }

        ImGui::TextWrapped( MKT_LOC( "inspector_focus_distance_is_measured_in_meters_aperture_changes_blur_not" ).c_str() );

        if ( changed ) {
            camera.SetDepthOfFieldSettings( settings );
        }

        ImGui::PopID();
    }

    static auto DrawCameraVignetteOptions( scene::SceneCamera& camera ) -> void {
        MKT_BEGIN_PROFILER();
        ImGui::Spacing();

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "inspector_vignette", "CameraVignette" ).c_str(),
                 ImGuiTreeNodeFlags_DefaultOpen ) ) {
            return;
        }

        ImGui::PushID( "CameraVignette" );
        auto settings{ camera.GetVignetteSettings() };
        bool changed{ imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_enable_vignette", "Enabled" ).c_str(), settings.mEnabled ) };
        ImGui::SameLine();

        if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "inspector_reset", "Reset" ).c_str() ) ) {
            settings = scene::VignetteSettings{};
            changed = true;
        }

        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2{ 4.0f, 5.0f } );

        if ( ImGui::BeginTable( "Options", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "inspector_property" ).c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 8.5f );
            ImGui::TableSetupColumn( MKT_LOC( "inspector_value" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
            const auto nextValue{ []( core::cstr key ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted( MKT_LOC( key ).c_str() );
                ImGui::TableSetColumnIndex( 1 );
                ImGui::SetNextItemWidth( -FLT_MIN );
            } };

            nextValue( "inspector_vignette_shape" );
            const std::array<core::cstr, 2> shapes{
                MKT_LOC( "inspector_vignette_circle" ).c_str(),
                MKT_LOC( "inspector_vignette_rectangle" ).c_str()
            };
            int shape{ core::as<int>( settings.mShape ) };

            if ( ImGui::Combo( "##Shape", &shape, shapes.data(), core::as<int>( shapes.size() ) ) ) {
                settings.mShape = core::as<scene::VignetteShape>( shape );
                changed = true;
            }

            nextValue( "inspector_vignette_center" );
            changed |= ImGui::SliderFloat2( "##Center", &settings.mCenter.x, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp );
            //imgui::widget::ItemTooltip( "%s", MKT_LOC( "inspector_vignette_center_help" ).c_str() );

            if ( settings.mShape == scene::VignetteShape::eCircle ) {
                nextValue( "inspector_vignette_radius" );
                changed |= ImGui::DragFloat( "##Radius", &settings.mHalfSize.x, 0.01f, 0.0001f, 4.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp );
            }
            else {
                nextValue( "inspector_vignette_half_size" );
                changed |= ImGui::DragFloat2( "##HalfSize", &settings.mHalfSize.x, 0.01f, 0.0001f, 4.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp );
            }
            //imgui::widget::ItemTooltip( "%s", MKT_LOC( "inspector_vignette_size_help" ).c_str() );

            nextValue( "inspector_vignette_feather" );
            changed |= ImGui::DragFloat( "##Feather", &settings.mFeather, 0.01f, 0.0001f, 4.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp );
            nextValue( "inspector_vignette_strength" );
            changed |= ImGui::SliderFloat( "##Strength", &settings.mStrength, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp );
            nextValue( "inspector_vignette_color" );
            changed |= ImGui::ColorEdit3( "##Color", &settings.mColor.x );
            nextValue( "inspector_vignette_aspect" );
            changed |= imgui::CheckBox( "##AspectCorrect", settings.mAspectCorrect );
            //imgui::widget::ItemTooltip( "%s", MKT_LOC( "inspector_vignette_aspect_help" ).c_str() );
            ImGui::EndTable();
        }

        ImGui::PopStyleVar();

        if ( changed ) {
            camera.SetVignetteSettings( settings );
        }

        ImGui::PopID();
    }


    auto InspectorPanel::DrawMeshInfo( scene::MeshComponent& component ) -> void {
        MKT_BEGIN_PROFILER();
        ImGui::TextUnformatted( MKT_LOC( "inspector_mesh_info" ).c_str() );
        auto* mesh{ component.GetMesh() };

        if ( !mesh ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_mesh_no_data" ).c_str() );
            return;
        }

        mMeshInfoDrawn = true;

        const auto& render{ mesh->GetRenderData() };
        const auto& collisionVertices{ mesh->GetCollisionVertices() };
        const auto& collisionIndices{ mesh->GetCollisionIndices() };
        const auto& skinningVertices{ mesh->GetSkinningVertices() };
        const auto& skin{ mesh->GetSkinBinding() };
        const auto& properties{ mesh->GetProperties() };
        const auto vertexBuffer{ mesh->GetVertexBuffer() };
        const auto indexBuffer{ mesh->GetIndexBuffer() };
        const core::usize vertexCount{ render ? render->GetVertexCount() : collisionVertices.size() };
        const core::usize indexCount{ render ? render->GetIndices().size() : collisionIndices.size() };
        const bool skinned{ render ? render->IsSkinned() : !skin.mJoints.empty() };
        const auto bytes{ []( core::usize size ) {
            constexpr core::f64 bytesPerMiB{ 1024.0 * 1024.0 };
            return string::Format( "{} B ({:.2f} MiB)", size, core::as<core::f64>( size ) / bytesPerMiB );
        } };
        const auto vector{ []( const core::float3& value ) {
            return string::Format( "({:.3f}, {:.3f}, {:.3f})", value.x, value.y, value.z );
        } };
        const auto& none{ MKT_LOC( "inspector_none" ) };

        if ( BeginMeshInfoTable( "MeshGeneralInfo" ) ) {
            MeshInfoRow( "inspector_name", mesh->GetName().empty() ? none : mesh->GetName() );
            MeshInfoRow( "inspector_mesh_index", string::Format( "{}", mesh->GetMeshIndex() ) );

            if ( const auto model{ component.GetModel() } ) {
                MeshInfoRow( "inspector_mesh_model_name", string::Format( "{}", model->GetName() ) );
                MeshInfoRow( "inspector_mesh_model_meshes", string::Format( "{}", model->GetMeshNodeCount() ) );
            }

            ImGui::EndTable();
        }

        ImGui::SeparatorText( MKT_LOC( "inspector_mesh_geometry_heading" ).c_str() );
        if ( BeginMeshInfoTable( "MeshGeometry" ) ) {
            MeshInfoRow( "inspector_mesh_vertices", string::Format( "{}", vertexCount ) );
            MeshInfoRow( "inspector_mesh_indices", string::Format( "{}", indexCount ) );
            constexpr core::usize indicesPerTriangle{ 3 };
            MeshInfoRow( "inspector_mesh_triangles", string::Format( "{}", indexCount / indicesPerTriangle ) );
            MeshInfoRow( "inspector_mesh_index_format", indexBuffer
                ? eastl::string{ rhi::GetFormatInfo( indexBuffer->GetFormat() ).mName }
                : ( render ? eastl::string{ rhi::GetFormatInfo( rhi::Format::eR32_UINT ).mName } : none ) );
            ImGui::EndTable();
        }

        ImGui::SeparatorText( MKT_LOC( "stats_memory_heading" ).c_str() );
        imgui::widget::ItemTooltip( "%s", MKT_LOC( "inspector_mesh_shared_memory" ).c_str() );

        if ( BeginMeshInfoTable( "MeshMemory" ) ) {
            MeshInfoRow( "inspector_mesh_vertex_buffer", vertexBuffer ? bytes( vertexBuffer->GetSizeBytes() ) : none );
            MeshInfoRow( "inspector_mesh_index_buffer", indexBuffer ? bytes( indexBuffer->GetSizeBytes() ) : none );

            if ( render ) {
                MeshInfoRow( "inspector_mesh_render_attributes", bytes( render->GetAttributes().size() ) );
                MeshInfoRow( "inspector_mesh_render_indices", bytes( render->GetIndices().size_bytes() ) );
            }

            ImGui::EndTable();
        }

        ImGui::SeparatorText( MKT_LOC( "inspector_mesh_imported_material_heading" ).c_str() );
        if ( BeginMeshInfoTable( "MeshMaterial" ) ) {
            MeshInfoRow( "inspector_material_name", properties.mName.empty() ? none : properties.mName );
            MeshInfoRow( "inspector_mesh_material_index", mesh->GetMaterialSourceIndex() == ~core::u32{}
                ? none
                : string::Format( "{}", mesh->GetMaterialSourceIndex() ) );
            MeshInfoRow( "inspector_mesh_texture_bindings", string::Format( "{}", properties.mTextureBindings.size() ) );
            ImGui::EndTable();
        }

        ImGui::Spacing();

        if ( ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "inspector_mesh_collision_heading", "MeshCollision" ).c_str() ) ) {
            if ( BeginMeshInfoTable( "MeshCollisionInfo" ) ) {
                MeshInfoRow( "inspector_mesh_collision_vertices", string::Format( "{}", collisionVertices.size() ) );
                MeshInfoRow( "inspector_mesh_collision_indices", string::Format( "{}", collisionIndices.size() ) );
                MeshInfoRow( "inspector_mesh_collision_data", bytes(
                    collisionVertices.size() * sizeof( core::float3 ) + collisionIndices.size() * sizeof( core::u32 ) ) );
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if ( ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "inspector_mesh_skinning_heading", "MeshSkinning" ).c_str() ) ) {
            if ( BeginMeshInfoTable( "MeshSkinningInfo" ) ) {
                MeshInfoRow( "inspector_mesh_skinned", MKT_LOC( skinned ? "inspector_mesh_yes" : "inspector_mesh_no" ) );
                MeshInfoRow( "inspector_mesh_skinning_vertices", string::Format( "{}", skinningVertices.size() ) );
                MeshInfoRow( "inspector_mesh_skinning_data", bytes( skinningVertices.size() * sizeof( animation::SkinningVertex ) ) );
                MeshInfoRow( "inspector_mesh_joints", string::Format( "{}", skin.mJoints.size() ) );

                if ( !skin.mJoints.empty() ) {
                    MeshInfoRow( "inspector_mesh_rig_index", string::Format( "{}", skin.mRigIndex ) );
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if ( render && ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "inspector_mesh_local_bounds", "MeshLocalBounds" ).c_str() ) ) {
            if ( BeginMeshInfoTable( "LocalBounds" ) ) {
                const auto center{ render->GetSortCenter() };
                const auto extent{ render->GetBoundsExtent() };
                MeshInfoRow( "inspector_mesh_bounds_minimum", vector( center - extent ) );
                MeshInfoRow( "inspector_mesh_bounds_maximum", vector( center + extent ) );
                MeshInfoRow( "inspector_mesh_bounds_center", vector( center ) );
                MeshInfoRow( "inspector_mesh_bounds_half_extent", vector( extent ) );
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if ( ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "inspector_mesh_import_transform", "MeshImportTransform" ).c_str() ) ) {
            DrawImportedMeshTransform( mesh->GetTransform() );
            ImGui::TreePop();
        }

        if ( render && ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "inspector_mesh_vertex_streams", "MeshVertexStreams" ).c_str() ) ) {
            if ( ImGui::BeginTable( "VertexStreams", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp ) ) {
                ImGui::TableSetupColumn( MKT_LOC( "inspector_mesh_stream" ).c_str() );
                ImGui::TableSetupColumn( MKT_LOC( "inspector_mesh_offset_bytes" ).c_str() );
                ImGui::TableHeadersRow();
                constexpr std::array<core::cstr, renderer::kMeshStreamCount> names{
                    "POSITION", "NORMAL", "COLOR", "UV0", "UV1", "JOINTS0", "WEIGHTS0", "JOINTS1", "WEIGHTS1"
                };
                const core::usize count{ render->IsSkinned() ? renderer::kMeshStreamCount : static_cast<core::usize>( renderer::MeshStream::Joints0 ) };

                for ( core::usize stream{}; stream < count; ++stream ) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( names[stream] );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( string::Format( "{}", render->GetOffsets()[stream] ).c_str() );
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::SeparatorText( MKT_LOC( "inspector_mesh_original_textures" ).c_str() );
        const auto& bindings{ properties.mTextureBindings };
        mMeshTexturePreviews.resize( bindings.size() );

        for ( core::usize index{}; index < bindings.size(); ++index ) {
            auto& preview{ mMeshTexturePreviews[index] };

            if ( preview.mPath != bindings[index].first ) {
                preview = {};
                preview.mPath = bindings[index].first;
            }
        }

        if ( bindings.empty() ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_mesh_no_original_textures" ).c_str() );
            return;
        }

        struct TextureRole {
            material::MapType mType{};
            core::cstr mLabel{};
            core::i32 material::SurfaceMaterialDescription::* mUvSet{};
        };
        static constexpr std::array<TextureRole, 10> roles{
            TextureRole{ material::MapType::eDiffuse, "inspector_diffuse", &material::SurfaceMaterialDescription::BaseColorTextureSet },
            TextureRole{ material::MapType::eBaseColor, "inspector_albedo", &material::SurfaceMaterialDescription::BaseColorTextureSet },
            TextureRole{ material::MapType::eNormal, "inspector_normal", &material::SurfaceMaterialDescription::NormalTextureSet },
            TextureRole{ material::MapType::eMetallic, "inspector_metallic", &material::SurfaceMaterialDescription::MetallicRoughnessTextureSet },
            TextureRole{ material::MapType::eRoughness, "inspector_roughness", &material::SurfaceMaterialDescription::MetallicRoughnessTextureSet },
            TextureRole{ material::MapType::eMetallicRoughness, "inspector_metallic_roughness", &material::SurfaceMaterialDescription::MetallicRoughnessTextureSet },
            TextureRole{ material::MapType::eAmbientOcclusion, "inspector_ambient_occlusion", &material::SurfaceMaterialDescription::OcclusionTextureSet },
            TextureRole{ material::MapType::eEmissive, "inspector_emission", &material::SurfaceMaterialDescription::EmissiveTextureSet },
            TextureRole{ material::MapType::eSpecularGlossiness, "inspector_specular_glossiness", &material::SurfaceMaterialDescription::SpecularGlossinessSet },
            TextureRole{ material::MapType::eClearCoatNormal, "inspector_clear_coat_normal", &material::SurfaceMaterialDescription::ClearCoatNormalTextureSet }
        };
        const float previewEdge{ ImGui::GetFrameHeight() * 3.0f };

        if ( !ImGui::BeginTable( "OriginalMeshTextures", 3,
                 ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp ) ) {
            return;
        }

        ImGui::TableSetupColumn( MKT_LOC( "inspector_mesh_preview" ).c_str(), ImGuiTableColumnFlags_WidthFixed, previewEdge + 4.0f );
        ImGui::TableSetupColumn( MKT_LOC( "inspector_mesh_map" ).c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.0f );
        ImGui::TableSetupColumn( MKT_LOC( "inspector_mesh_source_details" ).c_str(), ImGuiTableColumnFlags_WidthStretch );
        ImGui::TableHeadersRow();

        auto* assets{ asset::AssetRegistryService::GetPtr() };

        for ( core::usize index{}; index < bindings.size(); ++index ) {
            const auto& path{ bindings[index].first };
            const auto& binding{ bindings[index].second };

            auto& preview{ mMeshTexturePreviews[index] };
            auto texture{ binding.mTexture };

            if ( texture || path.IsEmpty() ) {
                preview.mRequest = {};
            }
            else {
                const auto state{ preview.mRequest.GetState() };

                if ( !preview.mRequest || state == asset::AssetState::eUnloaded || state == asset::AssetState::eEvicting ) {
                    preview.mRequest = assets->Request<asset::Texture>( asset::AssetRef<asset::Texture>{ path } );
                }

                if ( const auto loaded{ preview.mRequest.GetIfReady() } ) {
                    texture = loaded->GetResource();
                }
            }

            const TextureRole* role{};

            for ( const auto& candidate: roles ) {
                if ( candidate.mType == binding.mMapType ) {
                    role = &candidate;
                    break;
                }
            }

            ImGui::PushID( static_cast<int>( index ) );
            ImGui::TableNextRow( ImGuiTableRowFlags_None, previewEdge + 4.0f );
            ImGui::TableSetColumnIndex( 0 );
            const auto image{ texture ? texture : assets->GetDummyTexture() };
            const float longest{ static_cast<float>( eastl::max( image->GetWidth(), image->GetHeight() ) ) };
            const ImVec2 imageSize{ longest > 0.0f ? previewEdge * image->GetWidth() / longest : previewEdge,
                longest > 0.0f ? previewEdge * image->GetHeight() / longest : previewEdge };
            bool clicked{};
            {
                imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_FramePadding, ImVec2{ 2.0f, 2.0f } };
                clicked = imgui::PushImageButton( "OriginalTexture", imgui::ImGuiService::Get()->GetTextureID( image ), imageSize );
            }

            if ( texture ) {
                if ( clicked ) {
                    ImGui::OpenPopup( "OriginalTexturePreview" );
                }

                if ( ImGui::IsPopupOpen( "OriginalTexturePreview" ) ) {
                    // Measure the Inspector's viewport before entering the popup;
                    // an auto-sized popup may otherwise measure its own viewport.
                    const auto* viewport{ ImGui::GetWindowViewport() };
                    const ImVec2 maximumSize{ eastl::max( 1.0f, viewport->WorkSize.x - 32.0f ),
                        eastl::max( 1.0f, viewport->WorkSize.y - 32.0f ) };
                    const float padding{ ImGui::GetStyle().WindowPadding.x * 2.0f };
                    const float width{ eastl::min( ImGui::GetFontSize() * 24.0f + padding, maximumSize.x ) };
                    const float edge{ eastl::min( ImGui::GetFontSize() * 24.0f,
                        eastl::max( 1.0f, eastl::min( width - padding, maximumSize.y * 0.5f ) ) ) };
                    ImGui::SetNextWindowViewport( viewport->ID );
                    ImGui::SetNextWindowPos( viewport->GetWorkCenter(), ImGuiCond_Appearing, { 0.5f, 0.5f } );
                    // Fix the wrap width and allow scrolling if the details exceed
                    // the work area, rather than resizing or moving with the mouse.
                    ImGui::SetNextWindowSizeConstraints( { width, 0.0f }, { width, maximumSize.y } );

                    if ( ImGui::BeginPopup( "OriginalTexturePreview", ImGuiWindowFlags_NoMove ) ) {
                        if ( longest > 0.0f ) {
                            ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( texture ),
                                { edge * texture->GetWidth() / longest, edge * texture->GetHeight() / longest } );
                        }

                        DrawTextureProperties( *texture );

                        if ( !path.IsEmpty() ) {
                            ImGui::TextWrapped( "%s", path.GetC_Str() );
                        }

                        ImGui::EndPopup();
                    }
                }
                else if ( !ImGui::IsPopupOpen( nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel ) ) {
                    ShowTextureHoverTooltip( texture.GetPtr() );
                }
            }

            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextWrapped( "%s", MKT_LOC( role ? role->mLabel : "inspector_mesh_unknown_map" ).c_str() );

            if ( role ) {
                ImGui::TextDisabled( "%s: UV%d", MKT_LOC( "inspector_uv_set" ).c_str(), properties.*( role->mUvSet ) );
            }

            ImGui::TableSetColumnIndex( 2 );
            const auto caption{ !path.IsEmpty() ? path.GetFilename()
                                                : ( texture && !texture->GetDebugName().empty() ? eastl::string{ texture->GetDebugName() } : none ) };
            ImGui::TextWrapped( "%s", caption.data() );

            if ( !path.IsEmpty() ) {
                imgui::widget::ItemTooltip( "%s", path.GetC_Str() );
            }

            if ( texture ) {
                ImGui::TextDisabled( "%u x %u", texture->GetWidth(), texture->GetHeight() );
                ImGui::TextDisabled( "%s", rhi::GetFormatInfo( texture->GetFormat() ).mName );
            }
            else {
                const bool failed{ preview.mRequest.GetState() == asset::AssetState::eFailed };
                ImGui::PushTextWrapPos( 0.0f );
                ImGui::TextDisabled( "%s", MKT_LOC( failed ? "inspector_mesh_texture_failed"
                                                           : ( path.IsEmpty() ? "inspector_mesh_texture_unavailable" : "inspector_mesh_texture_loading" ) )
                    .c_str() );
                ImGui::PopTextWrapPos();

                if ( failed ) {
                    imgui::widget::ItemTooltip( "%s", preview.mRequest.GetError().c_str() );
                }
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }


    auto InspectorPanel::DrawNameTextInput( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();
        auto& tag{ entity->GetComponent<scene::TagComponent>() };
        const core::u64 id{ tag.GetGuid() };
        // Follow names changed in Hierarchy or scripts without replacing an active draft.
        const bool editing{ ImGui::GetActiveID() == ImGui::GetID( "##EntityName" ) };

        if ( mNameEntity != id || mNameScene != mState->mActiveScene || mNameBuffer.empty() ||
             ( !editing && tag.GetTag() != mNameSource ) ) {
            mNameEntity = id;
            mNameScene = mState->mActiveScene;
            mNameSource = tag.GetTag();
            mNameBuffer.resize( eastl::max<core::usize>( 512, tag.GetTag().size() + 1 ) );
            eastl::copy( tag.GetTag().begin(), tag.GetTag().end(), mNameBuffer.begin() );
            mNameBuffer[tag.GetTag().size()] = '\0';
        }
        // Own an expandable buffer instead of copying an unbounded name into
        // 512 bytes. Keep edits across frames and commit on Enter or focus loss.
        const auto resize{ []( ImGuiInputTextCallbackData* data ) -> core::i32 {
            auto& buffer{ *static_cast<eastl::vector<char>*>( data->UserData ) };
            buffer.resize( static_cast<core::usize>( data->BufSize ) );
            data->Buf = buffer.data();
            return 0;
        } };
        constexpr ImGuiInputTextFlags flags{ ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackResize };
        const bool submitted{ ImGui::InputText( "##EntityName", mNameBuffer.data(), mNameBuffer.size(), flags, resize, &mNameBuffer ) };

        if ( submitted || ImGui::IsItemDeactivatedAfterEdit() ) {
            tag.SetTag( mNameBuffer.data() );
            mNameSource = tag.GetTag();
        }
    }


    auto InspectorPanel::DrawTransformComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        scene::TransformComponent& transformComponent{ entity.GetComponent<scene::TransformComponent>() };

        glm::vec3 newTranslation{ transformComponent.GetTranslation() };
        glm::vec3 newRotation{ transformComponent.GetRotation() };
        glm::vec3 newScale{ transformComponent.GetScale() };

        ImGui::Spacing();

        const bool translationEdited{ DrawVec3Transform( MKT_LOC( "inspector_translation" ).c_str(), newTranslation ) };
        const bool rotationEdited = DrawVec3Transform( MKT_LOC( "inspector_rotation" ).c_str(), newRotation );

        bool uniformScale{ entity.GetComponent<scene::TransformComponent>().HasUniformScale() };
        const bool scaleEdited = DrawVec3Transform( MKT_LOC( "inspector_scale" ).c_str(), newScale, 1.0f );

        ImGui::Spacing();

        // Make separator take up whole window
        // just like separator for components
        ImGui::Unindent();
        ImGui::Separator();
        ImGui::Indent();

        ImGui::Spacing();

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_enable_uniform_scaling", "SetupTransformComponentTab:UniformScale" ).c_str(), uniformScale ) ) {
            transformComponent.SetUniformSale( uniformScale );
        }

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_transform_children", "Transform children" ).c_str(), mState->mTransformChildren ) ) {
            mState->mTransformEditRejected = false;
        }

        imgui::widget::ItemTooltip( MKT_LOC( "inspector_transform_children_hint" ).c_str() );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_rotate_around_selection_center", "Rotate around selection center" ).c_str(), mState->mCenterPivot );

        if ( mState->mTransformEditRejected ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "inspector_transform_children_singular" ).c_str() );
        }

        // Displaying the Inspector must not overwrite gizmo/animation/physics
        // matrices or strip shear from imported and parented transforms.
        if ( !translationEdited && !rotationEdited && !scaleEdited ) {
            mRotating = false;
            return;
        }
        // Stage all fields together so child compensation is applied once and a
        // rejected edit cannot leave the parent or part of its subtree modified.
        auto editedTransform{ transformComponent };

        if ( translationEdited ) {
            editedTransform.SetTranslation( newTranslation );
        }

        if ( rotationEdited ) {
            const bool centerPivot{ mState->mCenterPivot ||
                ( entity.HasComponent<scene::TextComponent>() && entity.GetComponent<scene::TextComponent>().IsWorldText() ) };

            if ( centerPivot && ( !mRotating || mRotationEntity != entity.GetComponent<scene::TagComponent>().GetGuid() ) ) {
                math::Bounds bounds{};
                const auto gather = [&]( auto&& self, const scene::Entity& item, const core::float4x4& relative ) -> void {
                    if ( !item.GetComponent<scene::TagComponent>().IsActive() ) {
                        return;
                    }

                    if ( item.HasComponent<scene::MeshComponent>() ) {
                        const auto& mesh{ item.GetComponent<scene::MeshComponent>() };

                        if ( mesh.HasMesh() ) {
                            const auto localBounds{ mesh.GetMesh()->GetTransformedBounds( relative ) };

                            if ( localBounds.HasExtent() ) {
                                bounds.Include( localBounds.mMinimum );
                                bounds.Include( localBounds.mMaximum );
                            }
                        }
                    }

                    if ( item.HasComponent<scene::TextComponent>() ) {
                        const auto& text{ item.GetComponent<scene::TextComponent>() };

                        if ( text.HasFont() && text.IsWorldText() ) {
                            const auto layout{ text::LayoutText( *text.GetFont(), text.GetUnicodeContents(),
                                { text.GetSize(), text.GetSpacing(), text.GetLineSpacing(),
                                    core::as<text::Alignment>( text.GetAlignment() ), false } ) };

                            if ( !layout.mGlyphs.empty() ) {
                                for ( const core::f32 x: { layout.mMinimum.x, layout.mMaximum.x } ) {
                                    for ( const core::f32 y: { layout.mMinimum.y, layout.mMaximum.y } ) {
                                        bounds.Include( core::float3{ relative * core::float4{ x, y, 0.0f, 1.0f } } );
                                    }
                                }
                            }
                        }
                    }

                    if ( !mState->mTransformChildren ) {
                        return;
                    }

                    for ( const auto childID: item.GetComponent<scene::RelationComponent>().GetChildren() ) {
                        if ( const auto* child{ mState->mActiveScene->FindByID( childID ) } ) {
                            self( self, *child, relative * child->GetComponent<scene::TransformComponent>().GetTransform() );
                        }
                    }
                };
                gather( gather, entity, core::float4x4{ 1.0f } );
                mRotationCenter = bounds.HasExtent() ? ( bounds.mMinimum + bounds.mMaximum ) * 0.5f : core::float3{};
                mRotationEntity = entity.GetComponent<scene::TagComponent>().GetGuid();
            }

            const core::float3 pivot{ centerPivot ? mRotationCenter : core::float3{} };
            const core::float3 before{ editedTransform.GetTransform() * core::float4{ pivot, 1.0f } };
            editedTransform.SetRotation( newRotation );
            const core::float3 after{ editedTransform.GetTransform() * core::float4{ pivot, 1.0f } };
            editedTransform.SetTranslation( editedTransform.GetTranslation() + before - after );
        }

        mRotating = rotationEdited;

        if ( scaleEdited ) {
            editedTransform.SetScale( newScale );
        }

        mState->mTransformEditRejected = !mState->mActiveScene->SetLocalTransform(
            entity.GetComponent<scene::TagComponent>().GetGuid(), editedTransform.GetTransform(), mState->mTransformChildren );

        if ( !mState->mTransformEditRejected ) {
            // Keep the Euler branch entered in the Inspector (for example 370 degrees).
            transformComponent = editedTransform;
        }
    }

    auto InspectorPanel::AssignScriptPayload( scene::Entity& entity, const ImGuiPayload& payload ) -> void {
        if ( !payload.Data || payload.DataSize <= 1 ) {
            return;
        }

        const auto* bytes{ static_cast<const char*>( payload.Data ) };

        if ( bytes[payload.DataSize - 1] != '\0' ) {
            return;
        }
        // Reject embedded terminators rather than silently accepting a truncated path.
        for ( int index{}; index < payload.DataSize - 1; ++index ) {
            if ( bytes[index] == '\0' ) {
                return;
            }
        }

        try {
            const filesystem::Path path{ bytes };
            const auto extension{ path.GetNormalizedExtension() };

            if ( extension != "lua" && extension != "cs" && extension != "cpp" ) {
                return;
            }

            const auto language{ extension == "lua" ? scripting::ScriptLanguage::eLua : extension == "cpp" ? scripting::ScriptLanguage::eNative
                                                                                                           : scripting::ScriptLanguage::eCSharp };
            // Loading is entity-bound and must stay on the editor thread.
            // Do not create a component until loading has succeeded.
            auto script{ scripting::ScriptingService::Get()->LoadScript( language, path, &entity ) };

            if ( !script ) {
                core::RuntimeConsole::Get()->Error( string::Format( fmt::runtime( MKT_LOC( "inspector_could_not_assign_script" ).c_str() ), path.GetC_Str() ) );
                return;
            }

            auto& component{ entity.GetOrEmplace<scene::ScriptComponent>( path, language ) };

            if ( language != scripting::ScriptLanguage::eLua ) {
                scripting::ScriptingService::Get()->GetLuaScriptingService()->ReleaseEntityScripts( &entity );
            }

            component.SetScript( script );
            mScriptPreviewPath = {};
        } catch ( const std::exception& error ) {
            core::RuntimeConsole::Get()->Error( string::Format( fmt::runtime( MKT_LOC( "inspector_could_not_assign_script" ).c_str() ), error.what() ) );
        }
    }


    auto InspectorPanel::DrawScriptingComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        scene::ScriptComponent& scriptComponent{ entity.GetComponent<scene::ScriptComponent>() };

        ImGui::Button( imgui::widget::LocalizedLabel( "inspector_drop_script_asset_here", "Drop script asset here" ).c_str(), ImVec2{ ImGui::GetContentRegionAvail().x, 0 } );

        if ( ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_SCRIPT_SOURCE" ) } ) {
                AssignScriptPayload( entity, *payload );
            }

            ImGui::EndDragDropTarget();
        }

        // ScriptLanguage contains eUnknown, so combo positions must map explicitly.
        constexpr scripting::ScriptLanguage languages[]{ scripting::ScriptLanguage::eLua,
            scripting::ScriptLanguage::eCSharp, scripting::ScriptLanguage::eNative };
        const char* languageNames[]{ "Lua", "C# (.NET)", "Native (C++)" };
        int selected{ -1 };

        for ( int index{}; index < 3; ++index ) {
            if ( languages[index] == scriptComponent.GetLanguage() ) {
                selected = index;
            }
        }

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_language", "Language" ).c_str(), &selected, languageNames, 3 ) ) {
            scripting::ScriptingService::Get()->GetLuaScriptingService()->ReleaseEntityScripts( &entity );
            scriptComponent.SetLanguage( languages[selected] );
            mScriptPreviewPath = {};
        }

        eastl::string scriptPath{ scriptComponent.GetFilePath().IsEmpty()
                                      ? MKT_LOC( "inspector_none" ).c_str()
                                      : scriptComponent.GetFilePath().GetC_Str() };
        ImGui::SetNextItemWidth( -1.0f );
        ImGui::InputText( "##ScriptPath", scriptPath.data(), scriptPath.size() + 1,
            ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll );
        ImGui::Spacing();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_create_script", "Create script" ).c_str() ) ) {
            try {
                auto script{ scripting::ScriptingService::Get()->CreateScript( scriptComponent.GetLanguage(), &entity ) };

                if ( script ) {
                    scriptComponent.SetScript( script );
                    mScriptPreviewPath = {};
                }
            } catch ( const std::exception& error ) {
                core::RuntimeConsole::Get()->Error( error.what() );
            }
        }

        if ( !scriptComponent.GetFilePath().IsEmpty() ) {
            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_show_in_explorer", "Show in Explorer" ).c_str() ) ) {
                filesystem::OpenInExplorer( scriptComponent.GetFilePath() );
            }
        }

        if ( scriptComponent.GetLanguage() == scripting::ScriptLanguage::eCSharp && !scripting::DotnetScriptingService::IsSupported() ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "inspector_c_support_is_disabled_in_this_build_configure_mikoto_enable" ).c_str() );
        }

        if ( !scriptComponent.GetFilePath().IsEmpty() ) {
            DrawScriptProperties( scriptComponent );
        }

        // Preview the assigned source immediately; FileService refreshes its
        // cached contents when the watcher detects an external edit.
        if ( scriptComponent.GetFilePath().IsEmpty() ) {
            return;
        }

        if ( scriptComponent.GetLanguage() == scripting::ScriptLanguage::eCSharp && scriptComponent.GetFilePath().GetNormalizedExtension() != "cs" ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_source_preview_is_available_for_cs_files" ).c_str() );
            return;
        }

        const filesystem::FileHandle file{ filesystem::FileService::Get()->LoadFile( scriptComponent.GetFilePath() ) };

        if ( file.IsEmpty() ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_cannot_read_the_selected_script" ).c_str() );
            return;
        }

        ImGui::Spacing();
        ImGui::SeparatorText( MKT_LOC( "inspector_script_preview" ).c_str() );

        const auto contents{ file->GetContentsString() };
        const bool sourceChanged{ mScriptPreviewPath != file->GetPath() };

        if ( sourceChanged || mScriptPreviewText != contents ) {
            const auto cursor{ mTextEditor.GetCursorPosition() };
            mTextEditor.SetText( contents );

            if ( !sourceChanged ) {
                mTextEditor.SetSelection( cursor, cursor );
            }

            mScriptPreviewPath = file->GetPath();
            mScriptPreviewText = contents;
        }

        // Style
        ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4{ 0.1f, 0.1f, 0.1f, 0.5f } );
        ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 6.0f );

        const core::f32 height{ ImGui::GetTextLineHeightWithSpacing() * 15.0f };

        if ( ImGui::BeginChild( "ScriptPreviewChild", ImVec2{ 0, height }, true, ImGuiWindowFlags_HorizontalScrollbar ) ) {
            imgui::ImGuiScopedTextFont newFont{ imgui::ImGuiService::Get()->PushFont( "./Resources/Fonts/Google_Sans_Code/static/GoogleSansCode-Light.ttf" ) };

            switch ( scriptComponent.GetLanguage() ) {
                case scripting::ScriptLanguage::eLua:
                    mTextEditor.SetLanguage( TextLanguage::eLua );
                    break;
                case scripting::ScriptLanguage::eNative:
                    mTextEditor.SetLanguage( TextLanguage::eCpp );
                    break;
                case scripting::ScriptLanguage::eCSharp:
                    mTextEditor.SetLanguage( TextLanguage::eCSharp );
                    break;
                default:;
            }

            mTextEditor.Draw( "##ScriptPreviewComponent", ImGui::GetContentRegionAvail() );
        }

        ImGui::EndChild();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_edit_script", "Edit script" ).c_str() ) ) {
            mScriptEditorLaunchFailed = !OpenInTextEditor( file->GetPath(), mState->mExternalEditor );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_editor_preview", "Editor Preview" ).c_str() ) ) {
            mDisplayScriptPreview = !mDisplayScriptPreview;
        }

        if ( mDisplayScriptPreview ) {
            if ( imgui::BeginWindow( imgui::widget::LocalizedLabel( "inspector_script_visualizer_scriptvisualizerpreviewwindow", "Script visualizer##ScriptVisualizerPreviewWindow" ).c_str(), MKT_ADDRESSOF( mDisplayScriptPreview ), ImGuiWindowFlags_MenuBar ) ) {
                imgui::ImGuiScopedTextFont newFont{ imgui::ImGuiService::Get()->PushFont( "./Resources/Fonts/Google_Sans_Code/static/GoogleSansCode-Light.ttf" ) };

                if ( ImGui::BeginMenuBar() ) {
                    if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "inspector_edit", "Edit" ).c_str() ) ) {
                        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "inspector_copy", "Copy" ).c_str(), "Ctrl+C", false, mTextEditor.HasSelection() ) ) {
                            mTextEditor.Copy();
                        }

                        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "inspector_select_all", "Select All" ).c_str(), "Ctrl+A" ) ) {
                            mTextEditor.SelectAll();
                        }

                        ImGui::EndMenu();
                    }

                    if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "inspector_view", "View" ).c_str() ) ) {
                        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "inspector_dark_palette", "Dark Palette" ).c_str() ) ) {
                            mTextEditor.SetPalette( TextEditorPalette::eDark );
                        }

                        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "inspector_light_palette", "Light Palette" ).c_str() ) ) {
                            mTextEditor.SetPalette( TextEditorPalette::eLight );
                        }

                        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "inspector_retro_blue_palette", "Retro Blue Palette" ).c_str() ) ) {
                            mTextEditor.SetPalette( TextEditorPalette::eRetroBlue );
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenuBar();
                }

                const auto cursor{ mTextEditor.GetCursorPosition() };
                ImGui::Text(
                    MKT_LOC( "inspector_lines" ).c_str(),
                    cursor.mLine + 1,
                    cursor.mColumn + 1,
                    mTextEditor.GetTotalLines(),
                    mTextEditor.IsOverwrite() ? MKT_LOC( "inspector_ovr" ).c_str() : MKT_LOC( "inspector_ins" ).c_str(),
                    mTextEditor.CanUndo() ? "*" : " ",
                    TextEditor::GetLanguageName( mTextEditor.GetLanguage() ) );

                ImGui::Separator();

                ImVec2 editorSize{};

                if ( editorSize.x <= 0.0f ) {
                    editorSize.x = ImGui::GetContentRegionAvail().x;
                }

                if ( editorSize.y <= 0.0f ) {
                    editorSize.y = ImGui::GetContentRegionAvail().y;
                }

                mTextEditor.Draw( "##ScriptPreviewComponentWindow", editorSize );
            }

            ImGui::End();
        }

        if ( mScriptEditorLaunchFailed ) {
            ImGui::TextWrapped( MKT_LOC( "inspector_could_not_open_the_editor_choose_its_executable_in_editor" ).c_str() );
        }
    }


    auto InspectorPanel::DrawAnimatorComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        const auto id{ entity.GetComponent<scene::AnimatorComponent>().GetAnimatorID() };

        if ( auto animator{ animation::AnimationSystem::Get()->GetAnimator( id ) } ) {
            DrawAnimatorSettings( *animator );
        }
        else {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_no_animator_is_assigned_to_this_entity" ).c_str() );
        }
    }

    auto InspectorPanel::DrawSkinMeshComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        scene::SkinnedMeshRenderer& skinnedMeshRendererComp{ entity.GetComponent<scene::SkinnedMeshRenderer>() };

        if ( !entity.HasComponent<scene::MeshComponent>() || !entity.GetComponent<scene::MeshComponent>().HasMesh() ) {
            ImGui::TextDisabled( "Assign a resident mesh to inspect its skeleton and animator." );
            return;
        }

        const auto& mesh{ entity.GetComponent<scene::MeshComponent>() };
        const auto model{ mesh.GetModel() };
        const core::u32 rig{ mesh.GetMesh()->GetSkinBinding().mRigIndex };
        const auto* skeleton{ model ? model->GetSkeleton( rig ) : nullptr };

        if ( skeleton && skeleton->GetOzzSkeleton() ) {
            ImGui::Text( MKT_LOC( "inspector_skeleton" ).c_str(), model->GetRigs()[rig].mName.c_str() );
            ImGui::TextDisabled( "%s", model->GetPath().GetC_Str() );
            ImGui::Text( MKT_LOC( "inspector_pose_nodes" ).c_str(), skeleton->GetOzzSkeleton()->num_joints() );

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "animator_inspector_bone_hierarchy", "BoneHierarchy" ).c_str() ) ) {
                DrawSkeletonHierarchy( *skeleton );
            }
        }

        auto* scene{ mState->mActiveScene };

        if ( !scene || !model ) {
            return;
        }

        auto animators{ scene->GetRegistry().view<scene::AnimatorComponent, scene::TagComponent>() };
        scene::Entity* selectedOwner{};

        for ( const auto handle: animators ) {
            if ( animators.get<scene::AnimatorComponent>( handle ).GetAnimatorID() == skinnedMeshRendererComp.GetAnimatorID() ) {
                selectedOwner = scene->FindByHandle( handle );
                break;
            }
        }

        const auto assign = [&]( scene::Entity* owner ) {
            if ( !owner || !owner->HasComponent<scene::AnimatorComponent>() ) {
                return;
            }

            const auto id{ owner->GetComponent<scene::AnimatorComponent>().GetAnimatorID() };
            const auto* animator{ animation::AnimationSystem::Get()->GetAnimator( id ) };

            if ( animator && animator->GetModel() == model ) {
                skinnedMeshRendererComp.SetAnimatorID( id );
            }
        };

        const char* label{ selectedOwner ? selectedOwner->GetComponent<scene::TagComponent>().GetTag().c_str() : "Unassigned" };

        if ( ImGui::BeginCombo( "Animator entity", label ) ) {
            if ( ImGui::Selectable( "None", !selectedOwner ) ) {
                skinnedMeshRendererComp.SetAnimatorID( 0 );
            }

            for ( const auto handle: animators ) {
                const auto id{ animators.get<scene::AnimatorComponent>( handle ).GetAnimatorID() };
                const auto* animator{ animation::AnimationSystem::Get()->GetAnimator( id ) };

                if ( !animator || animator->GetModel() != model ) {
                    continue;
                }

                ImGui::PushID( static_cast<int>( handle ) );

                if ( ImGui::Selectable( animators.get<scene::TagComponent>( handle ).GetTag().c_str(),
                         id == skinnedMeshRendererComp.GetAnimatorID() ) ) {
                    assign( scene->FindByHandle( handle ) );
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        if ( ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_ENTITY" ) }; payload && payload->DataSize == sizeof( core::u64 ) ) {
                assign( scene->FindByID( *static_cast<const core::u64*>( payload->Data ) ) );
            }

            ImGui::EndDragDropTarget();
        }

        if ( selectedOwner && ImGui::Button( "Select animator entity" ) ) {
            mState->SelectEntity( selectedOwner );
        }

        ImGui::TextDisabled( "Rig index: %u | Animator: %llu", rig, skinnedMeshRendererComp.GetAnimatorID() );
        ImGui::TextWrapped( "Only animators from this mesh's imported model are compatible. The armature and inverse bind poses belong to the imported skin." );
    }

    auto InspectorPanel::DrawMaterialComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        // ImGui by default will indent because the items in this function because this method is run inside the DrawComponent function,
        // so we are within a Tree Node, items within a tree node appear indented by default when you expand it
        ImGui::Unindent();

        scene::MaterialComponent& materialComponent{ entity.GetComponent<scene::MaterialComponent>() };

        if ( materialComponent.HasMaterial() ) {
            EditMaterial( materialComponent, mState->mSandboxMode );
        }
        else {
            ( void )imgui::InputText( MKT_LOC( "inspector_mesh_has_no_material" ).c_str(), true );

            ImGui::SameLine();

            eastl::string createMaterialID{ string::Format( fmt::runtime( MKT_LOC( "inspector_create_creatematerialid" ).c_str() ), ICON_MD_CREATE ) };

            if ( ImGui::Button( createMaterialID.c_str() ) ) {
                materialComponent.SetIsInternal( false );
                materialComponent.SetMaterial( asset::AssetRegistryService::Get()->CreateMaterial( material::SurfaceMaterialDescription{} ) );
            }

            imgui::SetCursorHandOnLastItemHovered();
            ImGui::SameLine();

            eastl::string loadMaterialID{ string::Format( fmt::runtime( MKT_LOC( "inspector_load_loadmaterialid" ).c_str() ), ICON_MD_SAVE_ALT ) };

            if ( ImGui::Button( loadMaterialID.c_str() ) ) {
                const std::initializer_list<filesystem::FileDialogPair> filters{
                    { MKT_LOC( "inspector_material" ).c_str(), filesystem::kMikotoMaterialExtension.data() }
                };
                filesystem::Path materialPath{ OpenFileDialog( filters ) };

                if ( !materialPath.IsEmpty() ) {
                    if ( auto loaded{ asset::AssetRegistryService::Get()->DeSerializeMaterial( materialPath ) } ) {
                        materialComponent.SetIsInternal( false );
                        materialComponent.SetMaterial( loaded );
                    }
                }
            }

            imgui::SetCursorHandOnLastItemHovered();
        }

        ImGui::Indent();
    }

    auto InspectorPanel::DrawLandscapeComponentTab( scene::Entity& ) -> void {
    }


    template<typename Component>
    auto InspectorPanel::DrawExtendedPhysicsComponentTab( scene::Entity& entity ) -> void {
        auto& component{ entity.GetComponent<Component>() };
        scene::VisitPhysicsFields( component, [&]( const char* name, auto& value ) {
            if constexpr ( std::is_same_v<Component, scene::ClothComponent> ) {
                if ( std::strcmp( name, "Drag" ) == 0 || std::strcmp( name, "Lift" ) == 0 ) {
                    ImGui::BeginDisabled();
                    EditPhysicsField( name, value, mState->mActiveScene );
                    ImGui::EndDisabled();

                    if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) ) {
                        ImGui::SetTooltip( "Reserved authoring value: the bundled Jolt soft-body API has no aerodynamic drag/lift solver." );
                    }

                    return false;
                }
            }

            return EditPhysicsField( name, value, mState->mActiveScene );
        } );
        const auto* error{ mState->mActiveScene->GetPhysicsWorld()->GetColliderError( entity.Get() ) };

        if ( error && *error ) {
            ImGui::TextWrapped( "%s", error );
        }
    }

    auto InspectorPanel::DrawVehicleComponentTab( scene::Entity& entity ) -> void {
        auto& vehicle{ entity.GetComponent<scene::VehicleComponent>() };
        auto type{ vehicle.GetVehicleType() };

        if ( EditPhysicsField( "VehicleType", type, mState->mActiveScene ) ) {
            vehicle.SetVehicleType( type );
        }

        auto wheels{ vehicle.GetWheelDescriptions() };

        if ( EditPhysicsField( "Wheels", wheels, mState->mActiveScene ) ) {
            vehicle.SetWheelDescriptions( wheels );
        }

        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_half_track_m", "Half track (m)" ).c_str(), &vehicle.mHalfTrack, 0.01f, 0.1f, 5.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_half_wheelbase_m", "Half wheelbase (m)" ).c_str(), &vehicle.mHalfWheelbase, 0.01f, 0.1f, 10.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_wheel_radius_m", "Wheel radius (m)" ).c_str(), &vehicle.mWheelRadius, 0.01f, 0.05f, 3.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_wheel_width_m", "Wheel width (m)" ).c_str(), &vehicle.mWheelWidth, 0.01f, 0.01f, 2.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_suspension_minimum_m", "Suspension minimum (m)" ).c_str(), &vehicle.mSuspensionMin, 0.01f, 0.01f, 2.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_suspension_maximum_m", "Suspension maximum (m)" ).c_str(), &vehicle.mSuspensionMax, 0.01f, 0.01f, 3.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_steering_limit_degrees", "Steering limit (degrees)" ).c_str(), &vehicle.mMaxSteerDegrees, 0.5f, 1.0f, 80.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_engine_torque_nm", "Engine torque (Nm)" ).c_str(), &vehicle.mEngineTorque, 10.0f, 1.0f, 10000.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_wheel_anchor_height_m", "Wheel anchor height (m)" ).c_str(), &vehicle.mWheelAnchorHeight, 0.01f, -3.0f, 3.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_center_of_mass_offset_y_m", "Center of mass offset Y (m)" ).c_str(), &vehicle.mCenterOfMassOffsetY, 0.01f, -3.0f, 3.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_suspension_frequency_hz", "Suspension frequency (Hz)" ).c_str(), &vehicle.mSuspensionFrequency, 0.05f, 0.1f, 10.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_suspension_damping", "Suspension damping" ).c_str(), &vehicle.mSuspensionDamping, 0.01f, 0.0f, 2.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_maximum_pitch_roll_degrees", "Maximum pitch / roll (degrees)" ).c_str(), &vehicle.mMaxPitchRollDegrees, 1.0f, 1.0f, 180.0f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_longitudinal_grip_multiplier", "Longitudinal grip multiplier" ).c_str(), &vehicle.mLongitudinalImpulseScale, 0.1f, 0.1f, 20.0f );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_four_wheel_drive", "Four-wheel drive" ).c_str(), vehicle.mFourWheelDrive );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_cylinder_wheel_casts", "Cylinder wheel casts" ).c_str(), vehicle.mCylinderWheelCast );

        if ( !vehicle.IsValid() ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "inspector_vehicle_disabled_settings_must_be_finite_and_positive_maximum_suspension" ).c_str() );
        }

        if ( !entity.HasComponent<scene::RigidBodyComponent>() || !entity.GetComponent<scene::RigidBodyComponent>().IsDynamic() ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "inspector_add_a_dynamic_rigid_body_to_enable_the_vehicle" ).c_str() );
        }
    }

    auto InspectorPanel::DrawWaterComponentTab( scene::Entity& entity ) -> void {
        auto& water{ entity.GetComponent<scene::WaterRenderComponent>() };
        const auto planeLabel{ MKT_LOC( "water_plane" ) };
        const auto customLabel{ MKT_LOC( "water_custom" ) };
        const char* shapes[]{ planeLabel.c_str(), customLabel.c_str() };
        int shape{ static_cast<int>( water.GetShape() ) };

        if ( ImGui::Combo( MKT_LOC( "water_shape" ).c_str(), &shape, shapes, 2 ) ) {
            water.SetShape( static_cast<scene::WaterShape>( shape ) );
        }

        if ( water.GetShape() != scene::WaterShape::ePlane ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "water_custom_pending" ).c_str() );
            return;
        }

        auto size{ water.GetPlaneDimensions() };
        bool resize{ imgui::widget::CircleSlider( MKT_LOC( "water_width" ), &size.x, .01f, 100000.f, "%.2f" ) };
        resize |= imgui::widget::CircleSlider( MKT_LOC( "water_depth" ), &size.y, .01f, 100000.f, "%.2f" );

        if ( resize ) {
            water.SetPlaneDimensions( size );
        }

        const auto resolution{ water.GetPlaneResolution() };
        core::i32 cellsX{ static_cast<core::i32>( resolution.x ) };
        core::i32 cellsZ{ static_cast<core::i32>( resolution.y ) };

        bool subdivide{ imgui::widget::CircleInt( MKT_LOC( "water_cells_x" ), &cellsX, 1, 1024 ) };
        subdivide |= imgui::widget::CircleInt( MKT_LOC( "water_cells_z" ), &cellsZ, 1, 1024 );

        if ( subdivide ) {
            water.SetPlaneResolution( core::float2{ cellsX, cellsZ } );
        }

        ImGui::Text( "%s: %u   %s: %u", MKT_LOC( "water_vertices" ).c_str(),
            ( cellsX + 1 ) * ( cellsZ + 1 ), MKT_LOC( "water_triangles" ).c_str(), cellsX * cellsZ * 2 );
        ImGui::SeparatorText( MKT_LOC( "render_debug_view" ).c_str() );
        const char* debugViews[]{ MKT_LOC( "render_debug_none" ).c_str(),
            MKT_LOC( "render_debug_vertices" ).c_str(),
            MKT_LOC( "render_debug_grid" ).c_str(),
            MKT_LOC( "render_debug_vertices_grid" ).c_str() };
        int debugView{ ( water.AreVerticesVisible() ? 1 : 0 ) | ( water.IsGridVisible() ? 2 : 0 ) };
        ImGui::SetNextItemWidth( -1.0f );

        if ( ImGui::Combo( "##WaterDebugView", &debugView, debugViews, IM_ARRAYSIZE( debugViews ) ) ) {
            water.SetShowVertices( ( debugView & 1 ) != 0 );
            water.SetShowGrid( ( debugView & 2 ) != 0 );
        }

        auto debugColor{ water.GetDebugPreviewColor() };

        if ( ImGui::ColorEdit3( imgui::widget::LocalizedLabel( "render_debug_preview_color", "WaterDebugPreviewColor" ).c_str(), &debugColor.x ) ) {
            water.SetDebugPreviewColor( debugColor );
        }

        auto foam{ water.GetFoamAmount() };

        if ( imgui::widget::CircleSlider( MKT_LOC( "water_foam_amount" ), &foam, 0.0f, 1.0f ) ) {
            water.SetFoamAmount( foam );
        }

        if ( ImGui::TreeNodeEx( "WaterSurface", ImGuiTreeNodeFlags_DefaultOpen, "%s", MKT_LOC( "material_dielectric" ).c_str() ) ) {
            auto surface{ water.GetMaterial() };
            auto color{ surface->GetBaseColorFactor() };

            if ( ImGui::ColorEdit3( MKT_LOC( "inspector_base_color" ).c_str(), &color.x ) ) {
                surface->SetBaseColorFactor( color );
            }

            auto roughness{ surface->GetRoughnessFactor() };

            if ( imgui::widget::CircleSlider( MKT_LOC( "inspector_roughness" ), &roughness, 0.04f, 1.0f ) ) {
                surface->SetRoughnessFactor( roughness );
            }

            auto settings{ surface->GetDielectric() };
            bool changed{};
            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_ior" ), &settings.mIndexOfRefraction, 1.0f, 2.5f, "%.3f" );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_transmission" ), &settings.mTransmission, 0.0f, 1.0f );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_thickness" ), &settings.mThickness, 0.0f, 100.0f );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "material_attenuation_distance" ), &settings.mAttenuationDistance, 0.001f, 1000.0f );
            changed |= ImGui::ColorEdit3( MKT_LOC( "material_attenuation_color" ).c_str(), &settings.mAttenuationColor.x );

            if ( changed ) {
                surface->SetDielectric( settings );
            }

            ImGui::TreePop();
        }
    }

    auto InspectorPanel::DrawWaterSimulationComponentTab( scene::Entity& entity ) -> void {
        auto& water{ entity.GetComponent<scene::WaterSimulationComponent>() };
        const auto sines{ MKT_LOC( "water_sines" ) };
        const auto fft{ MKT_LOC( "water_fft" ) };
        const char* types[]{ sines.c_str(), "Gerstner", fft.c_str() };
        auto type{ core::as<core::i32>( water.GetType() ) };

        if ( ImGui::Combo( MKT_LOC( "water_simulation_type" ).c_str(), &type, types, IM_ARRAYSIZE( types ) ) ) {
            water.SetType( static_cast<scene::WaterSimulationType>( type ) );
        }

        auto timeScale{ water.GetTimeScale() };

        if ( imgui::widget::CircleSlider( MKT_LOC( "water_time_scale" ), &timeScale, 0.f, 10.f, "%.2f" ) ) {
            water.SetTimeScale( timeScale );
        }

        if ( water.GetType() == scene::WaterSimulationType::eGerstner ) {
            auto steepness{ water.GetSteepness() };

            if ( imgui::widget::CircleSlider( MKT_LOC( "water_steepness" ), &steepness, 0.0f, 0.95f ) ) {
                water.SetSteepness( steepness );
            }
        }

        if ( ImGui::Button( MKT_LOC( "water_reset_time" ).c_str() ) ) {
            water.ResetTime();
        }

        if ( water.GetType() == scene::WaterSimulationType::eFastFourierTransform ) {
            using FftSettings = scene::WaterSimulationComponent::FftSettings;
            auto settings{ water.GetFftSettings() };
            bool changed{};

            changed |= imgui::widget::CircleSlider( MKT_LOC( "water_fft_patch_length" ), &settings.mPatchLength,
                FftSettings::kMinPatchLength, FftSettings::kMaxPatchLength, "%.1f" );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "water_fft_wind_speed" ), &settings.mWindSpeed,
                0.0f, FftSettings::kMaxWindSpeed, "%.1f" );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "water_fft_wind_direction" ), &settings.mWindDirection, -180.0f, 180.0f, "%.1f" );

            changed |= imgui::widget::CircleSlider( MKT_LOC( "water_amplitude" ), &settings.mAmplitude, 0.0f, FftSettings::kMaxAmplitude, "%.2f" );
            changed |= imgui::widget::CircleSlider( MKT_LOC( "water_fft_choppiness" ), &settings.mChoppiness, 0.0f, FftSettings::kMaxChoppiness, "%.2f" );
            changed |= ImGui::InputScalar( MKT_LOC( "inspector_random_seed" ).c_str(), ImGuiDataType_U32, &settings.mSeed );

            if ( changed ) {
                water.SetFftSettings( settings );
            }

            return;
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "water_wave_units" ).c_str() );
        auto waves{ water.GetWaves() };
        bool changed{};
        int remove{ -1 };

        for ( core::usize i{}; i < waves.size(); ++i ) {
            ImGui::PushID( static_cast<int>( i ) );

            if ( ImGui::TreeNodeEx( "Wave", ImGuiTreeNodeFlags_DefaultOpen, "%s %u", MKT_LOC( "water_wave" ).c_str(), static_cast<unsigned>( i + 1 ) ) ) {
                auto& wave{ waves[i] };
                float direction{ glm::degrees( std::atan2( wave.mDirection.y, wave.mDirection.x ) ) };

                if ( imgui::widget::CircleSlider( MKT_LOC( "water_direction" ), &direction, -180.0f, 180.0f, "%.1f" ) ) {
                    wave.mDirection = { std::cos( glm::radians( direction ) ), std::sin( glm::radians( direction ) ) };
                    changed = true;
                }

                changed |= imgui::widget::CircleSlider( MKT_LOC( "water_amplitude" ), &wave.mAmplitude, 0.f, 100.f, "%.2f" );
                changed |= imgui::widget::CircleSlider( MKT_LOC( "water_wavelength" ), &wave.mWavelength, .01f, 10000.f, "%.2f" );
                changed |= imgui::widget::CircleSlider( MKT_LOC( "water_speed" ), &wave.mSpeed, -100.f, 100.f, "%.2f" );
                changed |= imgui::widget::CircleSlider( MKT_LOC( "water_phase" ), &wave.mPhaseOffset, -glm::two_pi<float>(), glm::two_pi<float>(), "%.2f" );

                if ( ImGui::Button( MKT_LOC( "water_remove_wave" ).c_str() ) ) {
                    remove = static_cast<int>( i );
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if ( remove >= 0 ) {
            waves.erase( waves.begin() + remove );
            changed = true;
        }

        if ( changed ) {
            water.SetWaves( eastl::move( waves ) );
        }

        ImGui::BeginDisabled( water.GetWaveCount() >= scene::WaterSimulationComponent::kMaxWaves );

        if ( ImGui::Button( MKT_LOC( "water_add_wave" ).c_str() ) ) {
            water.AddWave( {} );
        }

        ImGui::EndDisabled();
        ImGui::SameLine();

        if ( ImGui::Button( MKT_LOC( "water_clear_waves" ).c_str() ) ) {
            water.ClearWaves();
        }
    }

    auto InspectorPanel::DrawVehicleWheelComponentTab( scene::Entity& entity ) -> void {
        auto& wheel{ entity.GetComponent<scene::VehicleWheelComponent>() };
        core::i32 index{ static_cast<core::i32>( wheel.mIndex ) };
        // Tracks and custom vehicles can have more than four visual wheel bindings.
        if ( ImGui::InputInt( imgui::widget::LocalizedLabel( "inspector_wheel_index", "Wheel index" ).c_str(), &index ) ) {
            wheel.mIndex = static_cast<core::u32>( eastl::max( 0, index ) );
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "inspector_parent_directly_to_the_vehicle_chassis_set_visual_scale_orientation" ).c_str() );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_capture_visual_scale_and_rotation", "Capture visual scale and rotation" ).c_str() ) ) {
            wheel.mVisualCorrection = entity.GetComponent<scene::TransformComponent>().GetTransform();
            wheel.mVisualCorrection[3] = core::float4{ 0, 0, 0, 1 };
        }
    }

    auto InspectorPanel::DrawPhysicsComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::RigidBodyComponent>() ) {
            return;
        }

        auto& rb{ entity.GetComponent<scene::RigidBodyComponent>() };

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "inspector_rigid_body", "Rigid Body" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {

            // --- Body Type ---
            eastl::array bodyTypes{ MKT_LOC( "inspector_static" ).c_str(), MKT_LOC( "inspector_kinematic" ).c_str(), MKT_LOC( "inspector_dynamic" ).c_str() };
            core::i32 currentType{ core::as<core::i32>( rb.GetBodyType() ) };

            ImGui::Text( "%s", MKT_LOC( "inspector_body_type" ).c_str() );
            ImGui::SameLine( ImGui::GetWindowWidth() * 0.25f );

            if ( ImGui::Combo( "##BodyType", &currentType, bodyTypes.data(), bodyTypes.size() ) ) {
                rb.SetBodyType( static_cast<scene::RigidBodyComponent::BodyType>( currentType ) );
            }

            // --- Use Gravity ---
            bool useGravity{ rb.UseGravity() };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_use_gravity", "Use Gravity" ).c_str(), useGravity ) ) {
                rb.SetUseGravity( useGravity );
            }

            // --- Mass ---
            float mass{ rb.GetMass() };
            ImGui::Text( "%s", MKT_LOC( "inspector_mass" ).c_str() );
            ImGui::SameLine( ImGui::GetWindowWidth() * 0.25f );

            if ( ImGui::DragFloat( "##PhysicsTabMass", &mass, 0.1f, 0.0f, 1000.0f, "%.2f" ) ) {
                rb.SetMass( mass );
            }

            // --- Friction ---
            float friction{ rb.GetFriction() };
            ImGui::Text( "%s", MKT_LOC( "inspector_friction" ).c_str() );
            ImGui::SameLine( ImGui::GetWindowWidth() * 0.25f );

            if ( ImGui::SliderFloat( "##PhysicsTabFriction", &friction, 0.0f, 1.0f, "%.2f" ) ) {
                rb.SetFriction( friction );
            }

            float restitution{ rb.GetRestitution() };
            ImGui::Text( "%s", MKT_LOC( "inspector_restitution_bounciness" ).c_str() );
            ImGui::SameLine( ImGui::GetWindowWidth() * 0.25f );

            if ( ImGui::SliderFloat( "##PhysicsTabRestitution", &restitution, 0.0f, 5.0f ) ) {
                rb.SetRestitution( restitution );
            }

            // --- Internal Handle Info (read-only) ---
            if ( rb.IsValidBodyID() ) {
                const auto handle{ rb.GetBodyID() };

                ImGui::Separator();
                ImGui::TextDisabled( "%s", MKT_LOC( "inspector_internal_handle" ).c_str() );
                ImGui::SameLine();
                ImGui::Text( MKT_LOC( "inspector_body_id" ).c_str(), as<ull>( handle ) );
            }
            else {
                ImGui::TextDisabled( "%s", MKT_LOC( "inspector_internal_handle" ).c_str() );
                ImGui::SameLine();
                ImGui::Text( "%s", MKT_LOC( "inspector_no_bodyid" ).c_str() );
            }

            // --- Linear Velocity ---
            core::float3 linearVel{ rb.GetLinearVelocity() };
            ImGui::Text( "%s", MKT_LOC( "inspector_linear_velocity" ).c_str() );
            ImGui::SameLine( ImGui::GetWindowWidth() * 0.35f );

            if ( ImGui::DragFloat3( "##LinearVelocity", &linearVel.x, 0.1f ) ) {
                rb.SetLinearVelocity( linearVel );
            }

            // --- Angular Velocity ---
            core::float3 angularVel{ rb.GetAngularVelocity() };
            ImGui::Text( "%s", MKT_LOC( "inspector_angular_velocity" ).c_str() );
            ImGui::SameLine( ImGui::GetWindowWidth() * 0.35f );

            if ( ImGui::DragFloat3( "##AngularVelocity", &angularVel.x, 0.1f ) ) {
                rb.SetAngularVelocity( angularVel );
            }

            ImGui::Spacing();
            ImGui::Separator();
        }
    }

    auto InspectorPanel::DrawRenderComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::MeshComponent>() ) {
            return;
        }

        ImGui::Unindent();
        ImGui::Spacing();

        const auto sourceLabel{ string::Format( fmt::runtime( MKT_LOC( "inspector_source" ).c_str() ), ICON_MD_ARCHIVE ) };
        const auto loadLabel{ string::Format( fmt::runtime( MKT_LOC( "inspector_load_drawrendercomponenttab_searchbutton" ).c_str() ), ICON_MD_SEARCH ) };
        const auto& style{ ImGui::GetStyle() };
        const core::f32 sourceWidth{ ImGui::CalcTextSize( sourceLabel.c_str(), nullptr, true ).x + 2.0f * style.FramePadding.x };
        const core::f32 loadWidth{ ImGui::CalcTextSize( loadLabel.c_str(), nullptr, true ).x + 2.0f * style.FramePadding.x };
        const core::f32 gap{ style.ItemSpacing.x };
        const core::f32 pathMinWidth{ ImGui::GetFontSize() * 5.0f };
        const bool fitsOnOneRow{ ImGui::GetContentRegionAvail().x >= sourceWidth + gap + pathMinWidth + gap + loadWidth };

        ImGui::BeginDisabled();
        ImGui::Button( sourceLabel.c_str() );
        ImGui::EndDisabled();

        if ( fitsOnOneRow ) {
            ImGui::SameLine();
        }

        scene::MeshComponent& component{ entity.GetComponent<scene::MeshComponent>() };
        eastl::string path{ component.GetModelPath().GetC_Str() };
        const core::f32 available{ ImGui::GetContentRegionAvail().x };
        const bool loadFitsBesidePath{ available >= pathMinWidth + gap + loadWidth };

        // Reserve the translated button's width before drawing the path.
        ImGui::SetNextItemWidth( loadFitsBesidePath ? available - loadWidth - gap : -FLT_MIN );
        ImGui::InputText( "##DrawRenderComponentTab_PathToModel", path.data(), path.size() + 1, ImGuiInputTextFlags_ReadOnly );
        if ( !path.empty() ) {
            imgui::widget::ItemTooltip( "%s", path.c_str() );
        }

        if ( loadFitsBesidePath ) {
            ImGui::SameLine();
        }

        if ( ImGui::Button( loadLabel.c_str() ) ) {
            scene::Scene* scene{ mState->mActiveScene };
            const entt::entity rootHandle{ entity.Get() };
            threading::TaskService::Get()->ExecuteWorkerTask( [scene, rootHandle]() -> void {
                const std::initializer_list<filesystem::FileDialogPair> filters{
                    { MKT_LOC( "inspector_model_files" ).c_str(), "obj,gltf,fbx,glb" },
                    { MKT_LOC( "inspector_obj_files" ).c_str(), "obj" },
                    { MKT_LOC( "inspector_gltf_files" ).c_str(), "gltf" },
                    { MKT_LOC( "inspector_fbx_files" ).c_str(), "fbx" },
                    { MKT_LOC( "inspector_glb_files" ).c_str(), "glb" }
                };
                filesystem::Path modelPath{ OpenFileDialog( filters ) };

                if ( !modelPath.IsEmpty() ) {
                    asset::ModelHandle model{ asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( modelPath ) };
                    threading::TaskService::Get()->PushMainThreadTask( [scene, rootHandle, model]() -> void {
                        if ( !scene || model.IsEmpty() ) {
                            return;
                        }

                        const scene::EntityCreateInfo entityCreateInfo{
                            .mRoot = scene->FindByHandle( rootHandle ),
                            .mName{ model->GetName() },
                            .mModel = model
                        };
                        scene->PushEntity( entityCreateInfo );
                    } );
                }
            } );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DrawMeshInfo( component );

        ImGui::Indent();
    }

    auto InspectorPanel::DrawLightComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        scene::LightComponent& lightComponent{ entity.GetComponent<scene::LightComponent>() };

        struct LightOption {
            renderer::LightType mType{};
            core::cstr mLabelKey{};
        };

        constexpr std::array<LightOption, 4> lightTypes{ { { renderer::LightType::eDirectional, "inspector_directional_light" },
            { renderer::LightType::ePoint, "inspector_point_light" },
            { renderer::LightType::eSpot, "inspector_spot_light" },
            { renderer::LightType::eAreaLight, "inspector_area_light" } } };

        const auto lightType{ lightComponent.GetActiveType() };
        eastl::string preview{ MKT_LOC( "inspector_light_type" ) };

        for ( const auto& option: lightTypes ) {
            if ( option.mType == lightType ) {
                preview = MKT_LOC( option.mLabelKey );
                break;
            }
        }

        if ( BeginLightPropertiesTable( "LightTypeTable" ) ) {
            LightPropertyRow( "inspector_light_type" );

            if ( ImGui::BeginCombo( "##LightType", preview.c_str() ) ) {
                for ( const auto& option: lightTypes ) {
                    const bool selected{ option.mType == lightType };

                    if ( ImGui::Selectable( imgui::widget::LocalizedLabel( option.mLabelKey ).c_str(), selected ) ) {
                        lightComponent.SetActiveType( option.mType );
                    }

                    imgui::SetCursorHandOnLastItemHovered();

                    if ( selected ) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            imgui::SetCursorHandOnLastItemHovered();
            ImGui::EndTable();
        }

        switch ( lightComponent.GetActiveType() ) {
            case renderer::LightType::eDirectional:
                DrawDirectionalLightOptions( lightComponent );
                break;

            case renderer::LightType::ePoint:
                DrawPointLightOptions( lightComponent );
                break;

            case renderer::LightType::eSpot:
                DrawSpotLightLightOptions( lightComponent );
                break;

            case renderer::LightType::eAreaLight:
                DrawAreaLightOptions( lightComponent );
                break;
            default:;
        }
    }

    auto InspectorPanel::DrawAreaLightOptions( scene::LightComponent& component ) -> void {
        auto& light{ component.Get<renderer::AreaLight>() };

        if ( BeginLightPropertiesTable( "AreaLightProperties" ) ) {
            LightPropertyRow( "inspector_area_light_shape" );
            int shape{ core::as<int>( light.GetShape() ) };

            if ( ImGui::Combo( "##AreaShape", &shape, MKT_LOC( "inspector_area_light_shapes" ).c_str() ) ) {
                light.SetShape( static_cast<renderer::AreaLightShape>( shape ) );
            }

            LightPropertyRow( "inspector_color" );
            auto color{ light.GetColor() };

            if ( imgui::ColorEdit3( "##AreaColor", color ) ) {
                light.SetColor( color );
            }

            LightPropertyRow( "inspector_intensity" );
            float intensity{ light.GetIntensity() };

            if ( ImGui::DragFloat( "##AreaIntensity", &intensity, 0.1f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp ) ) {
                light.SetIntensity( intensity );
            }

            if ( light.GetShape() == renderer::AreaLightShape::eRectangle ) {
                LightPropertyRow( "inspector_area_light_size" );
                auto size{ light.GetSize() };

                if ( ImGui::DragFloat2( "##AreaSize", &size.x, 0.01f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp ) ) {
                    light.SetSize( size );
                }
            }

            if ( light.GetShape() == renderer::AreaLightShape::eTube ) {
                LightPropertyRow( "inspector_area_light_length" );
                auto size{ light.GetSize() };

                if ( imgui::widget::CircleSlider( "##AreaLength", &size.x, 0.0f, 50.0f, "%.2f m", false ) ) {
                    light.SetSize( size );
                }
            }

            if ( light.GetShape() == renderer::AreaLightShape::eSphere || light.GetShape() == renderer::AreaLightShape::eTube ) {
                LightPropertyRow( "inspector_source_radius" );
                float radius{ light.GetRadius() };

                if ( imgui::widget::CircleSlider( "##AreaRadius", &radius, 0.0f, 10.0f, "%.2f m", false ) ) {
                    light.SetRadius( radius );
                }
            }

            if ( light.GetShape() != renderer::AreaLightShape::eMesh ) {
                LightPropertyRow( "inspector_area_light_range" );
                float range{ light.GetRange() };

                if ( imgui::widget::CircleSlider( "##AreaRange", &range, 0.0f, 100.0f, "%.2f m", false ) ) {
                    light.SetRange( range );
                }
            }

            if ( light.GetShape() == renderer::AreaLightShape::eRectangle || light.GetShape() == renderer::AreaLightShape::eMesh ) {
                LightPropertyRow( "inspector_two_sided" );
                bool twoSided{ light.IsTwoSided() };

                if ( imgui::CheckBox( "##AreaTwoSided", twoSided ) ) {
                    light.SetTwoSided( twoSided );
                }
            }

            ImGui::EndTable();
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "inspector_area_light_path_tracing" ).c_str() );
    }

    auto InspectorPanel::DrawTextComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        auto& text{ entity.GetComponent<scene::TextComponent>() };
        ImGui::PushID( static_cast<int>( entity.Get() ) );

        ImGui::SeparatorText( MKT_LOC( "inspector_font" ).c_str() );
        i32 backend{ as<i32>( text.GetBackend() ) };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_renderer", "Renderer" ).c_str(),
                 &backend, MKT_LOC( "inspector_msdf_slug_curves" ).c_str() ) ) {
            text.SetBackend( as<scene::TextBackend>( backend ) );
        }

        const auto* font{ text.GetFont() };
        ImGui::TextUnformatted( font ? font->GetName().data() : MKT_LOC( "inspector_no_font_selected" ).c_str() );

        if ( font && ImGui::IsItemHovered() ) {
            widget::ItemTooltip( "%s", font->GetPath().GetC_Str() );
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_choose_font", "Choose font..." ).c_str() ) ) {
            std::initializer_list<filesystem::FileDialogPair> filters{ { MKT_LOC( "inspector_font_files" ).c_str(), "ttf,otf" } };
            const filesystem::Path path{ filesystem::OpenFileDialog( filters ) };

            if ( !path.IsEmpty() ) {
                threading::TaskService::Get()->ExecuteWorkerTask(
                [this, pEntity = MKT_ADDRESSOF( entity ), path]() mutable -> void {
                    // We do not know if by the time this worker
                    // runs this entity still has a text component
                    if (!pEntity->HasComponent<TextComponent>()) {
                        return;
                    }

                    auto& textComponent{ pEntity->GetComponent<TextComponent>() };
                    FontHandle font{ asset::AssetRegistryService::Get()->LoadAsset<renderer::Font>( path ) };
                    if (!font.IsEmpty()) {
                        textComponent.SetFont( font );
                    }
                } );
            }
        }

        ImGui::SeparatorText( MKT_LOC( "inspector_text_material" ).c_str() );

        auto assigned{ text.GetMaterial() };
        ImGui::TextUnformatted( assigned ? assigned->GetName().c_str() : MKT_LOC( "inspector_default_component_tint" ).c_str() );
        const auto assignMaterial{ [&]( const filesystem::Path& path ) {
            try {
                auto loaded{ material::LoadMaterial( filesystem::FileHandle::New( path ) ) };

                if ( !dynamic_cast<material::TextMaterial*>( loaded.GetPtr() ) ) {
                    MKT_THROW_RUNTIME_ERROR( MKT_LOC( "inspector_choose_a_text_material_for_a_text_component" ).c_str() );
                }

                text.SetMaterial( loaded.As<material::TextMaterial>() );
            } catch ( const std::exception& error ) {
                MKT_CORE_LOGGER_ERROR( "{}", string::Format( fmt::runtime( MKT_LOC( "inspector_text_material" ).c_str() ), error.what() ) );
            }
        } };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_choose_text_material", "Choose text material..." ).c_str() ) ) {
            std::initializer_list<filesystem::FileDialogPair> filters{ { MKT_LOC( "inspector_mikoto_material" ).c_str(), "mktmtl" } };
            const auto path{ filesystem::OpenFileDialog( filters ) };

            if ( !path.IsEmpty() ) {
                assignMaterial( path );
            }
        }

        if ( ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_MATERIAL" ) } ) {
                if ( payload->DataSize > 0 && static_cast<const char*>( payload->Data )[payload->DataSize - 1] == '\0' ) {
                    assignMaterial( filesystem::Path{ static_cast<const char*>( payload->Data ) } );
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_clear_material", "Clear material" ).c_str() ) ) {
            text.SetMaterial( core::Ref<material::TextMaterial>::CreateEmpty() );
        }

        ImGui::TextDisabled( "%s", MKT_LOC( "inspector_material_color_and_opacity_multiply_the_component_tint" ).c_str() );
        ImGui::SeparatorText( MKT_LOC( "inspector_layout" ).c_str() );

        i32 space{ text.IsWorldText() ? 1 : 0 };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_space", "Space" ).c_str(), &space, MKT_LOC( "inspector_screen_world" ).c_str() ) ) {
            text.SetIsWorldText( space == 1 );
        }

        ImGui::TextWrapped( "%s", text.IsWorldText() ?
            MKT_LOC( "inspector_size_and_spacing_use_world_units_the_origin_is_the" ).c_str() :
            MKT_LOC( "inspector_size_spacing_and_position_use_render_pixels_from_the_top" ).c_str() );

        if ( !text.IsWorldText() ) {
            auto& placement = text.GetScreenPlacement();
            ImGui::DragFloat2( imgui::widget::LocalizedLabel( "inspector_position_px", "Position (px)" ).c_str(), glm::value_ptr( placement.mPosition ), 1.0f );
            float angle = glm::degrees( placement.mRotation );

            if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_rotation", "Rotation" ).c_str(), &angle, 0.5f ) ) {
                core::float2 center{};

                if ( font ) {
                    const auto layout{ text::LayoutText( *font, text.GetUnicodeContents(),
                        { text.GetSize(), text.GetSpacing(), text.GetLineSpacing(),
                            core::as<text::Alignment>( text.GetAlignment() ), true } ) };
                    center = ( layout.mMinimum + layout.mMaximum ) * 0.5f;
                }

                // Keep the visible center fixed while the layout origin follows its alignment.
                // Pivot reference: https://www.sfml-dev.org/tutorials/3.0/graphics/transform/#origin
                const core::float4 pivot{ center, 0.0f, 1.0f };
                const core::float2 before{ text::ScreenTextModel( placement ) * pivot };
                placement.mRotation = glm::radians( angle );
                const core::float2 after{ text::ScreenTextModel( placement ) * pivot };
                placement.mPosition += before - after;
            }

            ImGui::DragFloat2( imgui::widget::LocalizedLabel( "inspector_scale", "Scale" ).c_str(), glm::value_ptr( placement.mScale ), 0.01f, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_reset_screen_placement", "Reset screen placement" ).c_str() ) ) {
                placement = {};
            }
        }

        i32 alignment{ static_cast<int>( text.GetAlignment() ) };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_alignment", "Alignment" ).c_str(), &alignment, MKT_LOC( "inspector_left_center_right" ).c_str() ) ) {
            text.SetAlignment( static_cast<scene::TextAlignment>( alignment ) );
        }

        float size{ text.GetSize() };
        float spacing{ text.GetSpacing() };
        float lineSpacing{ text.GetLineSpacing() };

        if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_font_size", "Font size" ).c_str(), &size, text.IsWorldText() ? 0.01f : 0.25f,
                 scene::TextComponent::GetMinLetterSize(), scene::TextComponent::GetMaxLetterSize(), "%.3f", ImGuiSliderFlags_AlwaysClamp ) ) {
            text.SetSize( size );
        }

        if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_letter_spacing", "Letter spacing" ).c_str(), &spacing, 0.01f, scene::TextComponent::GetMinLetterSpacing(),
                 scene::TextComponent::GetMaxLetterSpacing(), "%.3f", ImGuiSliderFlags_AlwaysClamp ) ) {
            text.SetSpacing( spacing );
        }

        if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_line_spacing", "Line spacing" ).c_str(), &lineSpacing, 0.01f, 0.1f, 10.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp ) ) {
            text.SetLineSpacing( lineSpacing );
        }

        auto color = text.GetColor();

        if ( ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "inspector_color", "Color" ).c_str(), glm::value_ptr( color ), ImGuiColorEditFlags_AlphaBar ) ) {
            text.SetColor( color );
        }

        core::f32 outlineWidth{ text.GetOutlineWidth() };

        if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_outline_width", "Outline width" ).c_str(), &outlineWidth, 0.0f, 0.125f, "%.3f em" ) ) {// units: em's?
            text.SetOutlineWidth( outlineWidth );
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_outside_stroke_proportional_to_font_size_zero_disables_it_colour" ).c_str() );
        auto outlineColor{ text.GetOutlineColor() };

        if ( ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "inspector_outline_colour", "Outline colour" ).c_str(), glm::value_ptr( outlineColor ), ImGuiColorEditFlags_AlphaBar ) ) {
            text.SetOutlineColor( outlineColor );
        }

        ImGui::SeparatorText( MKT_LOC( "inspector_text" ).c_str() );
        eastl::string contents = text.GetContents();
        {
            // The editable sample uses the component's selected face, while
            // surrounding inspector controls keep the normal editor font.
            const auto previewFont{ font ? imgui::ImGuiService::Get()->PushFont( font->GetPath().GetC_Str() )
                                         : ImGuiScopedTextFont{ ImGuiScopedTextFont::Invalid } };

            if ( imgui::TextArea( contents ) ) {
                text.SetContents( contents );
            }
        }

        const auto decoded = text::GetUnicodeFromUtf8( text.GetContents() );
        ImGui::TextDisabled( MKT_LOC( "inspector_code_points_utf_8_bytes" ).c_str(), size_t( decoded.size() ), size_t( text.GetContents().size() ) );

        if ( font ) {
            const auto layout = text::LayoutText( *font, { decoded.data(), decoded.size() },
                { text.GetSize(), text.GetSpacing(), text.GetLineSpacing(), static_cast<text::Alignment>( text.GetAlignment() ), !text.IsWorldText() } );

            if ( layout.mMissingGlyphs ) {
                ImGui::TextWrapped( MKT_LOC( "inspector_characters_have_no_loaded_glyph_new_glyphs_load_when_rendered" ).c_str(), layout.mMissingGlyphs );
            }

            if ( ImGui::TreeNode( imgui::widget::LocalizedLabel( "inspector_atlas_details", "Atlas details" ).c_str() ) ) {
                ImGui::Text( MKT_LOC( "inspector_glyphs_across_pages" ).c_str(), size_t( font->GetGlyphCount() ), size_t( font->GetAtlasPageCount() ) );

                for ( core::usize page = 0; page < font->GetAtlasPageCount(); ++page ) {
                    const auto atlas = font->GetAtlas( page );

                    if ( atlas.IsEmpty() ) {
                        continue;
                    }

                    ImGui::PushID( static_cast<int>( page ) );

                    if ( ImGui::TreeNode( imgui::widget::LocalizedLabel( "inspector_page", "Page" ).c_str(), MKT_LOC( "inspector_page_x" ).c_str(), size_t( page + 1 ), atlas->GetWidth(), atlas->GetHeight() ) ) {
                        const float width = eastl::min( ImGui::GetContentRegionAvail().x, 256.0f );
                        ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( atlas ), { width, width * atlas->GetHeight() / atlas->GetWidth() }, { 0, 1 }, { 1, 0 } );
                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }

                ImGui::TreePop();
            }
        }

        ImGui::PopID();
    }

    auto InspectorPanel::DrawAudioListenerComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        auto& component{ entity.GetComponent<scene::AudioListenerComponent>() };
        bool enabled{ component.IsEnabled() };

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_enabled", "Enabled" ).c_str(), enabled ) ) {
            component.SetEnabled( enabled );
        }

        mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_enabled_listeners_can_hear_3d_sources_sources_use_the_closest" ).c_str() );

        if ( !component.IsActive() ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_no_listener_slot_allocated" ).c_str() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_allocate_listener", "Allocate listener" ).c_str() ) ) {
                const auto& transform{ entity.GetComponent<scene::TransformComponent>() };
                component.SetListener( audio::AudioService::Get()->CreateListener( transform.GetTranslation() ) );
            }

            return;
        }

        const auto& listener{ component.GetListener() };
        const auto& position{ listener.GetPosition() };
        ImGui::Text( MKT_LOC( "inspector_listener" ).c_str(), listener.GetIndex() + 1 );
        ImGui::Text( MKT_LOC( "inspector_position" ).c_str(), position.x, position.y, position.z );
        ImGui::Text( MKT_LOC( "inspector_speed_units_s" ).c_str(), glm::length( listener.GetVelocity() ) );
        ImGui::TextWrapped( "%s", MKT_LOC( "inspector_position_and_orientation_follow_the_world_transform_including_parents_turn" ).c_str() );
    }

    auto InspectorPanel::DrawAudioRoutingField( const char* label, core::u64& output, core::u64 mixer, core::u64 self, bool mixerOnly ) -> bool {
        auto* scene{ mState->mActiveScene };

        const auto accepts = [&]( scene::Entity* candidate ) {
            if ( !candidate ) {
                return false;
            }

            const auto id{ candidate->GetComponent<scene::TagComponent>().GetGuid() };

            if ( id == self ) {
                return false;
            }

            if ( candidate->HasComponent<scene::AudioMixerComponent>() ) {
                return !mixer || id == mixer;
            }

            if ( mixerOnly || !candidate->HasComponent<scene::AudioMixerGroupComponent>() ) {
                return false;
            }

            if ( !self ) {
                return true;
            }

            if ( candidate->GetComponent<scene::AudioMixerGroupComponent>().mMixerID != mixer ) {
                return false;
            }

            ankerl::unordered_dense::set<core::u64> visited{};

            while ( candidate && candidate->HasComponent<scene::AudioMixerGroupComponent>() ) {
                const core::u64 next{ candidate->GetComponent<scene::TagComponent>().GetGuid() };

                if ( next == self || !visited.insert( next ).second ) {
                    return false;
                }

                candidate = scene->FindByID( candidate->GetComponent<scene::AudioMixerGroupComponent>().mOutputID );
            }

            return true;
        };

        bool changed{};
        const auto assign = [&]( scene::Entity* candidate ) {
            if ( accepts( candidate ) ) {
                const core::u64 id{ candidate->GetComponent<scene::TagComponent>().GetGuid() };
                output = id == mixer ? 0 : id;
                changed = true;
            }
        };

        ImGui::PushID( label );
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( label );
        ImGui::SameLine( 90 );

        const auto* selected{ scene->FindByID( output ) };
        const eastl::string preview{ selected ? selected->GetComponent<scene::TagComponent>().GetTag() : MKT_LOC( output ? "audio_missing" : mixer   ? "audio_master"
                                                                                                                                         : mixerOnly ? "audio_none"
                                                                                                                                                     : "audio_device" ) };
        ImGui::SetNextItemWidth( -1 );

        if ( ImGui::BeginCombo( "##Routing", preview.c_str() ) ) {
            if ( ImGui::Selectable( mixerOnly ? MKT_LOC( "audio_none" ).c_str() : mixer ? MKT_LOC( "audio_master" ).c_str()
                                                                                        : MKT_LOC( "audio_device" ).c_str(),
                     !output ) ) {
                output = 0;
                changed = true;
            }

            for ( const auto handle: scene->GetRegistry().view<scene::TagComponent>() ) {
                auto* candidate{ scene->FindByHandle( handle ) };

                if ( !accepts( candidate ) ) {
                    continue;
                }

                ImGui::PushID( static_cast<int>( handle ) );

                if ( ImGui::Selectable( candidate->GetComponent<scene::TagComponent>().GetTag().c_str(), candidate == selected ) ) {
                    assign( candidate );
                }

                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        if ( ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_ENTITY" ) }; payload && payload->DataSize == sizeof( core::u64 ) ) {
                assign( scene->FindByID( *static_cast<const core::u64*>( payload->Data ) ) );
            }

            ImGui::EndDragDropTarget();
        }

        imgui::widget::ItemTooltip( mixerOnly ? MKT_LOC( "audio_drop_mixer" ).c_str() : MKT_LOC( "audio_drop_output" ).c_str() );

        ImGui::PopID();
        return changed;
    }

    auto InspectorPanel::DrawAudioMixerComponentTab( scene::Entity& entity ) -> void {
        auto& component{ entity.GetComponent<scene::AudioMixerComponent>() };

        auto volume{ component.GetVolume() };
        if (ImGui::SliderFloat( MKT_LOC( "audio_volume" ).c_str(), &volume, 0, 20, "%.2fx" )) {
            component.SetVolume( volume );
        }

        auto isMuted{ component.IsMuted() };
        if (imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_mute", "Mute" ).c_str(), isMuted )) {
            component.SetMute( isMuted );
        }

        ImGui::TextDisabled( "%s", MKT_LOC( "audio_device_output" ).c_str() );
        ImGui::SeparatorText( MKT_LOC( "audio_groups" ).c_str() );

        const auto id{ entity.GetComponent<scene::TagComponent>().GetGuid() };

        for ( auto&& entry:
            mState->mActiveScene->GetRegistry().view<scene::AudioMixerGroupComponent, scene::TagComponent>().each() ) {
            const auto handle{ std::get<0>( entry ) };
            auto& group{ std::get<1>( entry ) };
            auto& tag{ std::get<2>( entry ) };

            if ( group.mMixerID != id ) {
                continue;
            }

            ImGui::PushID( static_cast<int>( handle ) );

            if ( ImGui::Selectable( tag.GetTag().c_str() ) ) {
                mState->SelectEntity( mState->mActiveScene->FindByHandle( handle ) );
            }

            ImGui::PopID();
        }

        if ( ImGui::Button( MKT_LOC( "audio_create_group" ).c_str() ) ) {
            auto* group{ mState->mActiveScene->CreateEntity(
                scene::EntityCreateInfo{
                    .mName = MKT_LOC( "audio_group_name" ).c_str(),
                    .mEntityType = scene::EntityType::eSound } ) };

            if ( group ) {
                group->AddComponent<scene::AudioMixerGroupComponent>().mMixerID = id;
            }
        }
    }

    auto InspectorPanel::DrawAudioMixerGroupComponentTab( scene::Entity& entity ) -> void {
        auto& component{ entity.GetComponent<scene::AudioMixerGroupComponent>() };
        const core::u64 id{ entity.GetComponent<scene::TagComponent>().GetGuid() };

        if ( DrawAudioRoutingField( MKT_LOC( "audio_mixer" ).c_str(), component.mMixerID, 0, id, true ) ) {
            component.mOutputID = 0;
        }

        ImGui::BeginDisabled( !component.mMixerID );

        DrawAudioRoutingField( MKT_LOC( "audio_output" ).c_str(), component.mOutputID, component.mMixerID, id );

        ImGui::EndDisabled();

        ImGui::SliderFloat( MKT_LOC( "audio_volume" ).c_str(), &component.mVolume, 0, 20, "%.2fx" );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_mute", "Mute" ).c_str(), component.mMuted );
        ImGui::TextWrapped( "%s", MKT_LOC( "audio_group_help" ).c_str() );
    }

    auto InspectorPanel::DrawVideoComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        auto& component{ entity.GetComponent<scene::VideoComponent>() };
        eastl::string path{ component.GetClip().GetPath().GetC_Str() };

        ImGui::SetNextItemWidth( -eastl::numeric_limits<core::f32>::min() );
        ImGui::InputText( "##VideoClip", path.data(), path.size() + 1, ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll );

        if ( ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_VIDEO" ) } ) {
                component.SetClip( asset::AssetRef<asset::VideoClip>{ filesystem::Path{ static_cast<const char*>( payload->Data ) } } );
            }

            ImGui::EndDragDropTarget();
        }

        if ( ImGui::Button( "Choose video...", ImVec2{ ImGui::GetContentRegionAvail().x, 0 } ) ) {
            const auto selected{ filesystem::OpenFileDialog( { { "MP4 video", "mp4" } } ) };

            if ( !selected.IsEmpty() ) {
                component.SetClip( asset::AssetRef<asset::VideoClip>{ selected } );
            }
        }

        auto settings{ component.GetPlaybackSettings() };
        bool changed{};
        changed |= imgui::CheckBox( "Play on start", settings.mPlayOnStart );
        changed |= imgui::CheckBox( "Loop", settings.mLooping );
        changed |= imgui::CheckBox( "Audio", settings.mAudioEnabled );

        if ( settings.mAudioEnabled ) {
            changed |= imgui::CheckBox( "Spatialized audio", settings.mAudio.mSpatialized );
            changed |= ImGui::SliderFloat( "Volume", &settings.mAudio.mVolume, 0, 4 );
            changed |= ImGui::DragFloat( "Minimum distance", &settings.mAudio.mMinDistance, 0.1f, 0.1f, 100 );
            changed |= ImGui::DragFloat( "Maximum distance", &settings.mAudio.mMaxDistance, 0.1f, settings.mAudio.mMinDistance, 1000 );
        }

        if ( changed ) {
            component.SetPlaybackSettings( settings );
        }

        int map{ static_cast<int>( component.GetOutputMap() ) };

        if ( ImGui::Combo( "Material map", &map, "Base color\0Emissive\0" ) ) {
            component.SetOutputMap( static_cast<renderer::VideoOutputMap>( map ) );
        }

        auto playback{ component.GetPlayback() };

        if ( playback ) {
            if ( playback->GetState() == video::PlaybackState::eFailed ) {
                ImGui::TextWrapped( "%s", playback->GetError().c_str() );
            } else if ( mState->mActiveScene->IsPlaying() ) {
                const bool playing{ playback->GetState() == video::PlaybackState::ePlaying };

                if ( ImGui::Button( playing ? "Pause" : "Play" ) ) {
                    playing ? playback->Pause() : playback->Play();
                }

                ImGui::SameLine();

                if ( ImGui::Button( "Stop" ) ) {
                    playback->Stop();
                }

                if ( const auto clip{ playback->GetClip() } ) {
                    float time{ static_cast<float>( playback->GetTimeSeconds() ) };

                    if ( ImGui::SliderFloat( "Time", &time, 0, static_cast<float>( clip->GetInfo().mDurationSeconds ), "%.2f s" ) ) {
                        playback->Seek( time );
                    }
                }
            }
        }

        ImGui::PushTextWrapPos( 0.0f );
        ImGui::TextDisabled( "MP4 / H.264 hardware decoding. First frame shown while stopped; playback runs in Play." );
        ImGui::PopTextWrapPos();
    }

    auto InspectorPanel::DrawAudioComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        auto& component{ entity.GetComponent<scene::AudioSourceComponent>() };
        ImGui::PushID( static_cast<int>( entity.Get() ) );

        const auto clip{ component.GetClip() };

        eastl::string clipName{ clip ? clip->GetTrackName().data() : MKT_LOC( "inspector_no_audio_clip" ).c_str() };
        ImGui::SetNextItemWidth( -FLT_MIN );
        ImGui::InputText( "##AudioClip", clipName.data(), clipName.size() + 1, ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll );

        if ( clip && ImGui::IsItemHovered() ) {
            mikoto::imgui::widget::ItemTooltip( "%s", clip->GetFile()->GetPath().GetC_Str() );
        }

        ImGui::Spacing();
        const float clipButtonWidth{ clip ? ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x ) * 0.5f : ImGui::GetContentRegionAvail().x };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_choose_clip", "Choose clip..." ).c_str(), ImVec2{ clipButtonWidth, 0 } ) ) {
            std::initializer_list<filesystem::FileDialogPair> filters{ { MKT_LOC( "inspector_audio_files" ).c_str(), "wav,mp3,flac" } };
            const filesystem::Path path{ filesystem::OpenFileDialog( filters ) };

            if ( !path.IsEmpty() ) {
                // Decode away from the UI thread, then validate the destination:
                // the user may have closed this scene while the file was loading.
                threading::TaskService::Get()->ExecuteWorkerTask(
                [pEntity = MKT_ADDRESSOF( entity ), path] {
                    const auto audioHandle{ asset::AssetRegistryService::Get()->LoadAsset<audio::Audio>(
                        asset::AudioLoadDescription {
                            .mFile = filesystem::FileService::Get()->LoadFile( path )
                    } ) };
                    if (audioHandle.IsEmpty() || !pEntity->HasComponent<AudioSourceComponent>()) {
                        return;
                    }

                    auto& audioComponent{ pEntity->GetComponent<AudioSourceComponent>() };
                    audioComponent.SetClip( audioHandle );
                } );
            }
        }

        if ( clip ) {
            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_clear_clip", "Clear clip" ).c_str(), ImVec2{ clipButtonWidth, 0 } ) ) {
                component.SetClip( audio::AudioHandle{} );
            }
        }

        auto source{ component.GetSource() };
        DrawAudioRoutingField( MKT_LOC( "audio_output" ).c_str(), component.mOutputID );

        if ( !source || !source->IsReady() ) {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_choose_a_wav_mp3_or_flac_clip_to_enable_playback" ).c_str() );
        }

        auto settings{ component.GetSettings() };
        bool changed{};
        {
            if ( ImGui::TreeNodeEx( MKT_LOC( "inspector_playback" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                ImGui::Spacing();

                if ( source && source->IsReady() ) {
                    ImGui::BeginDisabled( source->IsPlaying() );

                    if ( ImGui::Button( source->IsPaused() ? MKT_LOC( "inspector_resume" ).c_str() : MKT_LOC( "inspector_play" ).c_str() ) ) {
                        source->Play();
                    }

                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::BeginDisabled( !source->IsPlaying() );

                    if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_pause", "Pause" ).c_str() ) ) {
                        source->Pause();
                    }

                    ImGui::EndDisabled();
                    ImGui::SameLine();

                    if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_stop", "Stop" ).c_str() ) ) {
                        source->Stop();
                    }

                    ImGui::SameLine();
                    ImGui::TextDisabled( "%s", source->IsPlaying() ? MKT_LOC( "inspector_playing" ).c_str() : source->IsPaused() ? MKT_LOC( "inspector_paused" ).c_str()
                                                                                                                                 : MKT_LOC( "inspector_stopped" ).c_str() );

                    const float duration{ source->GetAudioDuration() };
                    float cursor{ source->GetCurrentProgress() };
                    ImGui::BeginDisabled( duration <= 0 );

                    if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_position_s", "Position (s)" ).c_str(), &cursor, 0, glm::max( duration, 0.001f ), "%.2f" ) ) {
                        source->Seek( cursor );
                    }

                    ImGui::EndDisabled();
                    ImGui::TextDisabled( MKT_LOC( "inspector_seconds" ).c_str(), cursor, duration );
                }

                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_loop", "Loop" ).c_str(), settings.mLooping );
                ImGui::SameLine();
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_mute", imgui::widget::LocalizedLabel( "inspector_mute", "Mute" ).c_str() ).c_str(), settings.mMuted );
                changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_gain", "Gain" ).c_str(), &settings.mVolume, 0, audio::AudioSource::GetMaxVolume(), "%.2fx" );
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_1_0_is_the_original_level_high_gains_can_clip" ).c_str() );
                changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_pitch", "Pitch" ).c_str(), &settings.mPitch, 0.01f, 4.0f, "%.2fx" );
                changed |= ImGui::SliderFloat( MKT_LOC( "audio_pan" ).c_str(), &settings.mPan, -1, 1, "%.2f" );
                imgui::widget::ItemTooltip( MKT_LOC( "audio_pan_help" ).c_str() );
                ImGui::TreePop();
            }

            if ( ImGui::CollapsingHeader( MKT_LOC( "audio_settings_3d" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                ImGui::Spacing();
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_3d_headphone_spatialization", "3D headphone spatialization" ).c_str(), settings.mSpatialized );
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_steam_audio_hrtf_places_this_sound_around_the_listener_off" ).c_str() );
                ImGui::BeginDisabled( !settings.mSpatialized );
                int attenuation{ static_cast<int>( settings.mAttenuation ) };

                if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_distance_model", "Distance model" ).c_str(), &attenuation, MKT_LOC( "inspector_none_inverse_linear_exponential" ).c_str() ) ) {
                    settings.mAttenuation = static_cast<audio::Attenuation>( attenuation );
                    changed = true;
                }
                // Distance controls describe attenuation, not a hard audible
                // radius: inverse/exponential models need not go silent at max.
                ImGui::BeginDisabled( settings.mAttenuation == audio::Attenuation::eNone || settings.mCurves[0].mEnabled );
                changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_min_distance", "Min distance" ).c_str(), &settings.mMinDistance, 0.1f, 0.01f, 9999, "%.2f", ImGuiSliderFlags_AlwaysClamp );
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_reference_distance_for_full_volume_playback" ).c_str() );
                changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_rolloff", "Rolloff" ).c_str(), &settings.mRolloff, 0, 10, "%.2f" );
                ImGui::EndDisabled();
                changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_max_distance", "Max distance" ).c_str(), &settings.mMaxDistance, 1, settings.mMinDistance + 0.01f, 10000, "%.2f", ImGuiSliderFlags_AlwaysClamp );
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_upper_distance_used_by_attenuation_this_is_not_a_universal" ).c_str() );
                changed |= ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_doppler", "Doppler" ).c_str(), &settings.mDoppler, 0, 5, "%.2f" );
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "inspector_pitch_shift_from_relative_motion_zero_disables_doppler" ).c_str() );
                ImGui::TextWrapped( "%s", MKT_LOC( "inspector_position_and_velocity_follow_the_entity_s_world_transform_listener" ).c_str() );
                changed |= DrawAudioDistanceCurves( settings );
                ImGui::EndDisabled();
                ImGui::TextDisabled( "%s", MKT_LOC( "inspector_hrtf_steam_audio_best_heard_on_headphones" ).c_str() );
            }
        }

        if ( changed ) {
            component.SetSettings( settings );
        }

        ImGui::PopID();
    }


    // Combo/payload widgets follow Dear ImGui's inspector examples and the camera picker below.
    // https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
    auto InspectorPanel::DrawTextureRenderComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::TextureRenderComponent>() ) {
            return;
        }

        auto& component{ entity.GetComponent<scene::TextureRenderComponent>() };
        auto* scene{ mState->mActiveScene };
        const auto entityID{ entity.GetComponent<scene::TagComponent>().GetGuid() };
        const auto fieldID{ string::Format( "TextureRender_{}", entityID ) };
        ImGui::PushID( fieldID.c_str() );

        eastl::string cameraName{ MKT_LOC( "texture_render_no_camera" ) };
        const scene::Camera* selectedCamera{};

        if ( scene ) {
            for ( auto&& entry: scene->GetRegistry().view<scene::TagComponent, scene::CameraComponent>().each() ) {
                auto& tag{ std::get<1>( entry ) };
                auto& camera{ std::get<2>( entry ) };

                if ( camera.HasCamera() && ( component.GetCameraEntityID() ? tag.GetGuid() == component.GetCameraEntityID() :
                    &camera.GetCamera() == component.GetCamera() ) ) {
                    selectedCamera = &camera.GetCamera();
                    component.SetCamera( selectedCamera, tag.GetGuid() );
                    cameraName = tag.GetTag();
                    break;
                }
            }
        }

        // The exposed reference is a pointer; validate it through live scene cameras.
        if ( component.GetCamera() && !selectedCamera ) {
            component.SetCamera( nullptr );
        }

        if ( ImGui::BeginCombo( MKT_LOC( "texture_render_camera" ).c_str(), cameraName.c_str() ) ) {
            if ( ImGui::Selectable( MKT_LOC( "texture_render_no_camera" ).c_str(), !selectedCamera ) ) {
                component.SetCamera( nullptr );
            }

            if ( scene ) {
                for ( auto&& entry: scene->GetRegistry().view<scene::TagComponent, scene::CameraComponent>().each() ) {
                    const auto handle{ std::get<0>( entry ) };
                    auto& tag{ std::get<1>( entry ) };
                    auto& camera{ std::get<2>( entry ) };

                    if ( !camera.HasCamera() ) {
                        continue;
                    }

                    ImGui::PushID( as<i32>( handle ) );

                    if ( ImGui::Selectable( tag.GetTag().c_str(), tag.GetGuid() == component.GetCameraEntityID() ) ) {
                        component.SetCamera( &camera.GetCamera(), tag.GetGuid() );
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndCombo();
        }

        if ( scene && ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_ENTITY" ) }; payload && payload->DataSize == sizeof( u64 ) ) {
                auto* candidate{ scene->FindByID( *static_cast<const u64*>( payload->Data ) ) };

                if ( candidate && candidate->HasComponent<scene::CameraComponent>() ) {
                    auto& camera{ candidate->GetComponent<scene::CameraComponent>() };

                    if ( camera.HasCamera() ) {
                        component.SetCamera( &camera.GetCamera(), candidate->GetComponent<scene::TagComponent>().GetGuid() );
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        imgui::widget::ItemTooltip( "%s", MKT_LOC( "texture_render_camera_help" ).c_str() );
        ImGui::Spacing();

        i32 resolution{ as<i32>( component.GetResolution() ) };
        constexpr cstr resolutions[]{ "1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160" };

        if ( ImGui::Combo( MKT_LOC( "renderer_resolution" ).c_str(), &resolution, resolutions, as<i32>( eastl::size( resolutions ) ) ) ) {
            component.SetResolution( as<RenderResolution>( resolution ) );
        }

        i32 rate{ as<i32>( component.GetRenderRate() ) };
        constexpr cstr rates[]{ "30 FPS", "60 FPS", "120 FPS" };

        if ( ImGui::Combo( MKT_LOC( "texture_render_rate" ).c_str(), &rate, rates, as<i32>( eastl::size( rates ) ) ) ) {
            component.SetRenderRate( as<TextureRenderRate>( rate ) );
        }

        i32 path{ as<i32>( component.GetRenderPath() ) };
        const cstr paths[]{ MKT_LOC( "texture_render_rasterization" ).c_str(), MKT_LOC( "texture_render_path_tracing" ).c_str() };

        if ( ImGui::Combo( MKT_LOC( "texture_render_path" ).c_str(), &path, paths, as<i32>( eastl::size( paths ) ) ) ) {
            component.SetRenderPath( as<TextureRenderPath>( path ) );
        }

        ImGui::Spacing();

        i32 output{ as<i32>( component.GetRenderOutput() ) };
        const cstr outputs[]{
            MKT_LOC( "texture_render_base_color" ).c_str(),
            MKT_LOC( "texture_render_emissive" ).c_str()
        };

        if ( ImGui::Combo( MKT_LOC( "texture_render_output" ).c_str(), &output, outputs, as<i32>( eastl::size( outputs ) ) ) ) {
            component.SetRenderOutput( as<TextureRenderOutput>( output ) );
        }

        const auto material{ component.GetMaterial() };
        ImGui::Text( "%s: %s", MKT_LOC( "texture_render_material" ).c_str(),
            material ? material->GetName().c_str() : MKT_LOC( "texture_render_no_material" ).c_str() );

        ImGui::Spacing();

        if ( ImGui::CollapsingHeader( MKT_LOC( "texture_render_renderer_configuration" ).c_str() ) ) {
            if ( mTextureRenderScene != scene ) {
                mTextureRenderOptions.clear();
                mTextureRenderScene = scene;
            }

            auto options{ eastl::find_if( mTextureRenderOptions.begin(), mTextureRenderOptions.end(), [entityID]( const TextureRenderOptions& entry ) {
                return entry.mEntity == entityID;
            } ) };

            if ( options == mTextureRenderOptions.end() ) {
                mTextureRenderOptions.push_back( TextureRenderOptions{ .mEntity = entityID } );
                options = mTextureRenderOptions.end() - 1;
            }

            // These remain editor drafts; captures currently use the renderer's defaults.
            auto& settings{ options->mSettings };

            if ( component.GetRenderPath() == TextureRenderPath::eRasterization ) {
                i32 samples{ as<i32>( settings.mMultisampling ) };
                const cstr sampleNames[]{ MKT_LOC( "renderer_off_1x" ).c_str(), "2x", "4x", "8x", "16x" };

                if ( ImGui::Combo( MKT_LOC( "renderer_geometry_msaa" ).c_str(), &samples, sampleNames, as<i32>( eastl::size( sampleNames ) ) ) ) {
                    settings.mMultisampling = as<Multisampling>( samples );
                }

                i32 quality{ as<i32>( settings.mShadowQuality ) };

                if ( ImGui::Combo( MKT_LOC( "renderer_shadow_quality" ).c_str(), &quality, MKT_LOC( "renderer_low_medium_high" ).c_str() ) ) {
                    settings.mShadowQuality = as<ShadowMapQuality>( quality );
                }

                ImGui::DragFloat( MKT_LOC( "texture_render_shadow_distance" ).c_str(), &settings.mShadowDistance,
                    1.0f, 0.1f, 100000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp );
                imgui::CheckBox( MKT_LOC( "renderer_cascaded_shadows" ).c_str(), settings.mCascadedShadowsEnabled );
                imgui::CheckBox( MKT_LOC( "renderer_frustum_culling" ).c_str(), settings.mFrustumCullingEnabled );
            }
            else {
                i32 samples{ as<i32>( settings.mRayTracing.mPathSamplesPerFrame ) };
                i32 bounces{ as<i32>( settings.mRayTracing.mPathMaxBounces ) };

                if ( ImGui::SliderInt( MKT_LOC( "renderer_samples_per_pixel" ).c_str(), &samples,
                         1, as<i32>( RayTracingConfiguration::kMaxPathSamplesPerFrame ) ) ) {
                    settings.mRayTracing.mPathSamplesPerFrame = as<u32>( samples );
                }

                if ( ImGui::SliderInt( MKT_LOC( "renderer_maximum_bounces" ).c_str(), &bounces, 1, 32 ) ) {
                    settings.mRayTracing.mPathMaxBounces = as<u32>( bounces );
                }
            }

            ImGui::Spacing();
            ImGui::TextWrapped( "%s", MKT_LOC( "texture_render_configuration_pending" ).c_str() );
        }

        ImGui::PopID();
    }

    auto InspectorPanel::DrawCameraComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::CameraComponent>() ) {
            return;
        }

        if ( !mState->mEditorRenderer ) {
            return;
        }

        scene::CameraComponent& cameraComponent{ entity.GetComponent<scene::CameraComponent>() };
        const eastl::array<std::string, 2> kCameraProjectionTypeNames{ MKT_LOC( "inspector_orthographic" ).c_str(), MKT_LOC( "inspector_perspective" ).c_str() };

        if ( !cameraComponent.HasCamera() ) {
            if ( !imgui::ButtonTextIcon( string::Concat( ICON_MD_ADD, MKT_LOC( "inspector_add_camera" ).c_str() ).c_str() ) ) {
                return;
            }
        }

        scene::SceneCamera& sceneCamera{ cameraComponent.GetCamera() };
        const auto cameraCurrentProjectionType{ sceneCamera.GetProjectionType() };

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchSame };
        const core::f32 rightInset{ ImGui::GetStyle().ItemSpacing.x };

        if ( ImGui::BeginTable( "CameraComponentEditTable", 2, tableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( MKT_LOC( "inspector_clear_flags" ).c_str() );

            ImGui::TableSetColumnIndex( 1 );
            scene::CameraClearFlags currentClearFlags{ cameraComponent.GetClearFlags() };
            eastl::array<std::string, core::as<core::usize>( scene::CameraClearFlags::eCount )> choicesAlpha{
                MKT_LOC( "inspector_clear_color" ).c_str(), MKT_LOC( "inspector_blurred_skybox" ).c_str(), MKT_LOC( "inspector_skybox_option" ).c_str()
            };

            const core::f32 swatchSize{ ImGui::GetFrameHeight() };
            const core::f32 swatchSpace{ swatchSize + ImGui::GetStyle().ItemSpacing.x };
            const bool inlineSwatch{ currentClearFlags == scene::CameraClearFlags::eClearColor &&
                ImGui::GetContentRegionAvail().x >= rightInset + swatchSpace + swatchSize };
            ImGui::SetNextItemWidth( -( rightInset + ( inlineSwatch ? swatchSpace : 0.0f ) ) );
            scene::CameraClearFlags newClearFlags{ imgui::Combo( choicesAlpha, currentClearFlags ) };

            if ( newClearFlags != currentClearFlags ) {
                auto GetRenderBackground{
                    []( scene::CameraClearFlags rb ) {
                    switch ( rb ) {
                        case scene::CameraClearFlags::eSkybox:
                            return renderer::SceneBackgroundType::eSkybox;
                        case scene::CameraClearFlags::eBlurredSkybox:
                            return renderer::SceneBackgroundType::ePrefilterMap;
                        case scene::CameraClearFlags::eClearColor:
                            return renderer::SceneBackgroundType::eClearColor;
                        default:;
                    }

                    return renderer::SceneBackgroundType::eClearColor;
                }
                };

                cameraComponent.SetClearFlags( newClearFlags );

                mState->mEditorRenderer->SetGamma( cameraComponent.GetGamma() );
                mState->mEditorRenderer->SetExposure( cameraComponent.GetExposure() );
                mState->mEditorRenderer->SetRenderBackground( GetRenderBackground( newClearFlags ) );

                if ( newClearFlags == scene::CameraClearFlags::eClearColor ) {
                    mState->mEditorRenderer->SetClearColor( cameraComponent.GetClearColor() );
                }
            }

            if ( newClearFlags == scene::CameraClearFlags::eClearColor ) {
                if ( inlineSwatch ) {
                    ImGui::SameLine();
                }

                core::float4 colorFloat4{ cameraComponent.GetClearColor() };
                constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs };

                if ( ImGui::ColorEdit4( "##CameraClearColor", glm::value_ptr( colorFloat4 ), colorEditFlags ) ) {
                    cameraComponent.SetClearColor( colorFloat4 );
                    mState->mEditorRenderer->SetClearColor( colorFloat4 );
                }
            }

            if ( newClearFlags == scene::CameraClearFlags::eBlurredSkybox ) {
                ImGui::Spacing();
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( MKT_LOC( "inspector_blur" ).c_str() );

                ImGui::TableSetColumnIndex( 1 );
                core::f32 blur{ cameraComponent.GetSkyboxBlur() };

                ImGui::SetNextItemWidth( -rightInset );
                if ( ImGui::SliderFloat( "##CameraSkyboxBlurrSlider", MKT_ADDRESSOF( blur ), 0.0, 1.0f ) ) {
                    cameraComponent.SetSkyboxBlur( blur );
                    mState->mEditorRenderer->SetSkyboxBlur( blur );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( MKT_LOC( "inspector_exposure" ).c_str() );

            ImGui::TableSetColumnIndex( 1 );
            core::f32 exposure{ cameraComponent.GetExposure() };

            ImGui::SetNextItemWidth( -rightInset );
            if ( ImGui::SliderFloat( "##CameraExposureSlider", MKT_ADDRESSOF( exposure ), 0.1, 10.0f ) ) {
                cameraComponent.SetExposure( exposure );
                mState->mEditorRenderer->SetExposure( cameraComponent.GetExposure() );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( MKT_LOC( "inspector_gamma" ).c_str() );

            ImGui::TableSetColumnIndex( 1 );
            core::f32 gamma{ cameraComponent.GetGamma() };

            ImGui::SetNextItemWidth( -rightInset );
            if ( ImGui::SliderFloat( "##CameraGammaSlider", MKT_ADDRESSOF( gamma ), 0.1, 10.0f ) ) {
                cameraComponent.SetGamma( gamma );
                mState->mEditorRenderer->SetGamma( cameraComponent.GetGamma() );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::Spacing();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( MKT_LOC( "inspector_projection_type" ).c_str() );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );
            const auto& currentProjectionTypeStr{ kCameraProjectionTypeNames[core::as<core::u32>( cameraCurrentProjectionType )] };

            ImGui::SetNextItemWidth( -rightInset );
            if ( ImGui::BeginCombo( "##Projection", currentProjectionTypeStr.c_str() ) ) {
                core::u32 projectionIndex{};

                for ( const auto& projectionType: kCameraProjectionTypeNames ) {
                    // Tells whether we want to highlight this projection in the ImGui combo.
                    // This will be the case if this projection type is the current one for this camera.
                    bool isSelected{ projectionType == kCameraProjectionTypeNames[core::as<core::u32>( cameraCurrentProjectionType )] };

                    // Create a selectable combo item for each perspective
                    if ( ImGui::Selectable( projectionType.c_str(), isSelected ) ) {
                        sceneCamera.SetProjectionType( core::as<scene::ProjectionType>( projectionIndex ) );
                    }

                    if ( ImGui::IsItemHovered() ) {
                        ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                    }

                    if ( isSelected ) {
                        ImGui::SetItemDefaultFocus();
                    }

                    ++projectionIndex;
                }

                ImGui::EndCombo();
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }

        bool mainCamera{ cameraComponent.IsMainCamera() };

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_main_camera", "Main camera" ).c_str(), mainCamera ) ) {
            cameraComponent.SetMainCamera( mainCamera );
        }

        if ( ImGui::BeginTable( "##CameraProjectionControls", 2, tableFlags ) ) {
            if ( sceneCamera.IsOrthographic() ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( MKT_LOC( "inspector_view_height" ).c_str() );

                ImGui::TableSetColumnIndex( 1 );
                core::f32 height{ sceneCamera.GetOrthographicHeight() };

                ImGui::SetNextItemWidth( -rightInset );
                if ( ImGui::DragFloat( "##OrthographicHeight", &height, 0.1f, 0.01f, 100000.0f ) ) {
                    sceneCamera.SetOrthographicHeight( height );
                }
            }
            else if ( sceneCamera.GetProjectionType() == scene::ProjectionType::ePerspective ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( MKT_LOC( "inspector_field_of_view" ).c_str() );

                ImGui::TableSetColumnIndex( 1 );
                float fov{ sceneCamera.GetFOV() };

                ImGui::SetNextItemWidth( -rightInset );
                if ( ImGui::SliderFloat( "##Perspective FOV", std::addressof( fov ), 45.0f, 90.0f ) ) {
                    sceneCamera.SetFieldOfView( fov );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( MKT_LOC( "inspector_near_plane" ).c_str() );

                ImGui::TableSetColumnIndex( 1 );
                float nearPlane{ sceneCamera.GetNearPlane() };

                ImGui::SetNextItemWidth( -rightInset );
                if ( ImGui::SliderFloat( "##Perspective Near", std::addressof( nearPlane ), 0.001f, 1.0 ) ) {
                    sceneCamera.SetNearPlane( nearPlane );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( MKT_LOC( "inspector_far_plane" ).c_str() );

                ImGui::TableSetColumnIndex( 1 );
                float farPlane{ ( sceneCamera.GetFarPlane() ) };

                ImGui::SetNextItemWidth( -rightInset );
                if ( ImGui::SliderFloat( "##Perspective Far", &farPlane, 100.0f, 10000.0f ) ) {
                    sceneCamera.SetFarPlane( farPlane );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }
            }

            ImGui::EndTable();
        }

        DrawCameraDepthOfFieldOptions( sceneCamera );
        DrawCameraVignetteOptions( sceneCamera );
    }

    auto InspectorPanel::DrawSkyboxComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::SkyboxMaterialComponent>() ) {
            return;
        }

        scene::SkyboxMaterialComponent& sbComponent{ entity.GetComponent<scene::SkyboxMaterialComponent>() };
        material::SkyboxMaterial* material{ core::checked_cast<material::SkyboxMaterial*>( sbComponent.GetMaterial().GetPtr() ) };

        if ( !material ) {
            return;
        }

        ImGui::TextUnformatted( MKT_LOC( "inspector_material_name" ).c_str() );
        ImGui::SameLine();

        if ( imgui::InputText( material->GetName(), true ) ) {
            // Nothing
        }

        // Select type of skybox texture (equirectangular or cube faces)
        ImGui::Spacing();
        ImGui::TextUnformatted( MKT_LOC( "inspector_skybox_type" ).c_str() );
        material::SkyboxType currentSkyboxType{ material->GetType() };
        eastl::array<std::string, core::as<core::usize>( material::SkyboxType::eCount )> choicesAlpha{
            MKT_LOC( "inspector_cube_faces" ).c_str(), MKT_LOC( "inspector_hdr_texture" ).c_str()
        };

        ImGui::SameLine();
        material::SkyboxType newSkyboxType{ imgui::Combo( choicesAlpha, currentSkyboxType ) };

        if ( newSkyboxType != currentSkyboxType ) {
            material->SetType( newSkyboxType );
            mState->mEditorRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );
        }

        ImGui::Spacing();
        ImGui::SeparatorText( MKT_LOC( "inspector_texture_s" ).c_str() );

        constexpr auto columnCount{ 2 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None | ImGuiTableFlags_BordersInner };

        ImGui::Spacing();

        const auto drawTextureButton{ []( eastl::string_view id, ImTextureID texture ) -> bool {
            imgui::ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.0f };
            imgui::ImGuiScopedStyleVar border{ ImGuiStyleVar_FrameBorderSize, 1.0f };
            imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_FramePadding, ImVec2{ 2.0f, 2.0f } };
            ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f } );
            const bool clicked{ imgui::PushImageButton( id, texture, ImVec2{ 64.0f, 64.0f } ) };
            ImGui::PopStyleColor();
            return clicked;
        } };

        if ( material->IsType( material::SkyboxType::eCubeFaces ) ) {
            if ( ImGui::BeginTable( "##SetupSkyboxComponentTable_CubeFaces", columnCount, specularTableFlags ) ) {
                const eastl::array<eastl::pair<material::SkyboxFace, eastl::string_view>, 6> kCubeFaces{ { { material::SkyboxFace::eTop, MKT_LOC( "inspector_top_y" ).c_str() },
                    { material::SkyboxFace::eBottom, MKT_LOC( "inspector_bottom_y" ).c_str() },
                    { material::SkyboxFace::eBack, MKT_LOC( "inspector_back_z" ).c_str() },
                    { material::SkyboxFace::eFront, MKT_LOC( "inspector_front_z" ).c_str() },
                    { material::SkyboxFace::eLeft, MKT_LOC( "inspector_left_x" ).c_str() },
                    { material::SkyboxFace::eRight, MKT_LOC( "inspector_right_x" ).c_str() } } };

                for ( core::usize i{}; i < kCubeFaces.size(); ++i ) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex( 0 );

                    renderer::rhi::TextureHandle face{ material ? material->GetFace( kCubeFaces[i].first ) : renderer::rhi::TextureHandle::CreateEmpty() };

                    ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
                    ImGui::SameLine();
                    ImGui::TextUnformatted( string::Format( fmt::runtime( MKT_LOC( "inspector_face" ).c_str() ), kCubeFaces[i].second ).c_str() );

                    if ( face.IsEmpty() ) {
                        face = asset::AssetRegistryService::Get()->GetDummyTexture();
                    }

                    if ( drawTextureButton( string::Format( "##SetupSkyboxComponentTab:{}", kCubeFaces[i].second ), imgui::ImGuiService::Get()->GetTextureID( face.GetPtr() ) ) ) {
                        if ( material ) {
                            LoadMaterialTexture( *material, kCubeFaces[i].first );
                        }
                    }

                    if ( ImGui::BeginDragDropTarget() ) {
                        if ( const ImGuiPayload * payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_IMAGE" ) } ) {
                            auto cubeMap{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( filesystem::Path{ static_cast<cstr>( payload->Data ) }, renderer::rhi::TextureDimension::eTexture2D ) };
                            material->SetFace( kCubeFaces[i].first, cubeMap );

                            core::RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
                        }

                        ImGui::EndDragDropTarget();
                    }

                    if ( ImGui::IsItemHovered() ) {
                        ShowTextureHoverTooltip( material->GetFace( kCubeFaces[i].first ).GetPtr() );
                        ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                    }

                    ImGui::TableSetColumnIndex( 1 );

                    imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };

                    if ( ImGui::Button( string::Format( fmt::runtime( MKT_LOC( "inspector_remove" ).c_str() ), kCubeFaces[i].second ).c_str() ) ) {
                    }

                    if ( ImGui::IsItemHovered() ) {
                        ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                    }
                }

                ImGui::EndTable();
            }

            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };

                if ( ImGui::Button( string::Format( fmt::runtime( MKT_LOC( "inspector_apply" ).c_str() ), ICON_MD_CLOUD_DOWNLOAD ).c_str() ) ) {
                    mState->mEditorRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }
            }
        }
        else if ( material->IsType( material::SkyboxType::eEquirectangular ) ) {
            if ( ImGui::BeginTable( "##SetupSkyboxComponentTable_FlatImage", columnCount, specularTableFlags ) ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );

                renderer::rhi::TextureHandle face{ material ? material->GetEquirectangular() : renderer::rhi::TextureHandle::CreateEmpty() };

                ImGui::TextUnformatted( string::Format( "{}", ICON_MD_TEXTURE ).c_str() );
                ImGui::SameLine();
                ImGui::TextUnformatted( MKT_LOC( "inspector_equirectangular" ).c_str() );

                if ( face.IsEmpty() ) {
                    face = asset::AssetRegistryService::Get()->GetDummyTexture();
                }

                if ( drawTextureButton( "##SetupSkyboxComponentTable_FlatImage01", imgui::ImGuiService::Get()->GetTextureID( face.GetPtr() ) ) ) {
                    if ( material ) {
                        LoadMaterialTexture( *material );
                        mState->mEditorRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );
                    }
                }

                if ( ImGui::BeginDragDropTarget() ) {
                    if ( const ImGuiPayload * payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_IMAGE" ) } ) {
                        auto cubeMap{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( filesystem::Path{ static_cast<cstr>( payload->Data ) }, renderer::rhi::TextureDimension::eTexture2D ) };
                        material->SetEquirectangular( cubeMap );
                        mState->mEditorRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );

                        core::RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
                    }

                    ImGui::EndDragDropTarget();
                }

                if ( ImGui::IsItemHovered() ) {
                    ShowTextureHoverTooltip( material->GetEquirectangular().GetPtr() );
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableSetColumnIndex( 1 );

                imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };

                if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_remove_image", "Remove Image" ).c_str() ) ) {
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::EndTable();
            }
        }

        ImGui::SeparatorText( MKT_LOC( "inspector_settings" ).c_str() );
        ImGui::Spacing();

        core::f32 ambientScale{ material->GetAmbientScale() };

        const core::f32 contentWidth{ ImGui::GetContentRegionAvail().x };
        const core::f32 labelWidth{ contentWidth * 0.30f };
        const core::f32 sliderWidth{ contentWidth - labelWidth };

        auto DrawProperty = []( const char* label, const char* id, core::f32& value, core::f32 min, core::f32 max, core::f32 labelWidth, core::f32 sliderWidth ) {
            ImGui::PushID( id );

            ImGui::TextUnformatted( label );
            ImGui::SameLine( labelWidth );

            ImGui::SetNextItemWidth( sliderWidth );
            ImGui::SliderFloat( "##Slider", &value, min, max );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::PopID();
        };

        // Move exposure and gamma to environment tab
        DrawProperty( MKT_LOC( "inspector_ambient_scale" ).c_str(), MKT_LOC( "inspector_ambientscale" ).c_str(), ambientScale, 0.0f, 10.0f, labelWidth, sliderWidth );

        material->SetAmbientScale( ambientScale );
        mState->mEditorRenderer->SetAmbientScale( material->GetAmbientScale() );
    }

    template<typename Component>
    auto InspectorPanel::DrawVolumeComponentTab( scene::Entity& entity ) -> void {
        auto& volume{ entity.GetComponent<Component>() };
        constexpr bool cloud{ std::is_same_v<Component, scene::CloudComponent> };
        i32 shape{ core::as<i32>( volume.GetShape() ) };

        if ( ImGui::Combo( MKT_LOC( "volume_shape" ).c_str(), &shape,
                 MKT_LOC( cloud ? "volume_cloud_shapes" : "volume_fog_shapes" ).c_str() ) ) {
            volume.SetShape( core::as<decltype( volume.GetShape() )>( shape ) );
        }

        if constexpr ( cloud ) {
            if ( volume.GetShape() == scene::CloudShape::eSkyLayer ) {
                f32 bottomAltitude{ volume.GetBottomAltitude() };
                f32 layerThickness{ volume.GetLayerThickness() };

                if ( ImGui::DragFloat( MKT_LOC( "volume_bottom_altitude" ).c_str(), &bottomAltitude, 1 ) ) {
                    volume.SetBottomAltitude( bottomAltitude );
                }

                if ( ImGui::DragFloat( MKT_LOC( "volume_layer_thickness" ).c_str(), &layerThickness, 1, .001f, 10000000, "%.1f", ImGuiSliderFlags_AlwaysClamp ) ) {
                    volume.SetLayerThickness( layerThickness );
                }
            }

            float2 windDirection{ volume.GetWindDirection() };
            f32 windSpeed{ volume.GetWindSpeed() };

            if ( ImGui::DragFloat2( MKT_LOC( "volume_wind_direction" ).c_str(), &windDirection.x, .01f ) ) {
                volume.SetWindDirection( windDirection );
            }

            if ( ImGui::DragFloat( MKT_LOC( "volume_wind_speed" ).c_str(), &windSpeed, .1f ) ) {
                volume.SetWindSpeed( windSpeed );
            }
            ImGui::TextDisabled( "%s", MKT_LOC( "volume_wind_help" ).c_str() );
        }

        if ( shape == 0 ) {
            f32 horizonRadius{ volume.GetHorizonRadius() };

            if ( ImGui::DragFloat( MKT_LOC( "volume_horizon_radius" ).c_str(), &horizonRadius, 10, .001f, 10000000, "%.1f", ImGuiSliderFlags_AlwaysClamp ) ) {
                volume.SetHorizonRadius( horizonRadius );
            }
        }
        else {
            float3 dimensions{ volume.GetDimensions() };

            if ( ImGui::DragFloat3( MKT_LOC( "volume_dimensions" ).c_str(), &dimensions.x, 1, .001f, 10000000, "%.2f", ImGuiSliderFlags_AlwaysClamp ) ) {
                volume.SetDimensions( dimensions );
            }
        }

        f32 densityMultiplier{ volume.GetDensityMultiplier() };

        if ( ImGui::DragFloat( MKT_LOC( "volume_density_multiplier" ).c_str(), &densityMultiplier, .01f, 0, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp ) ) {
            volume.SetDensityMultiplier( densityMultiplier );
        }

        bool castShadows{ volume.IsShadowCaster() };
        bool receiveShadows{ volume.IsShadowReceiver() };

        if ( imgui::CheckBox( MKT_LOC( "volume_cast_shadows" ).c_str(), castShadows ) ) {
            volume.SetCastShadows( castShadows );
        }

        if ( imgui::CheckBox( MKT_LOC( "volume_receive_shadows" ).c_str(), receiveShadows ) ) {
            volume.SetReceiveShadows( receiveShadows );
        }

        ImGui::SeparatorText( MKT_LOC( "volume_material" ).c_str() );
        auto volumeMaterial{ volume.GetMaterial() };

        const auto assign{ [&]( const filesystem::Path& path ) {
            try {
                auto loaded{ material::LoadMaterial( filesystem::FileHandle::New( path ) ) };
                if ( !dynamic_cast<material::VolumeMaterial*>( loaded.GetPtr() ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Cloud and fog components require a volume material." );
                }
                volumeMaterial = loaded.As<material::VolumeMaterial>();
                volume.SetMaterial( volumeMaterial );
            } catch ( const std::exception& error ) {
                MKT_CORE_LOGGER_ERROR( "{}", error.what() );
            }
        } };
        if ( ImGui::Button( volumeMaterial ? volumeMaterial->GetName().c_str() : MKT_LOC( "volume_none" ).c_str(), ImVec2{ -1, 0 } ) && volumeMaterial ) {
            mState->mEditMaterial = core::Ref<asset::Material>::New( volumeMaterial );
            mState->mRequestMaterialEditor = true;
        }
        imgui::widget::ItemTooltip( "%s", MKT_LOC( "volume_material_help" ).c_str() );
        if ( ImGui::BeginDragDropTarget() ) {
            if ( const auto* payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_MATERIAL" ) } ) {
                if ( payload->DataSize > 0 && static_cast<const char*>( payload->Data )[payload->DataSize - 1] == '\0' ) {
                    assign( filesystem::Path{ static_cast<const char*>( payload->Data ) } );
                }
            }
            ImGui::EndDragDropTarget();
        }
        if ( ImGui::Button( MKT_LOC( "volume_choose_material" ).c_str() ) ) {
            const auto path{ filesystem::OpenFileDialog( { { "Mikoto Material", "mktmtl" } } ) };
            if ( !path.IsEmpty() ) {
                assign( path );
            }
        }
        ImGui::SameLine();
        if ( ImGui::Button( MKT_LOC( "volume_make_unique" ).c_str() ) && volumeMaterial ) {
            volumeMaterial = material::VolumeMaterialHandle::New( *volumeMaterial );
            volume.SetMaterial( volumeMaterial );
        }
        imgui::widget::ItemTooltip( "%s", MKT_LOC( "volume_unique_help" ).c_str() );
        if ( volumeMaterial ) {
            DrawVolumeMaterial( *volumeMaterial );
        }
    }

    auto InspectorPanel::DrawPostProcessComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        scene::PostProcessMaterialComponent& postProcessComponent{ entity.GetComponent<scene::PostProcessMaterialComponent>() };
        material::PostProcessMaterial* material{ core::checked_cast<material::PostProcessMaterial*>( postProcessComponent.GetMaterial().GetPtr() ) };

        if ( !material ) {
            return;
        }

        core::i32 category{ core::as<core::i32>( material->GetCategory() ) };
        const core::cstr categories[]{ MKT_LOC( "post_process_category_built_in" ).c_str(), MKT_LOC( "post_process_category_custom" ).c_str() };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "post_process_category", "Category" ).c_str(), &category, categories, 2 ) ) {
            material->SetCategory( core::as<material::PostProcessMaterialCategory>( category ) );
        }

        core::i32 stage{ core::as<core::i32>( material->GetStage() ) };
        const core::cstr stages[]{ MKT_LOC( "post_process_before_tone_mapping" ).c_str(), MKT_LOC( "post_process_after_tone_mapping" ).c_str() };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "post_process_stage", "Stage" ).c_str(), &stage, stages, 2 ) ) {
            material->SetStage( core::as<material::PostProcessStage>( stage ) );
        }

        DrawPostProcessBloom( *material );
        DrawPostProcessChromaticAberration( *material );

        ImGui::SeparatorText( MKT_LOC( "post_process_film_grain" ).c_str() );
        bool grainEnabled{ material->IsFilmGrainEnabled() };

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "post_process_film_grain_enabled", "Enabled" ).c_str(), grainEnabled ) ) {
            material->SetFilmGrainEnabled( grainEnabled );
        }

        if ( auto* scene{ mState->mActiveScene } ) {
            const auto accepts{ []( const scene::Entity* candidate ) {
                return candidate && candidate->HasComponent<scene::CameraComponent>() && candidate->GetComponent<scene::CameraComponent>().HasCamera();
            } };
            auto* cameraEntity{ scene->FindByID( postProcessComponent.GetCameraEntityID() ) };
            const eastl::string preview{ accepts( cameraEntity ) ? cameraEntity->GetComponent<scene::TagComponent>().GetTag()
                                                                 : MKT_LOC( "post_process_film_grain_no_camera" ) };

            // Keep entity references on the component, so reusable material assets remain scene-independent.
            if ( ImGui::BeginCombo( imgui::widget::LocalizedLabel( "post_process_film_grain_camera", "Camera" ).c_str(), preview.c_str() ) ) {
                if ( ImGui::Selectable( MKT_LOC( "post_process_film_grain_no_camera" ).c_str(), !accepts( cameraEntity ) ) ) {
                    postProcessComponent.SetCameraEntityID( 0 );
                }

                for ( auto&& entry: scene->GetRegistry().view<scene::TagComponent, scene::CameraComponent>().each() ) {
                    const auto handle{ std::get<0>( entry ) };
                    auto& tag{ std::get<1>( entry ) };
                    auto& camera{ std::get<2>( entry ) };

                    if ( !camera.HasCamera() ) {
                        continue;
                    }

                    ImGui::PushID( core::as<int>( handle ) );

                    if ( ImGui::Selectable( tag.GetTag().c_str(), tag.GetGuid() == postProcessComponent.GetCameraEntityID() ) ) {
                        postProcessComponent.SetCameraEntityID( tag.GetGuid() );
                    }

                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }

            if ( ImGui::BeginDragDropTarget() ) {
                if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_ENTITY" ) }; payload && payload->DataSize == sizeof( core::u64 ) ) {
                    auto* candidate{ scene->FindByID( *static_cast<const core::u64*>( payload->Data ) ) };

                    if ( accepts( candidate ) ) {
                        postProcessComponent.SetCameraEntityID( candidate->GetComponent<scene::TagComponent>().GetGuid() );
                    }
                }

                ImGui::EndDragDropTarget();
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "post_process_film_grain_camera_help" ).c_str() );
            cameraEntity = scene->FindByID( postProcessComponent.GetCameraEntityID() );

            if ( accepts( cameraEntity ) ) {
                auto& camera{ cameraEntity->GetComponent<scene::CameraComponent>().GetCamera() };
                auto grain{ camera.GetFilmGrainSettings() };

                // Edit the camera's parameters here; the material owns only the enable switch.
                if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "post_process_film_grain_amount", "Amount" ).c_str(),
                         &grain.mAmount, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp ) ) {
                    camera.SetFilmGrainSettings( grain );
                }
            }
        }

        ImGui::Spacing();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_open_in_graph_editor", "Open in graph editor" ).c_str(), ImVec2{ -1.0f, 0.0f } ) ) {
            mState->mEditMaterial = core::Ref<asset::Material>::New( postProcessComponent.GetMaterial() );
            mState->mRequestMaterialEditor = true;
        }
    }

    auto InspectorPanel::DrawMeshColliderComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::MeshColliderComponent>() ) {
            return;
        }

        auto& collider{ entity.GetComponent<scene::MeshColliderComponent>() };
        core::float3 offset{ collider.GetOffset() };
        bool isTrigger{ collider.IsTrigger() };
        bool changed{};

        if ( !entity.HasComponent<scene::RigidBodyComponent>() ) {
            ImGui::TextDisabled( MKT_LOC( "inspector_static_collider_no_rigid_body" ).c_str() );
        }

        if ( mState->mActiveScene ) {
            const char* error = mState->mActiveScene->GetPhysicsWorld()->GetColliderError( entity.Get() );

            if ( *error ) {
                ImGui::TextWrapped( MKT_LOC( "inspector_collider_disabled" ).c_str(), error );
            }
        }

        bool convex = collider.IsConvex();
        changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_convex", "Convex" ).c_str(), convex );
        collider.SetConvex( convex );
        ImGui::TextWrapped( MKT_LOC( "inspector_convex_uses_the_mesh_hull_triangle_meshes_require_a_static" ).c_str() );
        changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "inspector_offset", "Offset" ).c_str(), &offset.x, 0.01f );
        changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_trigger", "Trigger" ).c_str(), isTrigger );

        if ( changed ) {
            collider.SetOffset( offset );
            collider.SetTrigger( isTrigger );

            if ( mState->mActiveScene ) {
                mState->mActiveScene->GetPhysicsWorld()->AddCollider( &entity );
            }
        }
    }

    auto InspectorPanel::DrawBoxColliderComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::BoxColliderComponent>() ) {
            return;
        }

        auto& collider{ entity.GetComponent<scene::BoxColliderComponent>() };
        core::float3 halfExtents{ collider.GetHalfExtents() };
        core::float3 offset{ collider.GetOffset() };
        bool isTrigger{ collider.IsTrigger() };
        bool changed{};

        if ( !entity.HasComponent<scene::RigidBodyComponent>() ) {
            ImGui::TextDisabled( MKT_LOC( "inspector_static_collider_no_rigid_body" ).c_str() );
        }

        if ( mState->mActiveScene ) {
            const char* error = mState->mActiveScene->GetPhysicsWorld()->GetColliderError( entity.Get() );

            if ( *error ) {
                ImGui::TextWrapped( MKT_LOC( "inspector_collider_disabled" ).c_str(), error );
            }
        }

        changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "inspector_half_extents", "Half Extents" ).c_str(), &halfExtents.x, 0.01f, 0.001f, FLT_MAX );
        changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "inspector_offset", "Offset" ).c_str(), &offset.x, 0.01f );
        changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_trigger", "Trigger" ).c_str(), isTrigger );

        if ( changed ) {
            collider.SetHalfExtents( halfExtents );
            collider.SetOffset( offset );
            collider.SetTrigger( isTrigger );

            if ( mState->mActiveScene ) {
                mState->mActiveScene->GetPhysicsWorld()->AddCollider( &entity );
            }
        }
    }

    auto InspectorPanel::DrawSphereColliderComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::SphereColliderComponent>() ) {
            return;
        }

        auto& collider{ entity.GetComponent<scene::SphereColliderComponent>() };
        float radius{ collider.GetRadius() };
        core::float3 offset{ collider.GetOffset() };
        bool isTrigger{ collider.IsTrigger() };
        bool changed{};

        if ( !entity.HasComponent<scene::RigidBodyComponent>() ) {
            ImGui::TextDisabled( MKT_LOC( "inspector_static_collider_no_rigid_body" ).c_str() );
        }

        if ( mState->mActiveScene ) {
            const char* error = mState->mActiveScene->GetPhysicsWorld()->GetColliderError( entity.Get() );

            if ( *error ) {
                ImGui::TextWrapped( MKT_LOC( "inspector_collider_disabled" ).c_str(), error );
            }
        }

        changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_radius", "Radius" ).c_str(), &radius, 0.01f, 0.001f, FLT_MAX );
        changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "inspector_offset", "Offset" ).c_str(), &offset.x, 0.01f );
        changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_trigger", "Trigger" ).c_str(), isTrigger );

        if ( changed ) {
            collider.SetRadius( radius );
            collider.SetOffset( offset );
            collider.SetTrigger( isTrigger );

            if ( mState->mActiveScene ) {
                mState->mActiveScene->GetPhysicsWorld()->AddCollider( &entity );
            }
        }
    }

    auto InspectorPanel::DrawCapsuleColliderComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity.HasComponent<scene::CapsuleColliderComponent>() ) {
            return;
        }

        auto& collider{ entity.GetComponent<scene::CapsuleColliderComponent>() };
        float radius{ collider.GetRadius() };
        float halfHeight{ collider.GetHalfHeight() };
        core::float3 offset{ collider.GetOffset() };
        bool isTrigger{ collider.IsTrigger() };
        bool changed{};

        if ( !entity.HasComponent<scene::RigidBodyComponent>() ) {
            ImGui::TextDisabled( MKT_LOC( "inspector_static_collider_no_rigid_body" ).c_str() );
        }

        if ( mState->mActiveScene ) {
            const char* error = mState->mActiveScene->GetPhysicsWorld()->GetColliderError( entity.Get() );

            if ( *error ) {
                ImGui::TextWrapped( MKT_LOC( "inspector_collider_disabled" ).c_str(), error );
            }
        }

        changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_radius", "Radius" ).c_str(), &radius, 0.01f, 0.001f, FLT_MAX );
        changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_half_height", "Half Height" ).c_str(), &halfHeight, 0.01f, 0.0f, FLT_MAX );
        changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "inspector_offset", "Offset" ).c_str(), &offset.x, 0.01f );
        changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_trigger", "Trigger" ).c_str(), isTrigger );

        if ( changed ) {
            collider.SetRadius( radius );
            collider.SetHalfHeight( halfHeight );
            collider.SetOffset( offset );
            collider.SetTrigger( isTrigger );

            if ( mState->mActiveScene ) {
                mState->mActiveScene->GetPhysicsWorld()->AddCollider( &entity );
            }
        }
    }

    template<typename ComponentType>
    auto InspectorPanel::RegisterComponent( eastl::string label, ComponentUIFunc callback, core::cstr group ) -> void {
        MKT_BEGIN_PROFILER();
        mComponentUis.push_back( ComponentUiInfo{
            .mLabel = eastl::move( label ),
            .mGroup = group,
            .mPresent = []( const scene::Entity& entity ) {
            return entity.HasComponent<ComponentType>();
        },
            .mCallback = callback,
            .mDraw = []( InspectorPanel& panel, scene::Entity& entity, eastl::string_view label, ComponentUIFunc callback ) {
            constexpr bool removable{ !eastl::is_same_v<ComponentType, scene::TransformComponent> };
            DrawComponent<ComponentType>( panel, label, entity, callback, removable );
        } } );
    }

    InspectorPanel::InspectorPanel( const InspectorPanelCreateInfo& createInfo )
        : Panel{ "Inspector", "Panel", createInfo.mGroup, createInfo.mIsToggleable },
          mState( createInfo.mState ) {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_ERROR_OUTLINE, mPanelName );

        RegisterComponent<scene::TransformComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_transform" ).c_str() ), ICON_MD_DEVICE_HUB ), &InspectorPanel::DrawTransformComponentTab, "Core" );

        RegisterComponent<scene::MeshComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_mesh" ).c_str() ), ICON_MD_VIEW_IN_AR ), &InspectorPanel::DrawRenderComponentTab, "Rendering" );

        // Physics
        scene::ForEachExtendedPhysicsComponent( [&]<typename T>( const char* name ) {
            eastl::string label{ name };
            label.erase( label.size() - eastl::string_view{ "Component" }.size() );
            RegisterComponent<T>( PhysicsFieldLabel( label.c_str() ), &InspectorPanel::DrawExtendedPhysicsComponentTab<T>, "Physics" );
        } );
        RegisterComponent<scene::VehicleComponent>( MKT_LOC( "inspector_vehicle" ).c_str(), &InspectorPanel::DrawVehicleComponentTab, "Physics" );
        RegisterComponent<scene::VehicleWheelComponent>( MKT_LOC( "inspector_vehicle_wheel" ).c_str(), &InspectorPanel::DrawVehicleWheelComponentTab, "Physics" );
        RegisterComponent<scene::RigidBodyComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_physics" ).c_str() ), ICON_MD_FITNESS_CENTER ), &InspectorPanel::DrawPhysicsComponentTab, "Physics" );

        RegisterComponent<scene::MeshColliderComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_mesh_collider" ).c_str() ), ICON_MD_PERSON_3 ), &InspectorPanel::DrawMeshColliderComponentTab, "Physics" );

        RegisterComponent<scene::BoxColliderComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_box_collider" ).c_str() ), ICON_MD_GIF_BOX ), &InspectorPanel::DrawBoxColliderComponentTab, "Physics" );

        RegisterComponent<scene::SphereColliderComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_sphere_collider" ).c_str() ), ICON_MD_CIRCLE ), &InspectorPanel::DrawSphereColliderComponentTab, "Physics" );

        RegisterComponent<scene::CapsuleColliderComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_capsule_collider" ).c_str() ), ICON_MD_BUNGALOW ), &InspectorPanel::DrawCapsuleColliderComponentTab, "Physics" );

        // Rendering / audio / misc
        RegisterComponent<scene::LightComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_light" ).c_str() ), ICON_MD_LIGHT ), &InspectorPanel::DrawLightComponentTab, "Rendering" );

        RegisterComponent<scene::AudioListenerComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_audio_listener" ).c_str() ), ICON_MD_SPATIAL_AUDIO ), &InspectorPanel::DrawAudioListenerComponentTab, "Audio" );

        RegisterComponent<scene::AudioSourceComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_audio" ).c_str() ), ICON_MD_AUDIOTRACK ), &InspectorPanel::DrawAudioComponentTab, "Audio" );
        RegisterComponent<scene::VideoComponent>( "Video", &InspectorPanel::DrawVideoComponentTab, "Rendering" );
        RegisterComponent<scene::AudioMixerComponent>( MKT_LOC( "audio_mixer_title" ), &InspectorPanel::DrawAudioMixerComponentTab, "Audio" );
        RegisterComponent<scene::AudioMixerGroupComponent>( MKT_LOC( "audio_group_component" ), &InspectorPanel::DrawAudioMixerGroupComponentTab, "Audio" );
        RegisterComponent<scene::ParticleSimulatorComponent>( MKT_LOC( "particle_simulator_component" ).c_str(), &InspectorPanel::DrawParticleSimulatorComponentTab, "Rendering" );
        RegisterComponent<scene::CloudComponent>( MKT_LOC( "volume_cloud" ).c_str(), &InspectorPanel::DrawVolumeComponentTab<scene::CloudComponent>, "Rendering" );
        RegisterComponent<scene::FogComponent>( MKT_LOC( "volume_fog" ).c_str(), &InspectorPanel::DrawVolumeComponentTab<scene::FogComponent>, "Rendering" );
        RegisterComponent<scene::TextureRenderComponent>( MKT_LOC( "texture_render" ).c_str(), &InspectorPanel::DrawTextureRenderComponentTab, "Rendering" );

        RegisterComponent<scene::TextComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_text" ).c_str() ), ICON_MD_MESSAGE ), &InspectorPanel::DrawTextComponentTab, "UI" );

        RegisterComponent<scene::CameraComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_camera" ).c_str() ), ICON_MD_CAMERA_ALT ), &InspectorPanel::DrawCameraComponentTab, "Rendering" );

        RegisterComponent<scene::ScriptComponent>( string::Format( "{} {}", ICON_MD_CODE, MKT_LOC( "component_menu_script" ) ), &InspectorPanel::DrawScriptingComponentTab, "Scripting" );

        RegisterComponent<scene::AnimatorComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_animator" ).c_str() ), ICON_MD_ANIMATION ), &InspectorPanel::DrawAnimatorComponentTab, "Rendering" );

        RegisterComponent<scene::SkinnedMeshRenderer>( string::Format( fmt::runtime( MKT_LOC( "inspector_skinrenderer" ).c_str() ), ICON_MD_COOKIE ), &InspectorPanel::DrawSkinMeshComponentTab, "Rendering" );

        static const eastl::string canvasIcon{ imgui::GetStringFromUnicode( 58295 ) };
        RegisterComponent<scene::CanvasComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_canvas" ).c_str() ), canvasIcon ), &InspectorPanel::DrawCanvasComponentTab, "UI" );

        // Materials
        RegisterComponent<scene::MaterialComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_material" ).c_str() ), ICON_MD_INSIGHTS ), &InspectorPanel::DrawMaterialComponentTab, "Rendering" );
        RegisterComponent<scene::SkyboxMaterialComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_skybox" ).c_str() ), ICON_MD_CLOUD ), &InspectorPanel::DrawSkyboxComponentTab, "Rendering" );
        RegisterComponent<scene::PostProcessMaterialComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_post_process" ).c_str() ), ICON_MD_DRAW ), &InspectorPanel::DrawPostProcessComponentTab, "Rendering" );

        RegisterComponent<WaterRenderComponent>( string::Format( fmt::runtime( MKT_LOC( "inspector_water_rendering" ).c_str() ), ICON_FA_WATER ), &InspectorPanel::DrawWaterComponentTab, "Rendering" );
        RegisterComponent<WaterSimulationComponent>( string::Format( "{} {}", ICON_FA_WATER, MKT_LOC( "water_simulation" ) ), &InspectorPanel::DrawWaterSimulationComponentTab, "Rendering" );
        RegisterComponent<scene::LandscapeComponent>( string::Format( "{} {}", imgui::GetStringFromUnicode( 58359 ),
                                                          MKT_LOC( "hierarchy_terrain" ) ),
            &InspectorPanel::DrawLandscapeComponentTab, "Rendering" );

        // Prepare editor
        mTextEditor.SetReadOnly( true );
        mTextEditor.SetShowWhitespaces( false );
    }

    auto InspectorPanel::DrawComponents( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !entity || !mState->mActiveScene ) {
            return;
        }
        // Registration order defines a stable inspector layout, independent of
        // EnTT storage creation order. Each entry performs one component query.
        constexpr core::cstr groups[]{ "Core", "Rendering", "Physics", "Audio", "Scripting", "UI" };

        for ( const auto* group: groups ) {
            bool heading{};

            for ( const auto& component: mComponentUis ) {
                if ( std::strcmp( component.mGroup, group ) != 0 || !component.mPresent( *entity ) ) {
                    continue;
                }

                if ( !heading ) {
                    ImGui::SeparatorText( group );
                    heading = true;
                }

                component.mDraw( *this, *entity, component.mLabel, component.mCallback );
            }
        }
    }

    auto InspectorPanel::OnRender( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mMeshInfoDrawn = false;

        if ( !mPanelIsVisible ) {
            mMeshTexturePreviews.clear();
            return;
        }

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            ImGui::End();
            mMeshTexturePreviews.clear();
            return;
        }

        mPanelIsHovered = ImGui::IsWindowHovered();
        mPanelIsFocused = ImGui::IsWindowFocused();

        if ( scene::Entity * selectedEntity{ mState->mSelectedEntity };
            selectedEntity &&
            !IsPresent<GroupComponent>( selectedEntity ) ) {
            const core::u64 id{ selectedEntity->GetComponent<scene::TagComponent>().GetGuid() };
            ImGui::PushID( static_cast<core::i32>( id >> 32 ) );
            ImGui::PushID( static_cast<core::i32>( id ) );
            DrawVisibilityCheckBox( selectedEntity );

            ImGui::SameLine();

            ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
            DrawNameTextInput( selectedEntity );

            if ( selectedEntity ) {
                // Groups are created through Scene::CreateGroup, not by changing
                // a tag. Dropdown indices therefore differ from EntityType values.
                constexpr eastl::array<scene::EntityType, 10> types{
                    scene::EntityType::eEmpty, scene::EntityType::eSound, scene::EntityType::eMaterial,
                    scene::EntityType::eLight, scene::EntityType::eText, scene::EntityType::eMesh,
                    scene::EntityType::eCamera, scene::EntityType::eCanvas, scene::EntityType::eScript, scene::EntityType::eTerrain
                };
                core::i32 type{ -1 };

                for ( core::usize index{}; index < types.size(); ++index ) {
                    if ( types[index] == selectedEntity->GetType() ) {
                        type = core::as<core::i32>( index );
                        break;
                    }
                }

                const auto& elements{ MKT_LOC( "inspector_entity_types" ) };

                ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x - .8f );

                if ( ImGui::Combo( "##TagCompEntityDropdown", &type, elements.data() ) ) {
                    selectedEntity->SetType( types[type] );
                }
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            if ( mState->mActiveScene ) {
                if ( const auto pending{ mState->mActiveScene->GetModelCreationProgress( id ) } ) {
                    const auto state{ pending->mState };

                    if ( state == asset::AssetState::eFailed ) {
                        ImGui::TextWrapped( MKT_LOC( "inspector_model_load_failed" ).c_str(), pending->mError.c_str() );

                        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_retry_model_load", "Retry model load" ).c_str() ) ) {
                            asset::AssetRegistryService::Get()->ReloadModel( pending->mReference );
                        }
                    }
                    else {
                        ImGui::TextDisabled( "%s", state == asset::AssetState::eReady ? MKT_LOC( "inspector_creating_model_entities" ).c_str() : MKT_LOC( "inspector_loading_model" ).c_str() );
                    }
                }
            }

            DrawComponents( selectedEntity );
            ImGui::Spacing();
            DrawComponentButton( selectedEntity );

            // Submit after the component widgets so their smaller drop targets
            // take priority. This rectangle adds no layout or scroll extent.
            if ( ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem ) ) {
                auto* window{ ImGui::GetCurrentWindow() };

                if ( ImGui::BeginDragDropTargetCustom( window->InnerRect, ImGui::GetID( "##InspectorScriptDrop" ) ) ) {
                    if ( const auto* payload{ ImGui::AcceptDragDropPayload( "MIKOTO_SCRIPT_SOURCE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) } ) {
                        AssignScriptPayload( *selectedEntity, *payload );
                    }

                    ImGui::EndDragDropTarget();
                }
            }

            ImGui::PopID();
            ImGui::PopID();
        }
        else if ( IsPresent<GroupComponent>( selectedEntity ) ) {
            ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
            DrawNameTextInput( selectedEntity );
        }
        else {
            ImGui::TextDisabled( "%s", MKT_LOC( "inspector_no_entity_selected" ).c_str() );
            ImGui::TextWrapped( "%s", MKT_LOC( "inspector_select_an_entity_in_the_hierarchy_or_scene_to_edit" ).c_str() );
        }

        if ( !mMeshInfoDrawn || !mPanelIsVisible ) {
            mMeshTexturePreviews.clear();
        }

        ImGui::End();
    }

    auto InspectorPanel::DrawParticleSimulatorComponentTab( scene::Entity& entity ) -> void {
        MKT_BEGIN_PROFILER();
        auto& simulator{ entity.GetComponent<scene::ParticleSimulatorComponent>() };
        auto& assets{ asset::AssetRegistryService::Get() };
        const auto filter{ filesystem::FileDialogPair{ MKT_LOC( "particle_effect_asset" ).c_str(), "mktpart" } };
        bool load{};
        bool edit{};
        bool save{};
        if ( BeginParticleSettingsRows( "##ParticleEffectActions" ) ) {
            ( void )ParticleControlRow( "particle_effect_asset" );
            load = ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_load", "Load##ParticleEffect" ).c_str(), { -1, 0 } );
            ( void )ParticleControlRow( "particle_effect_edit" );
            edit = ImGui::Button( imgui::widget::LocalizedLabel( "particle_effect_edit", "Edit##ParticleEffect" ).c_str(), { -1, 0 } );
            ( void )ParticleControlRow( "particle_effect_save" );
            ImGui::BeginDisabled( !simulator.GetEffect() || mState->mSandboxMode );
            save = ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_save", "Save##ParticleEffect" ).c_str(), { -1, 0 } );
            ImGui::EndDisabled();
            ImGui::EndTable();
        }
        if ( load ) {
            const auto file{ filesystem::OpenFileDialog( { filter } ) };
            if ( !file.IsEmpty() ) {
                try {
                    auto effect{ assets.LoadAsset<asset::ParticleEffect>( file ) };
                    if ( effect ) {
                        simulator.SetEffect( eastl::move( effect ) );
                    }
                } catch ( const std::exception& error ) {
                    MKT_CORE_LOGGER_ERROR( "Cannot load particle effect: {}", error.what() );
                }
            }
        }
        if ( edit && mState->mEditParticleEffect ) {
            mState->mEditParticleEffect( simulator.GetEffect() );
        }
        if ( save ) {
            const auto file{ filesystem::SaveFileDialog( "Effect.mktpart", { filter } ) };
            if ( !file.IsEmpty() ) {
                try {
                    assets.SaveParticleEffect( *simulator.GetEffect(), file );
                } catch ( const std::exception& error ) {
                    MKT_CORE_LOGGER_ERROR( "Cannot save particle effect: {}", error.what() );
                }
            }
        }

        const auto instances{ simulator.GetInstances() };
        const bool playing{ eastl::any_of( instances.begin(), instances.end(), []( const auto& instance ) {
            return instance.GetPlayback() == renderer::ParticlePlayback::ePlaying;
        } ) };
        if ( BeginParticleSettingsRows( "##ParticlePlayback" ) ) {
            ( void )ParticleControlRow( "particle_simulation_simulation" );
            if ( ImGui::Button( playing ? MKT_LOC( "inspector_pause" ).c_str() : MKT_LOC( "inspector_play_resume" ).c_str() ) ) {
                for ( auto& instance: instances ) {
                    if ( !playing ) {
                        instance.Play();
                    }
                    else if ( instance.GetPlayback() == renderer::ParticlePlayback::ePlaying ) {
                        instance.Pause();
                    }
                }
            }
            ImGui::SameLine();
            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_restart", "Restart##ParticleEffect" ).c_str() ) ) {
                for ( auto& instance: instances ) {
                    instance.Restart();
                }
            }
            ( void )ParticleControlRow( "particle_simulation_emission" );
            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_stop_emitting", "Stop##ParticleEffect" ).c_str() ) ) {
                for ( auto& instance: instances ) {
                    instance.Stop( false );
                }
            }
            ImGui::SameLine();
            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_clear", "Clear##ParticleEffect" ).c_str() ) ) {
                for ( auto& instance: instances ) {
                    instance.Stop();
                }
            }
            ImGui::EndTable();
        }

        const auto entityID{ entity.GetComponent<scene::TagComponent>().GetGuid() };
        if ( mParticleScene != mState->mActiveScene || mParticleEntity != entityID ) {
            mParticleScene = mState->mActiveScene;
            mParticleEntity = entityID;
            mParticleEmitter = 0;
        }

        const auto effect{ simulator.GetEffect() };
        if ( !effect ) {
            return;
        }

        const auto label{ []( core::usize index ) {
            return string::Format( "{} {}", MKT_LOC( "particle_simulation_particle_emitter" ), index + 1 );
        } };
        const auto& emitters{ effect->GetEmitters() };
        mParticleEmitter = emitters.empty() ? 0 : eastl::min( mParticleEmitter, emitters.size() - 1 );

        ImGui::Separator();
        if ( BeginParticleSettingsRows( "##InspectorParticleEmitters" ) ) {
            ( void )ParticleControlRow( "particle_simulation_particle_emitter" );
            const auto current{ emitters.empty() ? MKT_LOC( "particle_effect_no_emitters" ) : label( mParticleEmitter ) };
            if ( ImGui::BeginCombo( "##InspectorParticleEmitter", current.c_str() ) ) {
                for ( core::usize index{}; index < emitters.size(); ++index ) {
                    if ( ImGui::Selectable( label( index ).c_str(), index == mParticleEmitter ) ) {
                        mParticleEmitter = index;
                    }
                }
                ImGui::EndCombo();
            }

            ( void )ParticleControlRow( "particle_effect_emitters" );
            ImGui::Text( "%zu", simulator.GetInstances().size() );
            ImGui::SameLine();
            if ( imgui::widget::ToolbarIconButton( "Add##InspectorEmitter", ICON_MD_ADD, MKT_LOC( "particle_effect_add_emitter" ).c_str() ) ) {
                auto updated{ emitters };
                updated.push_back( renderer::ParticleEmitterHandle::New() );
                mParticleEmitter = updated.size() - 1;
                simulator.SetEffect( asset::ParticleEffectHandle::New( eastl::move( updated ) ) );
            }

            ImGui::SameLine();
            ImGui::BeginDisabled( simulator.GetInstances().empty() );
            if ( imgui::widget::ToolbarIconButton( "Remove##InspectorEmitter", ICON_MD_REMOVE, MKT_LOC( "particle_effect_remove_emitter" ).c_str() ) ) {
                auto updated{ simulator.GetEffect()->GetEmitters() };
                updated.erase( updated.begin() + mParticleEmitter );
                mParticleEmitter = updated.empty() ? 0 : eastl::min( mParticleEmitter, updated.size() - 1 );
                simulator.SetEffect( asset::ParticleEffectHandle::New( eastl::move( updated ) ) );
            }
            ImGui::EndDisabled();
            ImGui::EndTable();
        }

        if ( simulator.GetInstances().empty() ) {
            return;
        }

        const auto emitter{ simulator.GetEffect()->GetEmitters()[mParticleEmitter] };
        auto settings{ emitter->GetSettings() };
        auto material{ emitter->GetMaterial() };
        try {
            bool changed{ DrawParticleEmitterMaterial( material, mState->mSandboxMode ) };
            changed |= DrawParticleEmitterSettings( settings );
            if ( changed ) {
                simulator.SetEmitter( mParticleEmitter, renderer::ParticleEmitterHandle::New(
                    renderer::ParticleEmitterCreateDescription{ .mSettings = settings, .mMaterial = material } ) );
            }
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Cannot edit particle emitter: {}", error.what() );
        }
    }

    auto InspectorPanel::DrawCanvasComponentTab( scene::Entity& entity ) -> void {
        auto& canvas = entity.GetComponent<scene::CanvasComponent>().GetCanvas();
        bool overlay = canvas.IsOverlay();

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_screen_overlay", "Screen overlay" ).c_str(), overlay ) ) {
            canvas.SetOverlay( overlay );
        }

        auto size = canvas.GetReferenceSize();

        if ( ImGui::DragFloat2( imgui::widget::LocalizedLabel( "inspector_reference_resolution", "Reference resolution" ).c_str(), glm::value_ptr( size ), 1, 1, 16384, "%.0f", ImGuiSliderFlags_AlwaysClamp ) ) {
            canvas.SetReferenceSize( size );
        }

        if ( !overlay ) {
            float scale = canvas.GetWorldScale();

            if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_world_units_per_pixel", "World units per pixel" ).c_str(), &scale, 0.0001f, 0.000001f, 100, "%.6f", ImGuiSliderFlags_AlwaysClamp ) ) {
                canvas.SetWorldScale( scale );
            }
        }

        int order = canvas.GetSortOrder();

        if ( ImGui::InputInt( imgui::widget::LocalizedLabel( "inspector_sort_order", "Sort order" ).c_str(), &order ) ) {
            canvas.SetSortOrder( order );
        }

        const auto entityId{ entity.GetComponent<scene::TagComponent>().GetGuid() };

        if ( mCanvasScene != mState->mActiveScene || mCanvasEntity != entityId ) {
            mCanvasScene = mState->mActiveScene;
            mCanvasEntity = entityId;
            mCanvasElement = 0;
        }

        if ( !canvas.FindElement( mCanvasElement ) ) {
            mCanvasElement = 0;
        }

        const char* names[]{ MKT_LOC( "inspector_panel" ).c_str(), MKT_LOC( "inspector_rectangle_outline" ).c_str(), MKT_LOC( "inspector_image" ).c_str(), MKT_LOC( "inspector_text" ).c_str(), MKT_LOC( "inspector_line" ).c_str(), MKT_LOC( "inspector_button" ).c_str(), MKT_LOC( "inspector_toggle" ).c_str(), MKT_LOC( "inspector_slider" ).c_str() };
        ImGui::TextDisabled( MKT_LOC( "inspector_widgets_belong_to_this_canvas_they_are_not_scene_entities" ).c_str() );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_add_widget", "Add widget" ).c_str() ) ) {
            ImGui::OpenPopup( "CanvasAddRoot" );
        }

        ImGui::SameLine();
        ImGui::BeginDisabled( !mCanvasElement );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_add_child", "Add child" ).c_str() ) ) {
            ImGui::OpenPopup( "CanvasAddChild" );
        }

        ImGui::EndDisabled();
        const auto addMenu = [&]( const char* popup, renderer::CanvasElementId parent ) {
            if ( ImGui::BeginPopup( popup ) ) {
                for ( core::i32 type{}; type < 8; ++type ) {
                    if ( ImGui::MenuItem( names[type] ) ) {
                        renderer::CanvasCommand command{};
                        command.mType = static_cast<renderer::CanvasCommandType>( type );
                        command.mName = names[type];
                        command.mTransform.mAnchoredPosition = parent ? core::float2{ 12, 12 } : core::float2{ 40, 40 };
                        command.mTransform.mSizeDelta = { 240, 64 };
                        command.mColor = { 0.25f, 0.45f, 0.8f, 1 };

                        if ( command.mType == renderer::CanvasCommandType::eText || command.mType == renderer::CanvasCommandType::eSprite ) {
                            command.mColor = { 1, 1, 1, 1 };
                        }

                        command.mEnd = { 240, 0 };
                        command.mThickness = 2;
                        command.mRounding = 6;

                        if ( command.mType == renderer::CanvasCommandType::eText || command.mType >= renderer::CanvasCommandType::eButton ) {
                            command.mText = names[type];
                        }

                        mCanvasElement = canvas.AddElement( eastl::move( command ), parent );
                    }
                }

                ImGui::EndPopup();
            }
        };
        addMenu( "CanvasAddRoot", 0 );
        addMenu( "CanvasAddChild", mCanvasElement );

        // Preorder follows the renderer's painter order. Indented selectable rows
        // expose ownership without manufacturing scene entities for UI controls.
        if ( ImGui::BeginChild( "Widget hierarchy", { 0, 180 }, ImGuiChildFlags_Borders ) ) {
            const auto commands{ canvas.GetCommands() };

            for ( const auto& element: canvas.GetResolvedElements() ) {
                const auto& command{ commands[element.mCommandIndex] };
                const auto id{ string::Format( "{}", command.mId ) };
                ImGui::PushID( id.c_str() );
                const core::f32 indent{ glm::min( element.mDepth, 16u ) * 14.0f };
                ImGui::Indent( indent + 1 );
                const auto label{ string::Format( "{} ({})", command.mName.empty() ? names[static_cast<core::u32>( command.mType )] : command.mName.c_str(), command.mId ) };

                if ( ImGui::Selectable( label.c_str(), command.mId == mCanvasElement ) ) {
                    mCanvasElement = command.mId;
                }

                ImGui::Unindent( indent + 1 );
                ImGui::PopID();
            }
        }

        ImGui::EndChild();
        auto* selected{ canvas.FindElement( mCanvasElement ) };

        if ( !selected ) {
            ImGui::TextDisabled( MKT_LOC( "inspector_select_a_widget_to_edit_its_properties" ).c_str() );
            return;
        }

        // These buffers retain capacity across frames. Only the active text needs
        // copying; a large canvas no longer allocates an editor buffer per widget.
        const auto prepareBuffer = []( eastl::vector<char>& buffer, const eastl::string& text, core::usize minimum ) {
            if ( buffer.size() < text.size() + 1024 || buffer.size() < minimum ) {
                buffer.resize( eastl::max( minimum, text.size() + 1024 ) );
            }

            std::memcpy( buffer.data(), text.data(), text.size() );
            buffer[text.size()] = '\0';
        };
        auto& c{ *selected };
        prepareBuffer( mCanvasNameBuffer, c.mName, 256 );

        if ( ImGui::InputText( imgui::widget::LocalizedLabel( "inspector_name", "Name" ).c_str(), mCanvasNameBuffer.data(), mCanvasNameBuffer.size() ) ) {
            c.mName = mCanvasNameBuffer.data();
        }

        ImGui::TextDisabled( MKT_LOC( "inspector_widget_id" ).c_str(), static_cast<unsigned long long>( c.mId ) );
        const auto* parent{ canvas.FindElement( c.mParentId ) };

        if ( ImGui::BeginCombo( imgui::widget::LocalizedLabel( "inspector_parent", "Parent" ).c_str(), parent ? parent->mName.c_str() : MKT_LOC( "inspector_canvas_root" ).c_str() ) ) {
            if ( ImGui::Selectable( imgui::widget::LocalizedLabel( "inspector_canvas_root", "Canvas root" ).c_str(), !c.mParentId ) ) {
                canvas.SetParent( c.mId, 0 );
            }

            for ( const auto& candidate: canvas.GetCommands() ) {
                // Exclude the selected subtree; reparenting may never form a cycle.
                const renderer::CanvasCommand* ancestor{ &candidate };
                bool descendant{};

                while ( ancestor ) {
                    if ( ancestor->mId == c.mId ) {
                        descendant = true;
                        break;
                    }

                    ancestor = canvas.FindElement( ancestor->mParentId );
                }

                if ( descendant ) {
                    continue;
                }

                const auto label{ string::Format( "{} ({})", candidate.mName.c_str(), candidate.mId ) };

                if ( ImGui::Selectable( label.c_str(), c.mParentId == candidate.mId ) ) {
                    canvas.SetParent( c.mId, candidate.mId );
                }
            }

            ImGui::EndCombo();
        }

        imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_visible", "Visible" ).c_str(), c.mVisible );
        ImGui::SameLine();
        imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_enabled", "Enabled" ).c_str(), c.mEnabled );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_interactable", "Interactable" ).c_str(), c.mInteractable );
        ImGui::TextDisabled( MKT_LOC( "inspector_tab_to_focus_enter_space_to_activate_arrows_adjust_sliders" ).c_str() );
        auto& t = c.mTransform;
        int mode = static_cast<int>( t.mLayoutMode );

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "inspector_layout", "Layout" ).c_str(), &mode, MKT_LOC( "inspector_absolute_anchored_stretched" ).c_str() ) ) {
            t.mLayoutMode = static_cast<renderer::CanvasLayoutMode>( mode );
        }

        const bool image{ c.mType == renderer::CanvasCommandType::eSprite };

        if ( image ) {
            imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_enable_uniform_resize", "Enable uniform resize" ).c_str(), t.mUniformResize );
        }

        auto parentSize{ canvas.GetReferenceSize() };

        if ( image ) {
            if ( mState->mActiveCamera ) {
                const auto viewport{ mState->mActiveCamera->GetViewPort() };
                parentSize = canvas.GetLayoutSize( { viewport.first, viewport.second } );
            }

            if ( c.mParentId ) {
                const auto commands{ canvas.GetCommands() };

                for ( const auto& element: canvas.GetResolvedElements( parentSize ) ) {
                    if ( commands[element.mCommandIndex].mId == c.mParentId ) {
                        parentSize = element.mRect.mSize;
                        break;
                    }
                }
            }
        }

        const auto drawResizeValues{ [&]( const char* label, core::float2& value, float speed ) {
            const auto previous{ value };

            if ( !ImGui::DragFloat2( label, glm::value_ptr( value ), speed ) ) {
                return false;
            }

            if ( image && t.mUniformResize ) {
                const core::u32 axis{ value.x != previous.x ? 0u : 1u };

                if ( previous[axis] != 0.0f ) {
                    value[1 - axis] = previous[1 - axis] * ( value[axis] / previous[axis] );
                }
                else {
                    // A collapsed image has no aspect ratio; let both axes grow again.
                    value[1 - axis] += value[axis] - previous[axis];
                }
            }

            return true;
        } };

        if ( mode ) {
            ImGui::SliderFloat2( imgui::widget::LocalizedLabel( "inspector_anchor_min", "Anchor min" ).c_str(), glm::value_ptr( t.mAnchorMin ), 0, 1 );
            ImGui::SliderFloat2( imgui::widget::LocalizedLabel( "inspector_anchor_max", "Anchor max" ).c_str(), glm::value_ptr( t.mAnchorMax ), 0, 1 );
            t.mAnchorMax = glm::max( t.mAnchorMax, t.mAnchorMin );
        }

        if ( mode == 2 ) {
            float edges[]{ t.mEdges.mLeft, t.mEdges.mTop, t.mEdges.mRight, t.mEdges.mBottom };

            if ( ImGui::DragFloat4( imgui::widget::LocalizedLabel( "inspector_margins_l_t_r_b", "Margins L/T/R/B" ).c_str(), edges, 1 ) ) {
                t.mEdges = { edges[0], edges[1], edges[2], edges[3] };
            }
        }
        else {
            ImGui::DragFloat2( imgui::widget::LocalizedLabel( "inspector_position", "Position" ).c_str(), glm::value_ptr( t.mAnchoredPosition ), 1 );

            if ( !image ) {
                ImGui::DragFloat2( imgui::widget::LocalizedLabel( "inspector_size", "Size" ).c_str(), glm::value_ptr( t.mSizeDelta ), 1 );
            }
        }

        if ( image ) {
            auto size{ t.Resolve( parentSize ).mSize };

            if ( drawResizeValues( imgui::widget::LocalizedLabel( "inspector_size", "Size" ).c_str(), size, 1.0f ) ) {
                t.SetSize( size, parentSize );
            }
        }

        ImGui::SliderFloat2( imgui::widget::LocalizedLabel( "inspector_pivot", "Pivot" ).c_str(), glm::value_ptr( t.mPivot ), 0, 1 );
        drawResizeValues( imgui::widget::LocalizedLabel( "inspector_scale", "Scale" ).c_str(), t.mScale, 0.01f );
        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_rotation", "Rotation" ).c_str(), &t.mRotation, 0.5f, 0, 0, MKT_LOC( "inspector_deg" ).c_str() );
        float color[]{ c.mColor.mR, c.mColor.mG, c.mColor.mB, c.mColor.mA };

        if ( ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "inspector_color", "Color" ).c_str(), color ) ) {
            c.mColor = { color[0], color[1], color[2], color[3] };
        }

        ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_rounding", "Rounding" ).c_str(), &c.mRounding, 0.5f, 0, 1024, "%.1f", ImGuiSliderFlags_AlwaysClamp );

        if ( c.mType == renderer::CanvasCommandType::eLine || c.mType == renderer::CanvasCommandType::eRect ) {
            ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_thickness", "Thickness" ).c_str(), &c.mThickness, 0.1f, 0, 1024, "%.1f", ImGuiSliderFlags_AlwaysClamp );
        }

        if ( c.mType == renderer::CanvasCommandType::eLine ) {
            ImGui::DragFloat2( imgui::widget::LocalizedLabel( "inspector_start", "Start" ).c_str(), glm::value_ptr( c.mStart ), 1 );
            ImGui::DragFloat2( imgui::widget::LocalizedLabel( "inspector_end", "End" ).c_str(), glm::value_ptr( c.mEnd ), 1 );
        }

        if ( c.mType == renderer::CanvasCommandType::eSprite ) {
            eastl::string imagePath{ c.GetTextureAsset().GetRef().GetPath().GetC_Str() };
            ImGui::SetNextItemWidth( -FLT_MIN );
            ImGui::InputText( "##CanvasImagePath", imagePath.data(), imagePath.size() + 1,
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll );

            if ( !imagePath.empty() && ImGui::IsItemHovered() ) {
                imgui::widget::ItemTooltip( "%s", imagePath.c_str() );
            }

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_choose_image", "Choose image..." ).c_str() ) ) {
                std::initializer_list<filesystem::FileDialogPair> filters{ { MKT_LOC( "inspector_image" ).c_str(), "png,jpg,jpeg,tga,bmp,ktx2" } };
                const auto path = filesystem::OpenFileDialog( filters );

                if ( !path.IsEmpty() ) {
                    c.SetTexture( asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, rhi::TextureDimension::eTexture2D ) );
                }
            }

            ImGui::SameLine();
            ImGui::BeginDisabled( !c.GetTextureAsset() && !c.GetTexture() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_remove_image", "Remove image" ).c_str() ) ) {
                c.SetTexture( rhi::TextureHandle{} );
            }

            ImGui::EndDisabled();

            const auto texture{ c.GetTexture() };
            ImGui::BeginDisabled( texture.IsEmpty() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_match_texture_dimensions", "Match texture dimensions" ).c_str() ) ) {
                t.SetSize( { core::as<float>( texture->GetWidth() ), core::as<float>( texture->GetHeight() ) }, parentSize );
            }

            ImGui::EndDisabled();
        }

        if ( c.mType == renderer::CanvasCommandType::eText || c.mType >= renderer::CanvasCommandType::eButton ) {
            prepareBuffer( mCanvasTextBuffer, c.mText, 4096 );

            if ( ImGui::InputTextMultiline( imgui::widget::LocalizedLabel( "inspector_text", "Text" ).c_str(), mCanvasTextBuffer.data(), mCanvasTextBuffer.size(), { 0, 90 } ) ) {
                c.mText = mCanvasTextBuffer.data();
            }

            ImGui::DragFloat( imgui::widget::LocalizedLabel( "inspector_font_size", "Font size" ).c_str(), &c.mFontSize, 0.5f, 1, 2048, "%.1f", ImGuiSliderFlags_AlwaysClamp );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_choose_font", "Choose font..." ).c_str() ) ) {
                std::initializer_list<filesystem::FileDialogPair> filters{ { MKT_LOC( "inspector_font" ).c_str(), "ttf,otf" } };
                const auto path = filesystem::OpenFileDialog( filters );

                if ( !path.IsEmpty() ) {
                    c.mFont = asset::AssetRegistryService::Get()->LoadAsset<renderer::Font>( path );
                }
            }

            if ( !c.mFont.IsEmpty() ) {
                ImGui::TextWrapped( "%s", c.mFont->GetPath().GetC_Str() );
            }
            else {
                ImGui::TextDisabled( MKT_LOC( "inspector_default_open_sans" ).c_str() );
            }
        }

        if ( c.mType == renderer::CanvasCommandType::eSlider ) {
            auto value{ c.mValue };

            if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "inspector_value", "Value" ).c_str(), &value, 0, 1 ) ) {
                canvas.SetValue( c.mId, value );
            }
        }

        if ( c.mType == renderer::CanvasCommandType::eToggle ) {
            bool checked = c.mValue > 0.5f;

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_checked", "Checked" ).c_str(), checked ) ) {
                canvas.SetValue( c.mId, checked ? 1.0f : 0.0f );
            }
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_remove_widget_and_children", "Remove widget and children" ).c_str() ) ) {
            canvas.RemoveElement( mCanvasElement );
            mCanvasElement = 0;
            return;
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_move_earlier", "Move earlier" ).c_str() ) ) {
            canvas.MoveElement( mCanvasElement, -1 );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_move_later", "Move later" ).c_str() ) ) {
            canvas.MoveElement( mCanvasElement, 1 );
        }
    }
}// namespace mikoto::editor
