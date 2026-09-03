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
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_string.h>

#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Reflect.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>

#include <Animation/AnimationSystem.hh>
#include <Animation/SkinnedAnimation.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Memory/Allocator.hh>

#include <Layers/EditorLayer.hh>

#include <Panels/InspectorPanel.hh>

#include <Scripting/ScriptingService.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    template<typename ComponentType>
    static auto DrawComponent(
            InspectorPanel& panel,
            const eastl::string_view componentLabel,
            Entity& entity,
            const InspectorPanel::ComponentUIFunc& uiFunc,
            const bool hasRemoveButton = true ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        constexpr ImGuiTreeNodeFlags treeNodeFlags{
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_FramePadding };

        if ( entity.HasComponent<ComponentType>() ) {
            bool removeComponent{ false };
            const ImVec2 contentRegionAvailable{ ImGui::GetContentRegionAvail() };

            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f } );

            // See ImGui implementation for button dimensions computation
            const float lineHeight{ GImGui->FontSize + GImGui->Style.FramePadding.y * 2.0f };
            const bool componentNodeOpen{ ImGui::TreeNodeEx( rc_cast<const void*>( &entity.GetComponent<ComponentType>() ), treeNodeFlags, "%s", componentLabel.data() ) };

            SetCursorHandOnLastItemHovered();

            ImGui::PopStyleVar();

            if ( hasRemoveButton ) {
                ImGui::SameLine( contentRegionAvailable.x - lineHeight * 0.5f );
                if ( ImGui::Button( string::Format( "{}##DrawComponent_Button{}", ICON_MD_SETTINGS,  typeid( ComponentType ).hash_code() ).c_str(), ImVec2{ lineHeight, lineHeight } ) ) {
                    ImGui::OpenPopup( "##DrawComponent_ButtonPopUp" );
                }

                SetCursorHandOnLastItemHovered();

                if ( ImGui::BeginPopup( "##DrawComponent_ButtonPopUp" ) ) {
                    if ( ImGui::MenuItem( "Remove Component" ) ) {
                        removeComponent = true;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }

            if ( componentNodeOpen ) {
                ( panel.*uiFunc )( entity );
                ImGui::TreePop();
            }

            if ( removeComponent ) {
                entity.RemoveComponent<ComponentType>();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
    }

    static auto ShowTextureHoverTooltip( const ITexture* texture ) -> void {
        ( void )PushImageButton( ( u64 )texture, ImGuiService::Get()->GetTextureID( texture ), ImVec2{ 128, 128 } );

        ImGui::SameLine();

        // Table showing texture properties
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp };

        if ( ImGui::BeginTable( "MaterialEditorDiffusePropertiesTable", 2, tableFlags ) ) {
            // First row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Dimensions" );

            // First row - second colum
            ImGui::TableSetColumnIndex( 1 );
            u32 width{ as<u32>( texture->GetWidth() ) };
            u32 height{ as<u32>( texture->GetHeight() ) };
            ImGui::TextUnformatted( fmt::format( "[{}, {}]", width, height ).c_str() );

            // Second row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Format" );

            // Second row - second colum
            ImGui::TableSetColumnIndex( 1 );
            const FormatInfo& formatInfo{ rhi::GetFormatInfo( texture->GetFormat() ) };
            ImGui::TextUnformatted(formatInfo.mName );

            // Third row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Size (memory)" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            f32 sizeMB{ (f32)((texture->GetWidth() * texture->GetHeight()) * formatInfo.mBytesPerBlock) };
            ImGui::TextUnformatted( string::Format( "{} MB", math::Round( sizeMB / 1'000'000, 2 ) ).c_str() );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Antialiasing" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            auto multisamplingToString = [](Multisampling value) -> eastl::string_view {
                switch (value) {
                    case Multisampling::eMsaaX1:  return "MsaaX1";
                    case Multisampling::eMsaaX2:  return "MsaaX2";
                    case Multisampling::eMsaaX4:  return "MsaaX4";
                    case Multisampling::eMsaaX8:  return "MsaaX8";
                    case Multisampling::eMsaaX16: return "MsaaX16";
                    default:                      return "Unknown";
                }
            };
            ImGui::TextUnformatted( multisamplingToString( texture->GetSampleCount() ).data() );

            ImGui::EndTable();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
    }

    static auto UpdateMaterialTexture( PhysicalMaterial& standardMat, MapType mapType ) -> void {
        const std::initializer_list<FileDialogPair> filters{
            { "Textures", "jpg,jpeg,png" },
            { "JPG", "jpg" },
            { "JPEG", "jpeg" },
            { "PNG", "png" }
        };

        const Path path{ filesystem::OpenFileDialog( filters ) };
        if ( !path.IsEmpty() ) {
            TextureHandle texture{ asset::AssetsService::Get()->LoadAsset<ITexture>( path, TextureDimension::eTexture2D ) };
            if (!texture.IsEmpty()) {
                standardMat.SetTexture( mapType, texture );
            }
        }
    }

    static auto LoadMaterialTexture( SkyboxMaterial& skyboxMat, SkyboxFace face ) -> void {
        const std::initializer_list<FileDialogPair> filters{
                { "Textures", "jpg,jpeg,png" },
                { "JPG", "jpg" },
                { "JPEG", "jpeg" },
                { "PNG", "png" }
        };

        const Path path{ filesystem::OpenFileDialog( filters ) };

        if ( !path.IsEmpty() ) {
            TextureHandle textureHandle{ AssetsService::Get()->LoadAsset<ITexture>( path, TextureDimension::eTexture2D ) };
            if ( !textureHandle.IsEmpty() ) {
                skyboxMat.SetFace( face, textureHandle );
            }
        }
    }

    static auto LoadMaterialTexture( SkyboxMaterial& skyboxMat ) -> void {
        const std::initializer_list<FileDialogPair> filters{
            { "Textures", "jpg,jpeg,png,hdr" },
            { "JPG", "jpg" },
            { "JPEG", "jpeg" },
            { "PNG", "png" },
            { "HDR", "hdr" }
        };

        const Path path{ filesystem::OpenFileDialog( filters ) };

        if ( !path.IsEmpty() ) {
            TextureHandle textureHandle{ AssetsService::Get()->LoadAsset<ITexture>( path, TextureDimension::eTexture2D ) };
            if ( !textureHandle.IsEmpty() ) {
                skyboxMat.SetEquirectangular( textureHandle );
            }
        }
    }

    static auto DisplayTextureEditTreeNode( const eastl::string_view title, PhysicalMaterial& standardMat, const eastl::function<void( PhysicalMaterial& standardMat )>& func ) -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_FramePadding };

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const eastl::string nodeLabel{ string::Format( "##{}:{}", "DisplayTextureEditTreeNode", title.data() ) };
        if ( ImGui::TreeNodeEx( nodeLabel.c_str(), treeNodeFlags, "%s", title.data() ) ) {
            func( standardMat );
            ImGui::TreePop();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
    }

    static auto EditDiffuseProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Diffuse" );

        TextureHandle diffuseMap{ material.GetTexture( MapType::eDiffuse ) };
        if ( diffuseMap.IsEmpty() ) {
            diffuseMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( PushImageButton( "##EditDiffuseProperties:TextureID", ImGuiService::Get()->GetTextureID( diffuseMap.GetRaw() ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eDiffuse );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstAlbedoMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eDiffuse, dstAlbedoMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eDiffuse ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( material.GetTexture( MapType::eDiffuse ).GetRaw() );
            },ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eDiffuse ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "DiffuseMapEditContentsTable", 1, tableFlags ) ) {
            constexpr auto columnIndex{ 0 };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            glm::vec4 color{ material.GetDiffuseFactor() };
            constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

            if ( ImGui::ColorEdit3( "Diffuse factor", glm::value_ptr( color ), colorEditFlags ) ) {
                material.SetDiffuseFactor( color );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eDiffuse );
            }

            ImGui::EndTable();
        }
    }

    static auto EditBaseColorProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Albedo" );

        TextureHandle diffuseMap{ material.GetTexture( MapType::eBaseColor ) };
        if ( diffuseMap.IsEmpty() ) {
            diffuseMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( imgui::PushImageButton( "##EditBaseColorProperties:TextureID", ImGuiService::Get()->GetTextureID( diffuseMap.GetRaw() ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eBaseColor );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstAlbedoMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eBaseColor, dstAlbedoMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eBaseColor ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( material.GetTexture( MapType::eBaseColor ).GetRaw() );
            }, ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eBaseColor ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Table to control albedo mix color and ambient value
        // Table has two rows and one colum
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "AlbedoMapEditContentsTable", 1, tableFlags ) ) {
            constexpr auto columnIndex{ 0 };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            glm::vec4 color{ material.GetBaseColorFactor() };
            constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

            if ( ImGui::ColorEdit3( "Color", glm::value_ptr( color ), colorEditFlags ) ) {
                material.SetBaseColorFactor( color );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            // Merge color with objects base color
            float cutOff{ material.GetAlphaMaskCutoff() };
            if ( imgui::Slider( "Alpha Cut-Off", cutOff, { 0.0f, 1.0f } ) ) {
                material.SetAlphaMaskCutoff( cutOff );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eBaseColor );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            AlphaMode currentAlphaMode{ material.GetAlphaMask() };
            std::array<std::string, as<size_t>(AlphaMode::eCount)> choicesAlpha{
                "Opaque", "Mask", "Blend",
            };

            AlphaMode newAlphaMode{ imgui::Combo( choicesAlpha, currentAlphaMode ) };
            if (currentAlphaMode != newAlphaMode) {
                material.SetAlphaMask( newAlphaMode );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            Workflow currentWorkFlow{ material.GetWorkflow() };
            std::array<std::string, static_cast<size_t>(Workflow::eCount)> choicesWorkflow{
                "Metallic-Roughness", "Specular-Glossiness",
            };

            Workflow newWorkFlow{ imgui::Combo( choicesWorkflow, currentWorkFlow ) };
            if (newWorkFlow != currentWorkFlow) {
                material.SetWorkflow( newWorkFlow );
            }

            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Spacing();

        EditDiffuseProperties( material );
    }

    static auto EditMetallicRoughnessProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Metallic-Roughness" );

        TextureHandle metallicMap{ material.GetTexture( MapType::eMetallicRoughness ) };
        if ( metallicMap.IsEmpty() ) {
            metallicMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( imgui::PushImageButton( "##EditMetallicRoughnessProperties:TextureID", ImGuiService::Get()->GetTextureID( metallicMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eMetallicRoughness );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstMetallicMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eMetallicRoughness, dstMetallicMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eMetallicRoughness ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( metallicMap.GetRaw() );
            },ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eMetallicRoughness ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        constexpr auto columnCount{ 1 };
        constexpr auto columnIndexSpecular{ 0 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "MetallicRoughnessEditContentsTable", columnCount, specularTableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            static float perceptualRoughness{ 0 }; // TODO
            if ( imgui::Slider( "Perceptual Roughness", perceptualRoughness, { 0.0f, 1.0f } ) ) {
            }

            imgui::SetCursorHandOnLastItemHovered();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eMetallicRoughness );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditMetallicProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Metallic" );

        TextureHandle metallicMap{ material.GetTexture( MapType::eMetallic ) };
        if ( metallicMap.IsEmpty() ) {
            metallicMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( imgui::PushImageButton( "##EditMetallicProperties::TextureID", ImGuiService::Get()->GetTextureID( metallicMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eMetallic );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstMetallicMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eMetallic, dstMetallicMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eMetallic ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( metallicMap.GetRaw() );
            }, ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eMetallic ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Table to control specular component
        // Table has one row and one colum
        constexpr auto columnCount{ 1 };
        constexpr auto columnIndexSpecular{ 0 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "MetallicMapEditContentsTable", columnCount, specularTableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            float strength{ material.GetMetallicFactor() };
            if ( imgui::Slider( "Metal factor", strength, { 0.0f, 1.0f } ) ) {
                material.SetMetallicFactor( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eMetallic );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }

        EditMetallicRoughnessProperties( material );
    }

    static auto EditNormalsProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Normal" );

        TextureHandle normalMap{ material.GetTexture( MapType::eNormal ) };
        if ( normalMap.IsEmpty() ) {
            normalMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( imgui::PushImageButton( (u64)normalMap, ImGuiService::Get()->GetTextureID( normalMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eNormal );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstNormalMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eNormal, dstNormalMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eNormal ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( normalMap.GetRaw() );
            }, ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eNormal ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Table to control specular component
        // Table has one row and one colum
        constexpr auto columnCount{ 1 };
        constexpr auto columnIndexSpecular{ 0 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "NormalMapEditContentsTable", columnCount, specularTableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );
            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eNormal );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditEmissionProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Emission" );

        TextureHandle normalMap{ material.GetTexture( MapType::eEmissive ) };
        if ( normalMap.IsEmpty() ) {
            normalMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( imgui::PushImageButton( (u64)normalMap, ImGuiService::Get()->GetTextureID( normalMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eEmissive );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstNormalMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eEmissive, dstNormalMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eEmissive ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( normalMap.GetRaw() );
            }, ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eEmissive ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Table to control specular component
        // Table has one row and one colum
        constexpr auto columnCount{ 1 };
        constexpr auto columnIndexSpecular{ 0 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "NormalMapEditContentsTable", columnCount, specularTableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            float3 factors{ material.GetEmissiveFactor() };
            if ( imgui::ColorEdit3( "Factors", factors ) ) {
                material.SetEmissiveFactor( factors );
            }

            float strength{ material.GetEmissiveStrength() };
            if ( imgui::Slider( "Strength", strength, { 0.0f, 10.0f } ) ) {
                material.SetEmissiveStrength( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );
            bool isBloomy{ material.IsBloomy() };
            if (imgui::CheckBox( "##EditEmissionProperties:IsBloomy", isBloomy ) ) {
                material.EnableBloom( isBloomy );
            }

            ImGui::SameLine();
            ImGui::TextUnformatted( "Enable bloom" );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );
            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eEmissive );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditRoughnessProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Roughness" );

        TextureHandle roughnessMap{ material.GetTexture( MapType::eRoughness ) };
        if ( roughnessMap.IsEmpty() ) {
            roughnessMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( imgui::PushImageButton( "##EditRoughnessProperties:TextureID", ImGuiService::Get()->GetTextureID( roughnessMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eRoughness );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstRoughnessMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eRoughness, dstRoughnessMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eRoughness ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( roughnessMap.GetRaw() );
            }, ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eRoughness ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Table to control specular component
        // Table has one row and one colum
        constexpr auto columnCount{ 1 };
        constexpr auto columnIndexSpecular{ 0 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "RoughnessMapEditContentsTable", columnCount, specularTableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            float strength{ material.GetRoughnessFactor() };
            if ( imgui::Slider( "Roughness", strength, { 0.0f, 1.0f } ) ) {
                material.SetRoughnessFactor( strength );
            }

            imgui::SetCursorHandOnLastItemHovered();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eRoughness );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditAmbientOcclusionProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Ambient Occlusion" );

        TextureHandle aoMap{ material.GetTexture( MapType::eAmbientOcclusion ) };
        if ( aoMap.IsEmpty() ) {
            aoMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( imgui::PushImageButton( (u64)aoMap, ImGuiService::Get()->GetTextureID( aoMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::eAmbientOcclusion );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                TextureHandle dstAoMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::eAmbientOcclusion, dstAoMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
            }

            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::eAmbientOcclusion ) ) {
            imgui::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( aoMap.GetRaw() );
            }, ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::eAmbientOcclusion ) ) {
                imgui::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Table to control specular component
        // Table has one row and one colum
        constexpr auto columnCount{ 1 };
        constexpr auto columnIndexSpecular{ 0 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None };
        if ( ImGui::BeginTable( "AmbientOccEditContentsTable", columnCount, specularTableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            float factor{ material.GetAoFactor() };

            if ( imgui::Slider( "AO Factor", factor, { 0.0f, 10.0f } ) ) {
                material.SetAoFactor( factor );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::eAmbientOcclusion );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditMaterial( PhysicalMaterial* material, bool isInternal ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( material == nullptr ) {
            return;
        }

        // Name
        ImGui::TextUnformatted( "Material name ");
        ImGui::SameLine();
        ( void )InputText( material->GetName(), true );

        ImGui::SameLine();

        // Cannot serialize internal materials, they are
        // part of the model file and loaded along with it
        ImGui::BeginDisabled(isInternal);

        eastl::string saveMaterialButtonID{ string::Format("{}##SaveMaterialButton", ICON_MD_SAVE_ALT ) };
        if (ImGui::Button( saveMaterialButtonID.c_str() )) {
            const std::initializer_list<FileDialogPair> filters{
                { "Material", filesystem::kMikotoMaterialExtension.data() } };
            Path materialPath{ SaveFileDialog( material->GetName(), filters ) };
            ( void )AssetsService::Get()->SerializeMaterial( MaterialHandle::Create( material ) );
        }

        SetCursorHandOnLastItemHovered();

        ImGui::EndDisabled();

        widget::MakeHelpPopUpDelay( "Save material", "" );

        // Select shading model
        ImGui::Spacing();
        ImGui::TextUnformatted( "Shading model");

        ImGui::SameLine();
        ShadingModel currentShadingModel{ material->GetShadingModel() };
        eastl::array<std::string, as<usize>(ShadingModel::eCount)> choicesAlpha{
            "Default PBR",
            "Clear Coat",
            "Toon Shading",
            "Flat Shading",
            "Cell Shading",
            "Subsurface Scattering" };
        ShadingModel newShadingModel{ Combo( choicesAlpha, currentShadingModel ) };
        if (newShadingModel != currentShadingModel) {
            material->SetShadingModel( newShadingModel );
        }

        ImGui::SeparatorText( "Properties" );

        switch (newShadingModel) {
            case ShadingModel::eDefaultPbr:
                DisplayTextureEditTreeNode( "Base Color", *material, EditBaseColorProperties );
                DisplayTextureEditTreeNode( "Metal-ness", *material, EditMetallicProperties );
                DisplayTextureEditTreeNode( "Roughness", *material, EditRoughnessProperties );
                DisplayTextureEditTreeNode( "Ambient Occlusion", *material, EditAmbientOcclusionProperties );
                DisplayTextureEditTreeNode( "Normals", *material, EditNormalsProperties );
                DisplayTextureEditTreeNode( "Emission", *material, EditEmissionProperties );
                break;
            case ShadingModel::eClearCoat:
                break;
            case ShadingModel::eToonShading:
                break;
            case ShadingModel::eFlatShading:
                break;
            case ShadingModel::eSubsurfaceScattering:
                break;
            default:;
        }
    }

    static auto DrawComponentButton( Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        ImGui::SameLine();
        ImGui::PushItemWidth( -1.0f );

        ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::Button( "Add component" ) ) {
            ImGui::OpenPopup( "AddComponentButtonPopup" );
        }

        if ( ImGui::BeginPopup( "AddComponentButtonPopup" ) ) {
            constexpr bool menuItemSelected{ false };
            const char* menuItemShortcut{ nullptr };

            if ( ImGui::MenuItem( "Physical Material", menuItemShortcut, menuItemSelected, !IsPresent<MaterialComponent>( entity ) ) ) {
                entity->AddComponent<MaterialComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Post-Process Material", menuItemShortcut, menuItemSelected, !IsPresent<PostProcessMaterialComponent>( entity ) ) ) {
                entity->AddComponent<PostProcessMaterialComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Skybox Material", menuItemShortcut, menuItemSelected, !IsPresent<SkyboxMaterialComponent>( entity ) ) ) {
                entity->AddComponent<SkyboxMaterialComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Script", menuItemShortcut, menuItemSelected, !IsPresent<ScriptComponent>( entity ) ) ) {
                entity->AddComponent<ScriptComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Mesh", menuItemShortcut, menuItemSelected, !IsPresent<MeshComponent>( entity ) ) ) {
                entity->AddComponent<MeshComponent>();

                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Camera", menuItemShortcut, menuItemSelected, !IsPresent<CameraComponent>( entity ) ) ) {
                entity->AddComponent<CameraComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Lighting", menuItemShortcut, menuItemSelected, !IsPresent<LightComponent>( entity ) ) ) {
                entity->AddComponent<LightComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Rigid body", menuItemShortcut, menuItemSelected, !IsPresent<RigidBodyComponent>( entity ) ) ) {
                entity->AddComponent<RigidBodyComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Audio", menuItemShortcut, menuItemSelected, !IsPresent<AudioSourceComponent>( entity ) ) ) {
                entity->AddComponent<AudioSourceComponent>("");
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Audio listener", menuItemShortcut, menuItemSelected, !IsPresent<AudioListenerComponent>( entity ) ) ) {
                entity->AddComponent<AudioListenerComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Text", menuItemShortcut, menuItemSelected, !IsPresent<TextComponent>( entity ) ) ) {
                TextComponent& textComponent{ entity->AddComponent<TextComponent>() };

                textComponent.SetSize( 12 );
                textComponent.SetContents( "Example" );
                textComponent.SetSpacing( 1 );

                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Skinned Mesh Renderer", menuItemShortcut, menuItemSelected, !IsPresent<SkinnedMeshRenderer>( entity ) ) ) {
                entity->AddComponent<SkinnedMeshRenderer>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Animator", menuItemShortcut, menuItemSelected, !IsPresent<AnimatorComponent>( entity ) ) ) {
                entity->AddComponent<AnimatorComponent>();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopItemWidth();
    }

    static auto DisplayMapInformation( TextureHandle texture, const eastl::string_view mapName ) -> void {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted( string::Format( "{} ", ICON_MD_PANORAMA ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( mapName.data() );

        ImGui::Spacing();

        ( void )imgui::PushImageButton( ( u64 )texture.GetRaw(), ImGuiService::Get()->GetTextureID( texture ), ImVec2{ 64, 64 } );

        ImGui::SameLine();

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp };
        if ( ImGui::BeginTable( "MaterialEditorDiffusePropertiesTable", 2, tableFlags ) ) {
            // First row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Dimensions" );

            // First row - second colum
            ImGui::TableSetColumnIndex( 1 );
            u32 width{ static_cast<u32>( texture->GetWidth() ) };
            u32 height{ static_cast<u32>( texture->GetHeight() ) };
            ImGui::TextUnformatted( fmt::format( "[{}, {}]", width, height ).c_str() );

            // Second row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Format" );

            // Second row - second colum
            ImGui::TableSetColumnIndex( 1 );
            const FormatInfo& formatInfo{ rhi::GetFormatInfo( texture->GetFormat() ) };
            ImGui::TextUnformatted(formatInfo.mName );

            // Third row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Size (memory)" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            f32 sizeMB{ (f32)((texture->GetWidth() * texture->GetHeight()) * formatInfo.mBytesPerBlock) };
            ImGui::TextUnformatted( string::Format( "{} MB", math::Round( sizeMB / 1'000'000, 2 ) ).c_str() );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Antialiasing" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            auto multisamplingToString = [](Multisampling value) -> eastl::string_view {
                switch (value) {
                    case Multisampling::eMsaaX1:  return "MsaaX1";
                    case Multisampling::eMsaaX2:  return "MsaaX2";
                    case Multisampling::eMsaaX4:  return "MsaaX4";
                    case Multisampling::eMsaaX8:  return "MsaaX8";
                    case Multisampling::eMsaaX16: return "MsaaX16";
                    default:                      return "Unknown";
                }
            };
            ImGui::TextUnformatted( multisamplingToString( texture->GetSampleCount() ).data() );

            ImGui::EndTable();
        }
    }

    static auto ShowGameObjectMaterialInfo( const MeshNode& meshTarget ) -> void {
        ImGui::Spacing();
        ImGui::TextUnformatted( "Mesh Info" );
        ImGui::SameLine();

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::Spacing();
        ImGui::Spacing();

        for ( auto& texture: meshTarget.GetProperties().mTexturesByUri | std::ranges::views::values ) {
            DisplayMapInformation( texture.mTexture, texture.mTexture->GetDebugName() );
        }
    }

    static auto DrawVec3Transform( const std::string_view label, glm::vec3& data, const double resetValue = 0.0, const double columWidth = 100.0, bool uniform = false ) -> void {
        // This Group is part of a unique label
        const eastl::string labelId{ string::Format( "{}:{}", MKT_STRINGIFY( DrawVec3Transform ), label.data() ) };

        ImGui::PushID( labelId.data() );

        ImGui::Columns( 2 );
        ImGui::SetColumnWidth( 0, static_cast<float>( columWidth ) );
        ImGui::Text( "%s", label.data() );
        ImGui::NextColumn();
        ImGui::PushMultiItemsWidths( 3, ImGui::CalcItemWidth() );

        imgui::ImGuiScopedStyleVar frameBorderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 7.0f, 5.0f } };

        const float lineHeight{ GImGui->FontSize + GImGui->Style.FramePadding.y * 3.0f };
        const ImVec2 buttonSize{ lineHeight + 3.0f, lineHeight };

        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.1f, 1.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.1f, 1.0f } );

        if ( ImGui::Button( "X", buttonSize ) ) {
            data.x = static_cast<float>( resetValue );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();
        ImGui::DragFloat( "##X", &data.x, 0.1f, 0.0f, 0.0f, "%.2f" );

        ImGui::PopItemWidth();
        ImGui::PopStyleColor( 3 );
        ImGui::SameLine();

        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.1f, 1.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.25f, 1.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.1f, 1.0f } );

        if ( ImGui::Button( "Y", buttonSize ) ) {
            data.y = static_cast<float>( resetValue );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();
        ImGui::DragFloat( "##Y", &data.y, 0.1f, 0.0f, 0.0f, "%.2f" );

        ImGui::PopItemWidth();
        ImGui::PopStyleColor( 3 );
        ImGui::SameLine();

        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4{ 0.25f, 0.3f, 0.9f, 1.0f } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f } );

        if ( ImGui::Button( "Z", buttonSize ) ) {
            data.z = static_cast<float>( resetValue );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();
        ImGui::DragFloat( "##Z", &data.z, 0.1f, 0.0f, 0.0f, "%.2f" );

        ImGui::PopStyleColor( 3 );
        ImGui::PopItemWidth();


        ImGui::Columns( 1 );

        ImGui::PopID();
    }

    static auto DrawVisibilityCheckBox( Entity* entity ) -> void {
        if ( entity == nullptr ) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity->GetComponent<TagComponent>() };

        bool isActive{ tag.IsActive() };
        if ( imgui::CheckBox( "##DrawVisibilityCheckBox::Checkbox", isActive ) ) {
            tag.SetActive( isActive );
        }
    }

    static auto DrawNameTextInput( Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !entity ) {
            return;
        }

        TagComponent& tag{ entity->GetComponent<TagComponent>() };

        constexpr u32 kInputNameLength{ 512 };
        constexpr ImGuiTextFlags flags{ ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll };

        eastl::array<char, kInputNameLength> name{};
        eastl::copy( tag.GetTag().begin(), tag.GetTag().end(), name.begin() );
        if ( ImGui::InputText( "##DrawNameTextInputTag", name.data(), kInputNameLength, flags ) ) {
            tag.SetTag( name.data() );
        }
    }

    static auto DrawPointLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "PointLightMainTable", 2, tableFlags ) ) {
            auto& pointLightData{ lightComponent.Get<PointLight>() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            glm::vec3 diffuseComponent{ pointLightData.GetColor() };
            if ( imgui::ColorEdit3( "Color", diffuseComponent ) ) {
                pointLightData.SetColor( diffuseComponent );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float intensity{ pointLightData.GetIntensity() };
            if ( imgui::Slider( "Intensity", intensity, { 1.0f, 1000.0f } ) ) {
                pointLightData.SetIntensity( intensity );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{ pointLightData.GetRadius() };
            if ( imgui::Slider( "Radius", radius, { 1.0f, 500.0f } ) ) {
                pointLightData.SetRadius( radius );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            static bool castShadows{};
            if ( imgui::CheckBox( "Cast shadows", castShadows ) ) {
            }

            ImGui::EndTable();
        }
    }

    static auto DrawSpotLightLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "SpotLightEditTable", 2, tableFlags ) ) {
            auto& spotLightData{ lightComponent.Get<SpotLight>() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            glm::vec3 direction{ spotLightData.GetDirection() };
            if ( DragFloat3( "Direction", "%.2f", direction, 0.01f, -math::constants::kPi, math::constants::kPi ) ) {
                spotLightData.SetDirection( direction );
            }

            ImGui::SameLine();
            widget::MakeHelpPopUp( "The spot position is determined by the objects position." );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float3 color{ spotLightData.GetColor() };
            if ( imgui::ColorEdit3( "Color", color ) ) {
                spotLightData.SetColor( color );
            }
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float intensity{ spotLightData.GetIntensity() };
            if ( imgui::Slider( "Intensity", intensity, { 1.0f, 1000.0f } ) ) {
                spotLightData.SetIntensity( intensity );
            }
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{ spotLightData.GetRadius() };
            if ( imgui::Slider( "Attenuation Radius", radius, { 1.0f, 500.0f } ) ) {
                spotLightData.SetRadius( radius );
            }
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float angle{ spotLightData.GetAngle() };
            if ( imgui::Slider( "Angle", angle, { 1.0f, SpotLight::GetMaxAngle() } ) ) {
                spotLightData.SetAngle( angle );
            }
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::SameLine();
            widget::MakeHelpPopUp( "Cone angle in degrees" );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float softness{ spotLightData.GetSoftness() };
            if ( imgui::Slider( "Softness", softness, { 0.0f, SpotLight::GetMaxSoftness() } ) ) {
                spotLightData.SetSoftness( softness );
            }

            ImGui::SameLine();
            widget::MakeHelpPopUp( "Edge softness of the spotlight" );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            static bool castShadows{};
            CheckBox( "Cast shadows", castShadows );

            ImGui::EndTable();
        }
    }

    static auto DrawDirectionalLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "DirectionalLightEditTable", 2, tableFlags ) ) {

            auto& direLightData{ lightComponent.Get<DirectionalLight>() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Intensity" );

            ImGui::TableSetColumnIndex( 1 );
            float intensity{ direLightData.GetIntensity() };
            if ( imgui::Slider( "##Intensity", intensity, { 1.0f, 10.0f } ) ) {
                direLightData.SetIntensity( intensity );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Color" );

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 diffuse{ direLightData.GetColor(), 1.0f };
            if ( imgui::ColorEdit4( "##DirectionalLightDiffuse", diffuse ) ) {
                direLightData.SetColor( diffuse );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Direction" );

            ImGui::SameLine();

            widget::MakeHelpPopUp(
                    "In the case of the fourth component having a value of 1.0f\n"
                    "we do light calculations using the light's position instead\n"
                    "which is the position of the game object." );

            ImGui::TableSetColumnIndex( 1 );

            constexpr float PI{ std::numbers::pi_v<float> };
            glm::vec3 direction{ direLightData.GetDirection() };

            if ( DragFloat3( "##DirectionalLightDirection", "%.2f", direction, 0.01f, -PI, PI ) ) {
                direLightData.SetDirection( direction );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Cast shadows" );

            ImGui::TableSetColumnIndex( 1 );
            bool castShadows{ direLightData.IsShadowCaster() };
            if (CheckBox( "##DirectionalLightShadows", castShadows )) {
                direLightData.SetIsShadowCaster( castShadows );
            }

            ImGui::EndTable();
        }
    }

    auto InspectorPanel::DrawTransformComponentTab( Entity& entity ) -> void {
        TransformComponent& transformComponent{ entity.GetComponent<TransformComponent>() };

        glm::vec3 newTranslation{ transformComponent.GetTranslation() };
        glm::vec3 newRotation{ transformComponent.GetRotation() };
        glm::vec3 newScale{ transformComponent.GetScale() };

        ImGui::Spacing();

        DrawVec3Transform( "Translation", newTranslation );
        DrawVec3Transform( "Rotation", newRotation );

        bool uniformScale{ entity.GetComponent<TransformComponent>().HasUniformScale() };
        DrawVec3Transform( "Scale", newScale, 1.0, 100.0, uniformScale );

        ImGui::Spacing();

        // Make separator take up whole window
        // just like separator for components
        ImGui::Unindent();
        ImGui::Separator();
        ImGui::Indent();

        ImGui::Spacing();

        if ( imgui::CheckBox( "##SetupTransformComponentTab:UniformScale", uniformScale ) ) {
            entity.GetComponent<TransformComponent>().SetUniformSale( uniformScale );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();
        ImGui::TextUnformatted( "Enable uniform scaling" );

        transformComponent.SetTranslation( newTranslation );
        transformComponent.SetRotation( newRotation );
        transformComponent.SetScale( newScale );
    }

    auto InspectorPanel::DrawScriptingComponentTab( Entity& entity ) -> void {
        ScriptComponent& scriptComponent{ entity.GetComponent<ScriptComponent>() };

        // Static so ImGui input buffer persists
        static eastl::string formattedPath{};
        FileHandle file{ FileService::Get()->LoadFile( scriptComponent.GetFilePath() ) };

        if ( !file.IsEmpty() ) {
            formattedPath = string::Format( "{}", file->GetPath().GetC_Str() );
        }

        ImGui::InputText( "##PathToScript", formattedPath.data(), formattedPath.size() + 1, ImGuiInputTextFlags_ReadOnly );
        ImGui::SameLine();

        if ( ImGui::Button( fmt::format( " {} Load ", ICON_MD_SEARCH ).c_str() ) ) {
            const std::initializer_list<FileDialogPair> filters{
                { "LUA Files", "lua" }
            };

            Path path{ filesystem::OpenFileDialog( filters ) };
            if ( !path.IsEmpty() ) {
                scriptComponent.SetScript( ScriptingService::Get()->LoadScript( path, std::addressof( entity ) ) );
            }
        }

        if ( ImGui::IsItemHovered() )
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

        if ( !file.IsEmpty() ) {
            ImGui::Spacing();
            ImGui::SeparatorText( "Script Preview" );

            const eastl::string& contents{ file->GetContentsString() };

            // Cap preview for performance
            constexpr usize kMaxPreviewSize{ 8192 * 4 };// more generous limit
            const bool truncated = contents.size() > kMaxPreviewSize;
            const std::string_view preview{
                contents.data(),
                truncated ? kMaxPreviewSize : contents.size()
            };

            // Split into lines once (to allow fast ImGuiListClipper iteration)
            static std::vector<std::string_view> lines{};
            lines.clear();
            {
                const char* start{ preview.data() };
                const char* end{ preview.data() + preview.size() };
                while ( start < end ) {
                    const char* lineEnd = std::find( start, end, '\n' );
                    lines.emplace_back( start, static_cast<size_t>( lineEnd - start ) );
                    start = ( lineEnd == end ) ? end : lineEnd + 1;
                }
            }

            // Style
            ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4{ 0.1f, 0.1f, 0.1f, 0.5f } );
            ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 6.0f );
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 6.0f, 6.0f } );

            const float height{ ImGui::GetTextLineHeightWithSpacing() * 15.0f };
            if ( ImGui::BeginChild( "ScriptPreviewChild", ImVec2{ 0, height }, true, ImGuiWindowFlags_HorizontalScrollbar ) ) {
                imgui::ImGuiScopedTextFont newFont{ ImGuiService::Get()->PushFont( "./Resources/Fonts/Google_Sans_Code/static/GoogleSansCode-Light.ttf" ) };

                ImGuiListClipper clipper{};
                clipper.Begin( static_cast<i32>( lines.size() ) );
                while ( clipper.Step() ) {
                    for ( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i ) {
                        ImGui::TextUnformatted( lines[i].data(), lines[i].data() + lines[i].size() );
                    }
                }
                clipper.End();

                if ( truncated ) {
                    ImGui::Separator();
                    ImGui::TextColored( ImVec4{ 1.0f, 0.7f, 0.2f, 1.0f }, "[Preview truncated, file too large]" );
                }
            }
            ImGui::EndChild();

            ImGui::PopStyleVar( 2 );
            ImGui::PopStyleColor();

            ImGui::Spacing();
            if ( ImGui::Button( fmt::format( "{} Open in External Editor", ICON_MD_OPEN_IN_NEW ).c_str() ) ) {
                RuntimeConsole::Get()->ExecuteCommand( string::Concat( "/", " code ", file->GetPath().GetC_Str() ).c_str() );
            }
        }
    }

    auto InspectorPanel::DrawAnimatorComponentTab( Entity& entity ) -> void {
        AnimatorComponent& animatorComponent{ entity.GetComponent<AnimatorComponent>() };
        Animator* animator{
            AnimationSystem::Get()->GetAnimator( animatorComponent.GetAnimatorID() )
        };

        eastl::string currentAnimationName{};
        if ( animator ) {
            if ( const SkinnedAnimation* current{ animator->GetCurrentAnimation() } )
                currentAnimationName = current->GetName();
        }

        imgui::UnindentScoped und{};

        imgui::DrawNode( "Animation List", [animator, &currentAnimationName]() -> void {
            if ( !animator )
                return;

            const auto& animationList{ animator->GetAnimationList() };

            if ( animationList.empty() )
                return;

            eastl::vector<eastl::string> animationNames{};
            animationNames.reserve( animationList.size() );

            for ( const auto& [name, animation]: animationList )
                animationNames.push_back( animation->GetName() );

            const i32 selectionIndex{
                imgui::Combo(
                        animationNames.data(),
                        static_cast<size_t>( animationNames.size() ),
                        currentAnimationName )
            };

            if ( selectionIndex != -1 ) {
                currentAnimationName = animationNames[selectionIndex];
                animator->SetCurrentAnimation( currentAnimationName );
            }
        } );

        bool play{ false };

        if ( animator )
            play = animator->IsPlaying();

        if ( imgui::CheckBox( "Play selected animation", play ) ) {
            if ( !animator )
                return;

            if ( play )
                animator->PlayAnimation( currentAnimationName );
            else
                animator->StopCurrentAnimation();
        }
    }

    auto InspectorPanel::DrawSkinMeshComponentTab( Entity& entity ) -> void {
        SkinnedMeshRenderer& skinnedMeshRendererComp{ entity.GetComponent<SkinnedMeshRenderer>() };
    }

    auto InspectorPanel::DrawMaterialComponentTab( Entity& entity ) -> void {
        // ImGui by default will indent because the items in this function because this method is run inside the DrawComponent function,
        // so we are within a Tree Node, items within a tree node appear indented by default when you expand it
        ImGui::Unindent();

        MaterialComponent& materialComponent{ entity.GetComponent<MaterialComponent>() };
        if ( materialComponent.HasMaterial() ) {
            EditMaterial( checked_cast<PhysicalMaterial*>( materialComponent.GetMaterial().GetRaw() ), materialComponent.IsInternal() );
        } else {
            ( void )InputText( "Mesh has no material", true );

            ImGui::SameLine();

            eastl::string createMaterialID{ string::Format("{} Create##CreateMaterialID", ICON_MD_CREATE ) };
            if (ImGui::Button( createMaterialID.c_str() )) {
                materialComponent.SetIsInternal( false );
                materialComponent.SetMaterial( AssetsService::Get()->CreateMaterial( PhysicMaterialDescription{} ) );
            }
            SetCursorHandOnLastItemHovered();

            ImGui::SameLine();

            eastl::string loadMaterialID{ string::Format("{} Load##LoadMaterialID", ICON_MD_SAVE_ALT ) };
            if (ImGui::Button( loadMaterialID.c_str() )) {
                const std::initializer_list<FileDialogPair> filters{
                    { "Material", filesystem::kMikotoMaterialExtension.data() } };
                Path materialPath{ OpenFileDialog( filters ) };

                materialComponent.SetIsInternal( false );
                materialComponent.SetMaterial( AssetsService::Get()->DeSerializeMaterial( materialPath ) );
            }
            SetCursorHandOnLastItemHovered();
        }

        ImGui::Indent();
    }

    auto InspectorPanel::DrawPhysicsComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<RigidBodyComponent>()) {
            return;
        }

        auto& rb{ entity.GetComponent<RigidBodyComponent>() };

        if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen)) {

            // --- Body Type ---
            eastl::array bodyTypes{ "Static", "Kinematic", "Dynamic" };
            i32 currentType{ as<i32>( rb.GetBodyType() ) };

            ImGui::Text("Body Type");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
            if ( ImGui::Combo( "##BodyType", &currentType, bodyTypes.data(), bodyTypes.size()) ) {
                rb.SetBodyType(static_cast<RigidBodyComponent::BodyType>(currentType));
            }

            // --- Use Gravity ---
            bool useGravity{ rb.UseGravity() };
            if (ImGui::Checkbox("Use Gravity", &useGravity)) {
                rb.SetUseGravity(useGravity);
            }

            // --- Mass ---
            float mass{ rb.GetMass() };
            ImGui::Text("Mass");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
            if (ImGui::DragFloat("##Mass", &mass, 0.1f, 0.0f, 1000.0f, "%.2f")) {
                rb.SetMass(mass);
            }

            // --- Friction ---
            float friction{ rb.GetFriction() };
            ImGui::Text("Friction");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
            if (ImGui::SliderFloat("##Friction", &friction, 0.0f, 1.0f, "%.2f")) {
                rb.SetFriction(friction);
            }

            // --- Internal Handle Info (read-only) ---
            if (rb.IsValidBodyID()) {
                const auto handle { rb.GetBodyID() };

                ImGui::Separator();
                ImGui::TextDisabled("Internal Handle:");
                ImGui::SameLine();
                ImGui::Text("Body ID: %d", handle);
            } else {
                ImGui::TextDisabled("Internal Handle:");
                ImGui::SameLine();
                ImGui::Text("No BodyID");
            }

            // --- Linear Velocity ---
            float3 linearVel{ rb.GetLinearVelocity() };
            ImGui::Text("Linear Velocity");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
            if (ImGui::DragFloat3("##LinearVelocity", &linearVel.x, 0.1f)) {
                rb.SetLinearVelocity(linearVel);
            }

            // --- Angular Velocity ---
            float3 angularVel{ rb.GetAngularVelocity() };
            ImGui::Text("Angular Velocity");
            ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
            if (ImGui::DragFloat3("##AngularVelocity", &angularVel.x, 0.1f)) {
                rb.SetAngularVelocity(angularVel);
            }

            ImGui::Spacing();
            ImGui::Separator();
        }
    }

    auto InspectorPanel::DrawRenderComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<MeshComponent>()) {
            return;
        }

        ImGui::Unindent();
        ImGui::Spacing();

        ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
        ImGui::Button( fmt::format( " {} Source ", ICON_MD_ARCHIVE ).c_str() );
        ImGui::PopItemFlag();

        ImGui::SameLine();

        Path path{ "" };
        MeshComponent& component{ entity.GetComponent<MeshComponent>() };

        const MeshNode* mesh{ component.GetMesh() };
        if ( mesh != nullptr ) {
            path = component.GetModel()->GetPath();
        }

        // Imgui Will need this later, so the buffer must still exist
        // can't be made a with automatic storage duration
        eastl::string formatedPath{ string::Format( "{}", path.GetC_Str() ) };

        // See imgui assert on the size of the buffer
        // formatedPath.size() already includes the terminator
        ImGui::InputText( "##DrawRenderComponentTab_PathToModel", formatedPath.data(),
            formatedPath.size() + 1, ImGuiInputTextFlags_ReadOnly );

        ImGui::SameLine();

        if ( ImGui::Button( string::Format( " {} Load ##DrawRenderComponentTab_SearchButton", ICON_MD_SEARCH ).c_str() ) ) {
            threading::TaskService::Get()->Submit( [rootEntity = std::addressof(entity), this]() -> void {
                const std::initializer_list<FileDialogPair> filters{
                    { "Model files", "obj,gltf,fbx,glb" },
                    { "OBJ files", "obj" },
                    { "glTF files", "gltf" },
                    { "FBX files", "fbx" },
                    { "GLB files", "glb" } };
                Path modelPath{ OpenFileDialog( filters ) };

                if ( !modelPath.IsEmpty() ) {
                    ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( modelPath ) };

                    const EntityCreateInfo entityCreateInfo{
                        .mRoot = rootEntity,
                        .mName{ model->GetName() },
                        .mModel = model };
                    mState->mActiveScene->PushEntity( entityCreateInfo );
                }
            });
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( mesh != nullptr ) {
            ShowGameObjectMaterialInfo( *mesh );
        }

        ImGui::Indent();
    }

    auto InspectorPanel::DrawLightComponentTab( Entity& entity ) -> void {
        LightComponent& lightComponent{ entity.GetComponent<LightComponent>() };

        static constexpr std::array<std::string_view, 3> lightTypes{ "Directional light", "Point light", "Spot light" };

        const LightType lightType{ lightComponent.GetActiveType() };

        ImGui::TextUnformatted( "Light type " );
        ImGui::SameLine();

        if ( ImGui::BeginCombo( "##LightType", lightTypes[static_cast<size_t>( lightType )].data() ) ) {
            size_t lightTypeIndex{};
            for ( const auto& currentType: lightTypes ) {
                // Tells whether we want to highlight this light type in the ImGui combo.
                // This will be the case if the current type of light is the same as the component
                const bool isSelected{ currentType == lightTypes[static_cast<size_t>( lightType )] };

                // This cast is valid because lightTypeIndex is always in the range [0, 2]
                // where each index indicates a type of light, see LightType definition.
                const LightType selectedType{ static_cast<LightType>( lightTypeIndex ) };

                // Create a selectable combo item for each light type
                if ( ImGui::Selectable( currentType.data(), isSelected ) ) {
                    // Update the type of light for this component
                    lightComponent.SetActiveType( selectedType );
                }

                // Light combo item is hovered
                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                if ( isSelected ) {
                    ImGui::SetItemDefaultFocus();
                }

                ++lightTypeIndex;
            }

            ImGui::EndCombo();
        }

        // Combo is hovered
        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        switch ( lightComponent.GetActiveType() ) {
            case LightType::eDirectional:
                DrawDirectionalLightOptions( lightComponent );
                break;

            case LightType::ePoint:
                DrawPointLightOptions( lightComponent );
                break;

            case LightType::eSpot:
                DrawSpotLightLightOptions( lightComponent );
                break;
            default:;
        }
    }

    auto InspectorPanel::DrawTextComponentTab( Entity& entity ) -> void {
        TextComponent& textComponent{ entity.GetComponent<TextComponent>() };

        glm::vec4 color{ textComponent.GetColor() };
        constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

        if ( ImGui::ColorEdit4( "Color", glm::value_ptr( color ), colorEditFlags ) ) {
            textComponent.SetColor( color );
        }

        widget::MakeHelpPopUpDelay( "Select the current font.", "" );

        eastl::string fontPath{ "Select font" };

        if (textComponent.HasFont()) {
            fontPath = textComponent.GetFont()->GetName();
        }

        ImGui::InputText( "##FontPath", fontPath.data(), fontPath.size() + 1, ImGuiInputTextFlags_ReadOnly );
        ImGui::SameLine();
        if ( ImGui::Button( "Load Font" ) ) {
            threading::TaskService::Get()->Submit( [&]() -> void {
                const std::initializer_list<FileDialogPair> filters{
                    { "Font Files", "ttf" }
                };

                Path path{ filesystem::OpenFileDialog( filters ) };
                if ( !path.IsEmpty() ) {
                    FontHandle newFont{ AssetsService::Get()->LoadAsset<Font>( path ) };

                    if ( !newFont.IsEmpty() ) {
                        textComponent.SetFont( newFont );
                    }
                }
            } );
        }

        ImGui::Spacing();
        static eastl::array textAlignment{ "Center", "Left", "Right" };

        static eastl::string currentAlignment{ textAlignment[0] };
        ComboList( textAlignment.begin(), textAlignment.end(), currentAlignment, [&]( const eastl::string_view target ) -> bool {
            return string::Equal( currentAlignment, target );
        }, "SetupTextComponentTab:Alignment" );

        if (!ImGui::IsItemActive()) {
            widget::MakeHelpPopUpDelay( "Text alignment.", "" );
        }

        // Slider float font size
        float currentSize{ textComponent.GetSize() };
        ImGui::Spacing();
        if ( imgui::Slider( "##WorldSize", currentSize, { TextComponent::GetMinLetterSize(), 500.0f } ) ) {
            textComponent.SetSize( currentSize );
        }
        if (!ImGui::IsItemActive()) {
            widget::MakeHelpPopUpDelay( "Text size in world space.", "" );
        }

        // Slider float letter spacing
        float spacing{ textComponent.GetSpacing() };
        ImGui::Spacing();
        if ( imgui::Slider( "##Spacing", spacing, { TextComponent::GetMinLetterSpacing(), 35.0f } ) ) {
            textComponent.SetSpacing( spacing );
        }

        if (!ImGui::IsItemActive()) {
            widget::MakeHelpPopUpDelay( "Text inner spacing.", "" );
        }


        // Slider float letter spacing
        bool isWorldText{ textComponent.IsWorldText() };
        ImGui::Spacing();
        if ( imgui::CheckBox( "Is World Text", isWorldText) ) {
            textComponent.SetIsWorldText( isWorldText );
        }

        ImGui::Spacing();

        eastl::string content{ textComponent.GetContents() };
        if ( imgui::TextArea( content ) ) {
            textComponent.SetContents( content );
        }

        // Optionally show atlas info
        if ( ImGui::CollapsingHeader( "Font Atlas Info", ImGuiTreeNodeFlags_DefaultOpen ) ) {
            const Font* font{ textComponent.GetFont() };
            if ( font != nullptr ) {
                TextureHandle atlas{ font->GetAtlas() };

                if ( imgui::PushImageButton( (u64)atlas, ImGuiService::Get()->GetTextureID( atlas ), ImVec2{ 256, 256 } ) ) {

                }

                imgui::ToolTip( [&]() -> void {
                    ShowTextureHoverTooltip( atlas.GetRaw() );
                },  ImGui::IsItemHovered() );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TextUnformatted( fmt::format( "Atlas Size: {} x {}", atlas->GetWidth(), atlas->GetHeight() ).c_str() );
                ImGui::TextUnformatted( fmt::format( "Number of Glyphs: {}", font->GetGlyphCount() ).c_str() );
            }
        }
    }

    auto InspectorPanel::DrawAudioListenerComponentTab( Entity& entity ) -> void {
        AudioListenerComponent& alc{ entity.GetComponent<AudioListenerComponent>() };

        if ( !alc.IsActive() ) {
            ImGui::TextDisabled( "Audio Listener is not active." );
            return;
        }

        AudioListener& listener{ alc.GetListener() };

        ImGui::SeparatorText( "Audio Listener" );

        //
        // Forward Vector
        //
        {
            float3 forward{ listener.GetForward() };

            ImGui::Text( "Forward" );
            ImGui::SameLine();
            if ( ImGui::DragFloat3( "##ForwardVec", glm::value_ptr( forward ), 0.05f ) ) {
                listener.SetForward( forward );
            }
        }

        //
        // Up Vector
        //
        {
            float3 up{ listener.GetUp() };

            ImGui::Text( "Up" );
            ImGui::SameLine();
            if ( ImGui::DragFloat3( "##UpVec", glm::value_ptr( up ), 0.05f ) ) {
                listener.SetUp( up );
            }
        }

        //
        // Velocity Vector
        //
        {
            float3 vel{ listener.GetVelocity() };

            ImGui::Text( "Velocity" );
            ImGui::SameLine();
            if ( ImGui::DragFloat3( "##VelVec", glm::value_ptr( vel ), 0.05f ) ) {
                listener.SetVelocity( vel );
            }
        }

        ImGui::Separator();

        //
        // Reset Button
        //
        if ( ImGui::Button( "Reset Listener Orientation" ) ) {
            listener.SetForward( float3{ 0.0f, 0.0f, -1.0f } );
            listener.SetUp( float3{ 0.0f, 1.0f, 0.0f } );
            listener.SetVelocity( float3{ 0.0f, 0.0f, 0.0f } );
        }
    }

    auto InspectorPanel::DrawAudioComponentTab( Entity& entity ) -> void {
        AudioSourceComponent& audioComponent{ entity.GetComponent<AudioSourceComponent>() };

        AudioSourceHandle source{ audioComponent.GetSource() };
        AudioHandle clip{ audioComponent.GetClip() };

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp };
        if ( !ImGui::BeginTable( "AudioComponentTable", 2, tableFlags ) ) {
            return;
        }

        // --- Audio clip path ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Audio Clip" );

        ImGui::TableSetColumnIndex( 1 );
        static eastl::string clipPath{};
        clipPath = !clip.IsEmpty() ? clip->GetTrackName() : "";
        ImGui::InputText( "##AudioClipPath", clipPath.data(), clipPath.size() + 1, ImGuiInputTextFlags_ReadOnly );
        ImGui::SameLine();
        if ( ImGui::Button( "Load Clip" ) ) {
            static bool loading{ false };
            if ( !loading ) {
                loading = true;

                threading::TaskService::Get()->Submit( [&]() -> void {
                    const std::initializer_list<FileDialogPair> filters{
                        { "Audio Files", "wav,mp3,ogg" }
                    };

                    Path path{ filesystem::OpenFileDialog( filters ) };
                    if ( !path.IsEmpty() ) {
                        AudioHandle newClip{ AssetsService::Get()->LoadAsset<Audio>( AudioLoadDescription {
                            .mFile{ FileService::Get()->LoadFile( path ) }
                        } ) };

                        if ( !newClip.IsEmpty() ) {
                            audioComponent.SetClip( newClip );
                        }
                    }

                    loading = false;
                } );
            }
        }

        // --- Spatialize ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Spatizlized" );
        ImGui::TableSetColumnIndex( 1 );
        bool isSpatialized{ source ? source->IsSpatialized() : false };
        if ( ImGui::Checkbox( "##IsSpatizlizedAudio", MKT_ADDRESSOF( isSpatialized ) ) ) {
            if ( source ) source->SetSpatialization( isSpatialized );
        }
        imgui::SetCursorHandOnLastItemHovered();

        // --- Muted ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Muted" );
        ImGui::TableSetColumnIndex( 1 );
        bool isMuted{ source ? source->IsMuted() : false };
        if ( ImGui::Checkbox( "##IsMutedAudio", &isMuted ) ) {
            if ( source ) source->Mute( isMuted );
        }
        imgui::SetCursorHandOnLastItemHovered();

        // --- Loop ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Loop" );
        ImGui::TableSetColumnIndex( 1 );
        bool isLooping = source ? source->IsLooping() : false;
        if ( ImGui::Checkbox( "##IsLoopingAudio", &isLooping ) ) {
            if ( source ) source->SetLooping( isLooping );
        }
        imgui::SetCursorHandOnLastItemHovered();

        // --- Volume ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Volume" );
        ImGui::TableSetColumnIndex( 1 );
        float volume{ source ? source->IsMuted() ? 0.0f : source->GetVolume() : 1.0f };
        if ( ImGui::SliderFloat( "##VolumeAudio", &volume, 0.0f, AudioSource::GetMaxVolume() ) ) {
            if ( source ) {
                source->SetVolume( volume );
            }
        }
        imgui::SetCursorHandOnLastItemHovered();

        // --- Playback controls ---
        if (!source.IsEmpty()) {

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Playback" );
            ImGui::TableSetColumnIndex( 1 );

            if ( ImGui::Button( string::Concat( ICON_MD_PLAY_ARROW, " Play" ).c_str() ) ) {
                source->Play();
            }
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::SameLine();
            if ( ImGui::Button( string::Concat( ICON_MD_PAUSE, " Pause" ).c_str() ) ) {
                source->Pause();
            }
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::SameLine();
            if ( ImGui::Button( string::Concat( ICON_MD_STOP, " Stop" ).c_str() ) ) {
                source->Stop();
            }
            imgui::SetCursorHandOnLastItemHovered();

            // --- Progress bar ---
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Progress" );
            ImGui::TableSetColumnIndex( 1 );

            float duration{ source->GetAudioDuration() };
            float progress{ source->GetCurrentProgress() };
            ImGui::ProgressBar( duration > 0.0f ? progress / duration : 0.0f, ImVec2( -1, 0 ) );
        }

        ImGui::EndTable();

        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                        ImGuiTreeNodeFlags_Framed |
                                                        ImGuiTreeNodeFlags_SpanAvailWidth |
                                                        ImGuiTreeNodeFlags_FramePadding };
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        if (source) {
            if ( ImGui::TreeNodeEx( "##SetupAudioComponentTab3DAudioSettings{}", treeNodeFlags, "3D Audio Settings" ) ) {

            // Doppler
            static float dopplerLevel{ source->GetDopplerFactor() };
            ImGui::TextUnformatted("Doppler Level");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            if ( ImGui::DragFloat(
                "##DopplerLevel",
                std::addressof( dopplerLevel ),
                0.01f,
                0.0f,
                5.0f,
                "%.2f"
            )) {
                source->SetDopplerFactor( dopplerLevel );
            }

            // Spread
            static float spread{};
            ImGui::TextUnformatted("Spread");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::SliderFloat(
                "##Spread",
                std::addressof( spread ),
                0.0f,
                180.0f,
                "%.0f°"
            );

            // Rolloff
            static constexpr const char* rolloffItems[] {
                "Logarithmic",
                "Linear",
                "Custom"
            };


            static int rolloffIndex{};

            ImGui::TextUnformatted("Volume Rolloff");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::Combo("##SetupAudioComponentRolloff", &rolloffIndex, rolloffItems, IM_ARRAYSIZE(rolloffItems))) {
            }

            // Min Distance
            static float minDistance{};
            static float maxDistance{};

            ImGui::TextUnformatted("Min Distance");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat(
                "##MinDistance",
                &minDistance,
                0.1f,
                0.0f,
                maxDistance,
                "%.2f"
            );

            // Max Distance
            ImGui::TextUnformatted("Max Distance");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat(
                "##MaxDistance",
                &maxDistance,
                1.0f,
                minDistance,
                10000.0f,
                "%.1f"
            );

            ImGui::TreePop();
        }
        }
    }

    auto InspectorPanel::DrawCameraComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<CameraComponent>()) {
            return;
        }

        if (!mState->mSceneRenderer) {
            return;
        }

        CameraComponent& cameraComponent{ entity.GetComponent<CameraComponent>() };
        const eastl::array<std::string, 2> kCameraProjectionTypeNames{ "Orthographic", "Perspective" };

        if ( !cameraComponent.HasCamera() ) {
            if ( !ButtonTextIcon( string::Concat( ICON_MD_ADD, " Add camera" ).c_str() ) ) {
                return;
            }
        }

        SceneCamera& sceneCamera{ cameraComponent.GetCamera() };
        const auto cameraCurrentProjectionType{ sceneCamera.GetProjectionType() };

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchSame };
        if ( ImGui::BeginTable( "CameraComponentEditTable", 2, tableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Clear flags" );

            ImGui::TableSetColumnIndex( 1 );
            CameraClearFlags currentClearFlags{ cameraComponent.GetClearFlags() };
            eastl::array<std::string, as<usize>(CameraClearFlags::eCount)> choicesAlpha{
                "Clear color", "Blurred Skybox", "Skybox" };

            CameraClearFlags newClearFlags{ imgui::Combo( choicesAlpha, currentClearFlags ) };
            if (newClearFlags != currentClearFlags) {
                auto GetRenderBackground{
                    []( CameraClearFlags rb ) {
                        switch (rb) {
                            case CameraClearFlags::eSkybox: return SceneBackgroundType::eSkybox;
                            case CameraClearFlags::eBlurredSkybox: return SceneBackgroundType::ePrefilterMap;
                            case CameraClearFlags::eClearColor: return SceneBackgroundType::eClearColor;
                            default:;
                        }

                        return SceneBackgroundType::eClearColor;
                    }
                };

                cameraComponent.SetClearFlags( newClearFlags );

                mState->mSceneRenderer->SetGamma( cameraComponent.GetGamma() );
                mState->mSceneRenderer->SetExposure( cameraComponent.GetExposure() );
                mState->mSceneRenderer->SetRenderBackground( GetRenderBackground(newClearFlags) );

                if (newClearFlags == CameraClearFlags::eClearColor) {
                    mState->mSceneRenderer->SetClearColor( cameraComponent.GetClearColor() );
                }
            }

            ImGui::SameLine();

            if (newClearFlags == CameraClearFlags::eClearColor) {
                float4 colorFloat4{ cameraComponent.GetClearColor() };
                constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs };

                if ( ImGui::ColorEdit4( "##CameraClearColor", glm::value_ptr( colorFloat4 ), colorEditFlags ) ) {
                    cameraComponent.SetClearColor( colorFloat4 );
                    mState->mSceneRenderer->SetClearColor( colorFloat4 );
                }
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Exposure" );

            ImGui::TableSetColumnIndex( 1 );
            f32 exposure{ cameraComponent.GetExposure() };
            if (ImGui::SliderFloat( "##CameraExposureSlider", MKT_ADDRESSOF( exposure ), 0.1, 10.0f )) {
                cameraComponent.SetExposure( exposure );
                mState->mSceneRenderer->SetExposure( cameraComponent.GetExposure() );
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Gamma" );

            ImGui::TableSetColumnIndex( 1 );
            f32 gamma{ cameraComponent.GetGamma() };

            if (ImGui::SliderFloat( "##CameraGammaSlider", MKT_ADDRESSOF( gamma ), 0.1, 10.0f )) {
                cameraComponent.SetGamma( gamma );
                mState->mSceneRenderer->SetGamma( cameraComponent.GetGamma() );
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::Spacing();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Projection Type" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );
            const auto& currentProjectionTypeStr{ kCameraProjectionTypeNames[as<u32>( cameraCurrentProjectionType )] };

            if ( ImGui::BeginCombo( "##Projection", currentProjectionTypeStr.c_str() ) ) {
                u32 projectionIndex{};
                for ( const auto& projectionType: kCameraProjectionTypeNames ) {
                    // Tells whether we want to highlight this projection in the ImGui combo.
                    // This will be the case if this projection type is the current one for this camera.
                    bool isSelected{ projectionType == kCameraProjectionTypeNames[as<u32>( cameraCurrentProjectionType )] };

                    // Create a selectable combo item for each perspective
                    if ( ImGui::Selectable( projectionType.c_str(), isSelected ) ) {
                        sceneCamera.SetProjectionType( as<ProjectionType>( projectionIndex ) );
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

        if ( sceneCamera.GetProjectionType() == ProjectionType::PERSPECTIVE ) {

            if ( ImGui::BeginTable( "##PerspectiveProjControl", 2, tableFlags ) ) {

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Field of view" );

                ImGui::TableSetColumnIndex( 1 );
                float fov{ sceneCamera.GetFOV() };
                if ( ImGui::SliderFloat( "##Perspective FOV", std::addressof( fov ), 45.0f, 90.0f ) ) {
                    sceneCamera.SetFieldOfView( fov );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Near plane" );

                ImGui::TableSetColumnIndex( 1 );
                float nearPlane{ sceneCamera.GetNearPlane() };
                if ( ImGui::SliderFloat( "##Perspective Near", std::addressof( nearPlane ), 0.001f, 1.0 ) ) {
                    sceneCamera.SetNearPlane( nearPlane );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Far plane" );

                ImGui::TableSetColumnIndex( 1 );
                float farPlane{ ( sceneCamera.GetFarPlane() ) };
                if ( ImGui::SliderFloat( "##Perspective Far", &farPlane, 100.0f, 10000.0f ) ) {
                    sceneCamera.SetFarPlane( farPlane );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::EndTable();
            }
        }
    }

    auto InspectorPanel::DrawSkyboxComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<SkyboxMaterialComponent>()) {
            return;
        }

        SkyboxMaterialComponent& sbComponent{ entity.GetComponent<SkyboxMaterialComponent>() };
        SkyboxMaterial* material{ checked_cast<SkyboxMaterial*>( sbComponent.GetMaterial().GetRaw() ) };

        if (!material) {
            return;
        }

        ImGui::TextUnformatted( "Material name");
        ImGui::SameLine();
        if (imgui::InputText( material->GetName(), true ) ) {
            // Nothing
        }

        // Select type of skybox texture (equirectangular or cube faces)
        ImGui::Spacing();
        ImGui::TextUnformatted( "Skybox Type");
        SkyboxType currentSkyboxType{ material->GetType() };
        eastl::array<std::string, as<usize>(SkyboxType::eCount)> choicesAlpha{
            "Cube Faces", "HDR Texture" };

        ImGui::SameLine();
        SkyboxType newSkyboxType{ imgui::Combo( choicesAlpha, currentSkyboxType ) };
        if (newSkyboxType != currentSkyboxType) {
            material->SetType( newSkyboxType );
            mState->mSceneRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );
        }

        ImGui::Spacing();
        ImGui::SeparatorText( "Texture (s)");

        constexpr auto columnCount{ 2 };
        constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None | ImGuiTableFlags_BordersInner };

        ImGui::Spacing();

        if (material->IsType( SkyboxType::eCubeFaces )) {
            if ( ImGui::BeginTable( "##SetupSkyboxComponentTable_CubeFaces", columnCount, specularTableFlags ) ) {
                static constexpr eastl::array<eastl::pair<SkyboxFace, eastl::string_view>, 6> kCubeFaces{{
                    { SkyboxFace::eTop,    "Top ( +Y )"    },
                    { SkyboxFace::eBottom, "Bottom ( -Y )" },
                    { SkyboxFace::eBack,   "Back ( -Z )"   },
                    { SkyboxFace::eFront,  "Front ( +Z )"  },
                    { SkyboxFace::eLeft,   "Left ( -X )"   },
                    { SkyboxFace::eRight,  "Right ( +X )"  }
                }};

                for (usize i{}; i < kCubeFaces.size(); ++i ) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex( 0 );

                    TextureHandle face{ material ? material->GetFace( kCubeFaces[i].first ) : TextureHandle::CreateEmpty() };

                    ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
                    ImGui::SameLine();
                    ImGui::TextUnformatted( string::Format( " Face {}", kCubeFaces[i].second ).c_str() );

                    if ( face.IsEmpty() ) {
                        face = AssetsService::Get()->GetDummyTexture();
                    }

                    if ( PushImageButton( string::Format( "##SetupSkyboxComponentTab:{}", kCubeFaces[i].second ), ImGuiService::Get()->GetTextureID( face.GetRaw() ), ImVec2{ 64, 64 } ) ) {
                        if (material) {
                            LoadMaterialTexture( *material, kCubeFaces[i].first );
                        }
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                            TextureHandle cubeMap{ *as<TextureHandle*>( payload->Data ) };
                            material->SetFace( kCubeFaces[i].first, cubeMap );

                            RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if ( ImGui::IsItemHovered() ) {
                        if ( material->GetFace( kCubeFaces[i].first ).IsEmpty() ) {
                            imgui::ToolTip( "Click me to load a texture." );
                        } else {
                            imgui::ToolTip( [&]() -> void {
                                ShowTextureHoverTooltip( material->GetFace( kCubeFaces[i].first ).GetRaw() );
                            }, ImGui::IsItemHovered() );
                        }
                        ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                    }

                    ImGui::TableSetColumnIndex( 1 );

                    imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
                    imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

                    if ( ImGui::Button( string::Format("Remove {}", kCubeFaces[i].second ).c_str() ) ) {

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
                imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

                if ( ImGui::Button( string::Format( "{} Apply", ICON_MD_CLOUD_DOWNLOAD ).c_str()) ) {
                    mState->mSceneRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }
            }
        } else if (material->IsType( SkyboxType::eEquirectangular )) {
            if ( ImGui::BeginTable( "##SetupSkyboxComponentTable_FlatImage", columnCount, specularTableFlags ) ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );

                TextureHandle face{ material ? material->GetEquirectangular() : TextureHandle::CreateEmpty() };

                ImGui::TextUnformatted( string::Format( "{}", ICON_MD_TEXTURE ).c_str() );
                ImGui::SameLine();
                ImGui::TextUnformatted( " Equirectangular" );

                if ( face.IsEmpty() ) {
                    face = AssetsService::Get()->GetDummyTexture();
                }

                if ( PushImageButton( "##SetupSkyboxComponentTable_FlatImage01", ImGuiService::Get()->GetTextureID( face.GetRaw() ), ImVec2{ 64, 64 } ) ) {
                    if (material) {
                        LoadMaterialTexture( *material );
                        mState->mSceneRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );
                    }
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT_IMAGE") }) {
                        TextureHandle cubeMap{ *as<TextureHandle*>( payload->Data ) };
                        material->SetEquirectangular( cubeMap );
                        mState->mSceneRenderer->SetSkyboxMaterial( sbComponent.GetMaterial() );

                        RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT_IMAGE" );
                    }
                    ImGui::EndDragDropTarget();
                }

                if ( ImGui::IsItemHovered() ) {
                    if ( material->GetEquirectangular().IsEmpty() ) {
                        imgui::ToolTip( "Click me to load a texture." );
                    } else {
                        imgui::ToolTip( [&]() -> void {
                            ShowTextureHoverTooltip( material->GetEquirectangular().GetRaw() );
                        }, ImGui::IsItemHovered() );
                    }
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableSetColumnIndex( 1 );

                imgui::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
                imgui::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

                if ( ImGui::Button( "Remove Image" ) ) {

                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::EndTable();
            }
        }

        ImGui::SeparatorText( "Settings");
        ImGui::Spacing();

        f32 ambientScale{ material->GetAmbientScale() };

        const f32 contentWidth{ ImGui::GetContentRegionAvail().x };
        const f32 labelWidth{ contentWidth * 0.30f };
        const f32 sliderWidth{ contentWidth - labelWidth };

        auto DrawProperty = []( const char* label, const char* id, f32& value, f32 min, f32 max, f32 labelWidth, f32 sliderWidth ) {
            ImGui::PushID( id );

            ImGui::TextUnformatted( label );
            ImGui::SameLine( labelWidth );

            ImGui::SetNextItemWidth( sliderWidth );
            ImGui::SliderFloat( "##Slider", &value, min, max );

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::PopID();
        };

        // Move exposure and gamma to environment tab
        DrawProperty( "Ambient Scale", "AmbientScale", ambientScale, 0.0f, 10.0f, labelWidth, sliderWidth );

        material->SetAmbientScale( ambientScale );
        mState->mSceneRenderer->SetAmbientScale( material->GetAmbientScale() );
    }

    auto InspectorPanel::DrawMeshColliderComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<MeshColliderComponent>()) {
            return;
        }
    }

    auto InspectorPanel::DrawBoxColliderComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<BoxColliderComponent>()) {
            return;
        }
    }

    auto InspectorPanel::DrawSphereColliderComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<SphereColliderComponent>()) {
            return;
        }
    }

    auto InspectorPanel::DrawCapsuleColliderComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<CapsuleColliderComponent>()) {
            return;
        }
    }

    InspectorPanel::InspectorPanel( const InspectorPanelCreateInfo& createInfo )
        : Panel{ "Inspector" }, mState( createInfo.mState ) {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_ERROR_OUTLINE, mPanelName );

        mComponentUis.emplace(
            entt::type_hash<TransformComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Transform", ICON_MD_DEVICE_HUB ),
                .mCallback = &InspectorPanel::DrawTransformComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<MeshComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Mesh", ICON_MD_VIEW_IN_AR ),
                .mCallback = &InspectorPanel::DrawRenderComponentTab } );

        // Physics
        mComponentUis.emplace(
            entt::type_hash<RigidBodyComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Physics", ICON_MD_FITNESS_CENTER ),
                .mCallback = &InspectorPanel::DrawPhysicsComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<MeshColliderComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Mesh Collider", ICON_MD_PERSON_3 ),
                .mCallback = &InspectorPanel::DrawMeshColliderComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<BoxColliderComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Box Collider", ICON_MD_GIF_BOX ),
                .mCallback = &InspectorPanel::DrawBoxColliderComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<SphereColliderComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Sphere Collider", ICON_MD_CIRCLE ),
                .mCallback = &InspectorPanel::DrawSphereColliderComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<CapsuleColliderComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Capsule Collider", ICON_MD_BUNGALOW ),
                .mCallback = &InspectorPanel::DrawCapsuleColliderComponentTab } );

        // Rendering / audio / misc
        mComponentUis.emplace(
            entt::type_hash<LightComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Light", ICON_MD_LIGHT ),
                .mCallback = &InspectorPanel::DrawLightComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<AudioListenerComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Audio Listener", ICON_MD_AUTO_GRAPH ),
                .mCallback = &InspectorPanel::DrawAudioListenerComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<AudioSourceComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Audio", ICON_MD_AUDIOTRACK ),
                .mCallback = &InspectorPanel::DrawAudioComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<TextComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Text", ICON_MD_MESSAGE ),
                .mCallback = &InspectorPanel::DrawTextComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<CameraComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Camera", ICON_MD_CAMERA_ALT ),
                .mCallback = &InspectorPanel::DrawCameraComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<ScriptComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Script", ICON_MD_CODE ),
                .mCallback = &InspectorPanel::DrawScriptingComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<AnimatorComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Animator", ICON_MD_ANIMATION ),
                .mCallback = &InspectorPanel::DrawAnimatorComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<SkinnedMeshRenderer>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} SkinRenderer", ICON_MD_COOKIE ),
                .mCallback = &InspectorPanel::DrawSkinMeshComponentTab } );

        // Materials
        mComponentUis.emplace(
            entt::type_hash<MaterialComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Material", ICON_MD_INSIGHTS ),
                .mCallback = &InspectorPanel::DrawMaterialComponentTab } );

        mComponentUis.emplace(
            entt::type_hash<SkyboxMaterialComponent>::value(),
            ComponentUiInfo{
                .mLabel = string::Format( "{} Skybox", ICON_MD_CLOUD ),
                .mCallback = &InspectorPanel::DrawSkyboxComponentTab } );
    }

    auto InspectorPanel::DrawComponents( Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr || mState->mActiveScene == nullptr ) {
            return;
        }

        const auto& registry{ mState->mActiveScene->GetRegistry() };

        for ( auto&& [type, storage]: registry.storage() ) {
            if ( !storage.contains( entity->Get() ) ) {
                continue;
            }

            const auto it{ mComponentUis.find( type ) };
            if ( it == mComponentUis.end() ) {
                continue;
            }

            const ComponentUiInfo& inspector{ it->second };

            if ( type == entt::type_hash<TransformComponent>::value() ) {
                DrawComponent<TransformComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback,
                    false );
            } else if ( type == entt::type_hash<MeshComponent>::value() ) {
                DrawComponent<MeshComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<RigidBodyComponent>::value() ) {
                DrawComponent<RigidBodyComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<MeshColliderComponent>::value() ) {
                DrawComponent<MeshColliderComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<BoxColliderComponent>::value() ) {
                DrawComponent<BoxColliderComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<SphereColliderComponent>::value() ) {
                DrawComponent<SphereColliderComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<CapsuleColliderComponent>::value() ) {
                DrawComponent<CapsuleColliderComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<LightComponent>::value() ) {
                DrawComponent<LightComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<AudioListenerComponent>::value() ) {
                DrawComponent<AudioListenerComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<AudioSourceComponent>::value() ) {
                DrawComponent<AudioSourceComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<TextComponent>::value() ) {
                DrawComponent<TextComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<CameraComponent>::value() ) {
                DrawComponent<CameraComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<ScriptComponent>::value() ) {
                DrawComponent<ScriptComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<AnimatorComponent>::value() ) {
                DrawComponent<AnimatorComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<SkinnedMeshRenderer>::value() ) {
                DrawComponent<SkinnedMeshRenderer>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<MaterialComponent>::value() ) {
                DrawComponent<MaterialComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            } else if ( type == entt::type_hash<SkyboxMaterialComponent>::value() ) {
                DrawComponent<SkyboxMaterialComponent>(
                    *this,
                    inspector.mLabel,
                    *entity,
                    inspector.mCallback );
            }
        }
    }

    auto InspectorPanel::OnUpdate( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), ImGuiWindowFlags_NoCollapse );

        if ( Entity* target{ mState->mSelectedEntity } ) {
            DrawVisibilityCheckBox( target );

            ImGui::SameLine();

            DrawNameTextInput( target );
            DrawComponentButton( target );

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            DrawComponents( target );
        }

        ImGui::End();
    }
}// namespace Mikoto