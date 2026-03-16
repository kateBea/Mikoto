/**
 * InspectorPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <algorithm>
#include <array>
#include <iterator>
#include <numbers>

// Third-Party Libraries
#include <fmt/format.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <Animation/AnimationSystem.hh>
#include <Animation/Animator.hh>
#include <Application/EditorUtility.hh>
#include <Common/Common.hh>
#include <Common/String.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Library/Math/Math.hh>
#include <Material/PhysicalMaterial.hh>
#include <Panels/InspectorPanel.hh>
#include <Scene/Component.hh>
#include <Scene/Entity.hh>

#include "Scripting/ScriptingService.hh"

namespace Mikoto {

    template<typename ComponentType, typename UIFunction, typename... Args>
    static auto DrawComponent( const std::string_view componentLabel, Entity& entity, const UIFunction& uiFunc, const bool hasRemoveButton = true, Args&&... args ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding };

        if ( entity.HasComponent<ComponentType>() ) {
            bool removeComponent{ false };
            const ImVec2 contentRegionAvailable{ ImGui::GetContentRegionAvail() };

            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f } );

            // See ImGui implementation for button dimensions computation
            const float lineHeight{ GImGui->FontSize + GImGui->Style.FramePadding.y * 2.0f };

            const bool componentNodeOpen{
                ImGui::TreeNodeEx( reinterpret_cast<void*>( typeid( ComponentType ).hash_code() ), treeNodeFlags, "%s",
                                   componentLabel.data() )
            };

            // Node frame is hovered
            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::PopStyleVar();

            if ( hasRemoveButton ) {
                ImGui::SameLine( contentRegionAvailable.x - lineHeight * 0.5f );
                if ( ImGui::Button( fmt::format( "{}", ICON_MD_SETTINGS ).c_str(), ImVec2{ lineHeight, lineHeight } ) ) {
                    ImGui::OpenPopup( "ComponentSettingsButton" );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                if ( ImGui::BeginPopup( "ComponentSettingsButton" ) ) {
                    if ( ImGui::MenuItem( "Remove Component" ) ) {
                        removeComponent = true;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }

            if ( componentNodeOpen ) {

                uiFunc( entity, std::forward<Args>( args )... );

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

    static auto ShowTextureHoverTooltip( const Texture* texture ) -> void {
        if ( ImGuiUtils::PushImageButton( texture->GetHandle(), ImGuiService::Get()->GetTextureID( texture ), ImVec2{ 128, 128 } ) ) {
        }

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
            UInt32 width{ static_cast<UInt32>( texture->GetWidth() ) };
            UInt32 height{ static_cast<UInt32>( texture->GetHeight() ) };
            ImGui::TextUnformatted( fmt::format( "{} x {}", width, height ).c_str() );

            // Second row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Type" );

            // Second row - second colum
            constexpr auto type{ FileType::UNKNOWN_FILE_TYPE };

            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( GetFileExtensionName( type ).data() );

            // Third row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "File size" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( fmt::format( "{} MB", Math::Round( 2.33, 2 ) ).c_str() );

            ImGui::EndTable();
        }

        if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }
    }

    static auto UpdateMaterialTexture( PhysicalMaterial& standardMat, MapType mapType ) -> void {
        const std::initializer_list<std::pair<std::string, std::string>> filters{
            { "Textures", "jpg,jpeg,png" },
            { "JPG", "jpg" },
            { "JPEG", "jpeg" },
            { "PNG", "png" }
        };

        const Path path{ FileService::Get()->OpenDialog( filters ) };

        if ( !path.empty() ) {
            static bool loading{ false };

            if ( !loading ) {
                loading = true;

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( path ) };
                if ( !texture.IsEmpty() ) {
                    standardMat.SetTexture( mapType, texture );
                }

                loading = false;

                TaskService::Get()->Submit( [&]() -> void {

                } );
            }
        }
    }

    static auto DisplayTextureEditTreeNode( const std::string_view title, PhysicalMaterial& standardMat, const std::function<void( PhysicalMaterial& standardMat )>& func ) -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                    ImGuiTreeNodeFlags_Framed |
                                                    ImGuiTreeNodeFlags_SpanAvailWidth |
                                                    ImGuiTreeNodeFlags_FramePadding };
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const std::string nodeLabel{ fmt::format( "##{}:{}", "DisplayTextureEditTreeNode", title.data() ) };

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

        TextureHandle diffuseMap{ material.GetTexture( MapType::DIFFUSE_TEXTURE ) };
        if ( diffuseMap.IsEmpty() ) {
            diffuseMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( "##EditDiffuseProperties:TextureID", ImGuiService::Get()->GetTextureID( diffuseMap.GetRaw() ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::DIFFUSE_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstAlbedoMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::DIFFUSE_TEXTURE, dstAlbedoMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::DIFFUSE_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( material.GetTexture( MapType::DIFFUSE_TEXTURE ).GetRaw() );
            },ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTexture( MapType::DIFFUSE_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::DIFFUSE_TEXTURE );
            }

            ImGui::EndTable();
        }
    }

    static auto EditBaseColorProperties( PhysicalMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Albedo" );

        TextureHandle diffuseMap{ material.GetTexture( MapType::BASE_COLOR_TEXTURE ) };
        if ( diffuseMap.IsEmpty() ) {
            diffuseMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( "##EditBaseColorProperties:TextureID", ImGuiService::Get()->GetTextureID( diffuseMap.GetRaw() ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::BASE_COLOR_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstAlbedoMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::BASE_COLOR_TEXTURE, dstAlbedoMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::BASE_COLOR_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( material.GetTexture( MapType::BASE_COLOR_TEXTURE ).GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTexture( MapType::BASE_COLOR_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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
            if ( ImGuiUtils::Slider( "Alpha Cut-Off", cutOff, { 0.0f, 1.0f } ) ) {
                material.SetAlphaMaskCutoff( cutOff );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::BASE_COLOR_TEXTURE );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            PBR_AlphaMode currentAlphaMode{ material.GetAlphaMask() };
            std::array<std::string, static_cast<Size>(PBR_AlphaMode::AlphaMode_Count)> choicesAlpha{
                "Opaque", "Mask", "Blend",
            };

            PBR_AlphaMode newAlphaMode{ ImGuiUtils::Combo( choicesAlpha, currentAlphaMode ) };
            if (currentAlphaMode != newAlphaMode) {
                material.SetAlphaMask( newAlphaMode );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            PBR_Workflow currentWorkFlow{ material.GetWorkflow() };
            std::array<std::string, static_cast<Size>(PBR_Workflow::Workflow_Count)> choicesWorkflow{
                "Metallic-Roughness", "Specular-Glossiness",
            };

            PBR_Workflow newWorkFlow{ ImGuiUtils::Combo( choicesWorkflow, currentWorkFlow ) };
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

        TextureHandle metallicMap{ material.GetTexture( MapType::METALLIC_ROUGHNESS_TEXTURE ) };
        if ( metallicMap.IsEmpty() ) {
            metallicMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( "##EditMetallicRoughnessProperties:TextureID", ImGuiService::Get()->GetTextureID( metallicMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::METALLIC_ROUGHNESS_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstMetallicMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::METALLIC_ROUGHNESS_TEXTURE, dstMetallicMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::METALLIC_ROUGHNESS_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( metallicMap.GetRaw() );
            },ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {
            if ( !material.HasTexture( MapType::METALLIC_ROUGHNESS_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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
            if ( ImGuiUtils::Slider( "Perceptual Roughness", perceptualRoughness, { 0.0f, 1.0f } ) ) {
            }

            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::METALLIC_ROUGHNESS_TEXTURE );
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

        TextureHandle metallicMap{ material.GetTexture( MapType::METALLIC_TEXTURE ) };
        if ( metallicMap.IsEmpty() ) {
            metallicMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( "##EditMetallicProperties::TextureID", ImGuiService::Get()->GetTextureID( metallicMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::METALLIC_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstMetallicMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::METALLIC_TEXTURE, dstMetallicMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::METALLIC_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( metallicMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTexture( MapType::METALLIC_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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

            if ( ImGuiUtils::Slider( "Metal factor", strength, { 0.0f, 1.0f } ) ) {
                material.SetMetallicFactor( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::METALLIC_TEXTURE );
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

        TextureHandle normalMap{ material.GetTexture( MapType::NORMAL_TEXTURE ) };
        if ( normalMap.IsEmpty() ) {
            normalMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( normalMap->GetHandle(), ImGuiService::Get()->GetTextureID( normalMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::NORMAL_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstNormalMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::NORMAL_TEXTURE, dstNormalMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::NORMAL_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( normalMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTexture( MapType::NORMAL_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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
            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::NORMAL_TEXTURE );
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

        TextureHandle normalMap{ material.GetTexture( MapType::EMISSIVE_TEXTURE ) };
        if ( normalMap.IsEmpty() ) {
            normalMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( normalMap->GetHandle(), ImGuiService::Get()->GetTextureID( normalMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::EMISSIVE_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstNormalMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::EMISSIVE_TEXTURE, dstNormalMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::EMISSIVE_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( normalMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTexture( MapType::EMISSIVE_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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

            Vec3F factors{ material.GetEmissiveFactor() };
            if ( ImGuiUtils::ColorEdit3( "Factors", factors ) ) {
                material.SetEmissiveFactor( factors );
            }

            float strength{ material.GetEmissiveStrength() };
            if ( ImGuiUtils::Slider( "Strength", strength, { 0.0f, 10.0f } ) ) {
                material.SetEmissiveStrength( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );
            bool isBloomy{ material.IsBloomy() };
            if (ImGuiUtils::CheckBox( "##EditEmissionProperties:IsBloomy", isBloomy ) ) {
                material.EnableBloom( isBloomy );
            }

            ImGui::SameLine();
            ImGui::TextUnformatted( "Enable bloom" );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );
            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::EMISSIVE_TEXTURE );
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

        TextureHandle roughnessMap{ material.GetTexture( MapType::ROUGHNESS_TEXTURE ) };
        if ( roughnessMap.IsEmpty() ) {
            roughnessMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( roughnessMap->GetHandle(), ImGuiService::Get()->GetTextureID( roughnessMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::ROUGHNESS_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstRoughnessMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::ROUGHNESS_TEXTURE, dstRoughnessMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }
            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::ROUGHNESS_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( roughnessMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTexture( MapType::ROUGHNESS_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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
            if ( ImGuiUtils::Slider( "Roughness", strength, { 0.0f, 1.0f } ) ) {
                material.SetRoughnessFactor( strength );
            }

            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::ROUGHNESS_TEXTURE );
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

        TextureHandle aoMap{ material.GetTexture( MapType::AMBIENT_OCCLUSION_TEXTURE ) };
        if ( aoMap.IsEmpty() ) {
            aoMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( aoMap->GetHandle(), ImGuiService::Get()->GetTextureID( aoMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::AMBIENT_OCCLUSION_TEXTURE );
        }

        // Target from content browser
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload{ ImGui::AcceptDragDropPayload("CONTENT_BROWSER_TEXT") }) {
                TextureHandle dstAoMap{ *static_cast<TextureHandle*>( payload->Data ) };
                material.SetTexture( MapType::AMBIENT_OCCLUSION_TEXTURE, dstAoMap );

                RuntimeConsole::Get()->Debug( "You dropped texture from CONTENT_BROWSER_TEXT" );
            }

            ImGui::EndDragDropTarget();
        }

        if ( material.HasTexture( MapType::AMBIENT_OCCLUSION_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( aoMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTexture( MapType::AMBIENT_OCCLUSION_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
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

            if ( ImGuiUtils::Slider( "AO Factor", factor, { 0.0f, 10.0f } ) ) {
                material.SetAoFactor( factor );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveTexture( MapType::AMBIENT_OCCLUSION_TEXTURE );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditMaterial( PhysicalMaterial* material ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( material == nullptr ) {
            return;
        }

        DisplayTextureEditTreeNode( "Base Color", *material, EditBaseColorProperties );
        DisplayTextureEditTreeNode( "Metal-ness", *material, EditMetallicProperties );
        DisplayTextureEditTreeNode( "Roughness", *material, EditRoughnessProperties );
        DisplayTextureEditTreeNode( "Ambient Occlusion", *material, EditAmbientOcclusionProperties );
        DisplayTextureEditTreeNode( "Normals", *material, EditNormalsProperties );
        DisplayTextureEditTreeNode( "Emission", *material, EditEmissionProperties );
    }

    static auto DrawComponentButton( Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        ImGui::SameLine();
        ImGui::PushItemWidth( -1.0f );

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::Button( "Add component" ) ) {
            ImGui::OpenPopup( "AddComponentButtonPopup" );
        }

        if ( ImGui::BeginPopup( "AddComponentButtonPopup" ) ) {
            constexpr bool menuItemSelected{ false };
            const char* menuItemShortcut{ nullptr };

            if ( ImGui::MenuItem( "Material", menuItemShortcut, menuItemSelected, !IsPresent<MaterialComponent>( entity ) ) ) {
                entity->AddComponent<MaterialComponent>();
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
                entity->AddComponent<AudioSourceComponent>();
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

    static auto DisplayMapInformation( const TextureHandle& texture, const std::string_view mapName ) -> void {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted( fmt::format( "{} ", ICON_MD_PANORAMA ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( mapName.data() );

        ImGui::Spacing();

        if ( ImGuiUtils::PushImageButton( texture->GetHandle(), ImGuiService::Get()->GetTextureID( texture ), ImVec2{ 64, 64 } ) ) {
        }

        ImGui::SameLine();

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp };

        if ( ImGui::BeginTable( "MaterialEditorDiffusePropertiesTable", 2, tableFlags ) ) {
            // First row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Dimensions" );

            // First row - second colum
            ImGui::TableSetColumnIndex( 1 );
            UInt32 width{ static_cast<UInt32>( texture->GetWidth() ) };
            UInt32 height{ static_cast<UInt32>( texture->GetHeight() ) };
            ImGui::TextUnformatted( fmt::format( "{} x {}", width, height ).c_str() );

            // Second row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Type" );

            // Second row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( GetFileExtensionName( FileType::MP3_AUDIO_TYPE ).data() );

            // Third row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "File size" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( fmt::format( "{} MB", Math::Round( 111, 2 ) ).c_str() );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Channels" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( fmt::format( "{}", texture->GetChannels() ).c_str() );

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

        for ( auto& texture: meshTarget.GetProperties().TexturesByUri | std::ranges::views::values ) {
            DisplayMapInformation( texture, texture->GetDebugName() );
        }
    }

    static auto DrawVec3Transform( const std::string_view label, glm::vec3& data, const double resetValue = 0.0, const double columWidth = 100.0, bool uniform = false ) -> void {
        // This Group is part of a unique label
        const std::string labelId{ fmt::format( "{}:{}", MKT_STRINGIFY( DrawVec3Transform ), label.data() ) };

        ImGui::PushID( labelId.data() );

        ImGui::Columns( 2 );
        ImGui::SetColumnWidth( 0, static_cast<float>( columWidth ) );
        ImGui::Text( "%s", label.data() );
        ImGui::NextColumn();
        ImGui::PushMultiItemsWidths( 3, ImGui::CalcItemWidth() );

        ImGuiUtils::ImGuiScopedStyleVar frameBorderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 7.0f, 5.0f } };

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
        if ( ImGuiUtils::CheckBox( "##DrawVisibilityCheckBox::Checkbox", isActive ) ) {
            tag.SetActive( isActive );
        }
    }

    static auto DrawNameTextInput( Entity* entity ) -> void {
        using namespace StringUtil;

        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity->GetComponent<TagComponent>() };

        constexpr ImGuiTextFlags flags{ ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll };

        StaticString<1024> name{ tag.GetTag() };
        if ( ImGui::InputText( "##DrawNameTextInputTag", name.GetData(), name.GetCapacity(), flags ) ) {
            tag.SetTag( name.GetView() );
        }
    }

    static auto SetupTransformComponentTab( Entity& entity, Scene* scene ) -> void {
        TransformComponent& transformComponent{ entity.GetComponent<TransformComponent>() };

        glm::vec3 newTranslation{ transformComponent.GetTranslation() };
        glm::vec3 newRotation{ transformComponent.GetRotation() };
        glm::vec3 newScale{ transformComponent.GetScale() };

        ImGui::Spacing();

        DrawVec3Transform( "Translation", newTranslation );
        DrawVec3Transform( "Rotation", newRotation );

        bool uniformScale{ entity.GetComponent<TransformComponent>().HasUniformScale() };
        DrawVec3Transform( "Scale", newScale, 1.0, 100.0, uniformScale );
        ImGui::SameLine();

        if ( ImGuiUtils::CheckBox( "##SetupTransformComponentTab:UniformScale", uniformScale ) ) {
            entity.GetComponent<TransformComponent>().SetUniformSale( uniformScale );
        }

        if ( ImGui::IsItemHovered() ) {
            ImGuiUtils::ToolTip( "Enable uniform scaling" );
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        transformComponent.SetTranslation( newTranslation );
        transformComponent.SetRotation( newRotation );
        transformComponent.SetScale( newScale );
    }

    static auto SetupScriptingComponentTab( Entity& entity ) -> void {
        ScriptComponent& scriptComponent{ entity.GetComponent<ScriptComponent>() };

        // Static so ImGui input buffer persists
        static std::string formattedPath{};
        const File* file{ FileService::Get()->LoadFile( scriptComponent.GetFilePath() ) };

        if ( file != nullptr ) {
            formattedPath = fmt::format( "{}", file->GetPath() );
        }

        ImGui::InputText( "##PathToScript", formattedPath.data(), formattedPath.size() + 1, ImGuiInputTextFlags_ReadOnly );
        ImGui::SameLine();

        if ( ImGui::Button( fmt::format( " {} Load ", ICON_MD_SEARCH ).c_str() ) ) {
            const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "LUA Files", "lua" }
            };

            Path path{ FileService::Get()->OpenDialog( filters ).string() };
            if ( !path.empty() ) {
                scriptComponent.SetScript( ScriptingService::Get()->LoadScript( path, std::addressof( entity ) ) );
            }
        }

        if ( ImGui::IsItemHovered() )
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

        if ( file != nullptr ) {
            ImGui::Spacing();
            ImGui::SeparatorText( "Script Preview" );

            const std::string& contents{ file->GetContentsString() };

            // Cap preview for performance
            constexpr Size kMaxPreviewSize{ 8192 * 4 };// more generous limit
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
                    lines.emplace_back( start, static_cast<Size>( lineEnd - start ) );
                    start = ( lineEnd == end ) ? end : lineEnd + 1;
                }
            }

            // Style
            ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4{ 0.1f, 0.1f, 0.1f, 0.5f } );
            ImGui::PushStyleVar( ImGuiStyleVar_ChildRounding, 6.0f );
            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 6.0f, 6.0f } );

            const float height{ ImGui::GetTextLineHeightWithSpacing() * 15.0f };
            if ( ImGui::BeginChild( "ScriptPreviewChild", ImVec2{ 0, height }, true, ImGuiWindowFlags_HorizontalScrollbar ) ) {
                ImGuiUtils::ImGuiScopedTextFont newFont{ ImGuiService::Get()->PushFont( "./Resources/Fonts/Google_Sans_Code/static/GoogleSansCode-Light.ttf" ) };

                ImGuiListClipper clipper{};
                clipper.Begin( static_cast<Int32>( lines.size() ) );
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
                RuntimeConsole::Get()->ExecuteCommand(
                        StringUtils::Concat( "/", " code ", file->GetPath() ) );
            }
        }
    }

    static auto SetupAnimatorComponentTab( Entity& entity ) -> void {
        AnimatorComponent& animatorComponent{ entity.GetComponent<AnimatorComponent>() };

        Animator* animator{
            AnimationSystem::Get()->GetAnimator( animatorComponent.GetAnimatorID() )
        };

        std::string currentAnimationName{};

        if ( animator ) {
            if ( const SkinnedAnimation* current{ animator->GetCurrentAnimation() } )
                currentAnimationName = current->GetName();
        }

        ImGuiUtils::UnindentScoped und{};

        ImGuiUtils::DrawNode( "Animation List", [animator, &currentAnimationName]() -> void {
            if ( !animator )
                return;

            const auto& animationList{ animator->GetAnimationList() };

            if ( animationList.empty() )
                return;

            std::vector<std::string> animationNames{};
            animationNames.reserve( animationList.size() );

            for ( const auto& name: animationList | std::views::keys )
                animationNames.push_back( name );

            const Int32 selectionIndex{
                ImGuiUtils::Combo(
                        animationNames.data(),
                        static_cast<Size>( animationNames.size() ),
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

        if ( ImGuiUtils::CheckBox( "Play selected animation", play ) ) {
            if ( !animator )
                return;

            if ( play )
                animator->PlayAnimation( currentAnimationName );
            else
                animator->StopCurrentAnimation();
        }
    }

    static auto SetupSkinMeshComponentTab( Entity& entity ) -> void {
        SkinnedMeshRenderer& skinnedMeshRendererComp{ entity.GetComponent<SkinnedMeshRenderer>() };
    }


    static auto SetupMaterialComponentTab( Entity& entity ) -> void {
        // ImGui by default will indent because the items in this function because this method is run inside the DrawComponent function,
        // so we are within a Tree Node, items within a tree node appear indented by default when you expand it
        ImGui::Unindent();

        MaterialComponent& materialComponent{ entity.GetComponent<MaterialComponent>() };

        if ( materialComponent.HasMaterial() ) {
            EditMaterial( materialComponent.GetMaterial().Dynamic<PhysicalMaterial>() );
        }

        ImGui::Indent();
    }

    static auto SetupPhysicsComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<RigidBodyComponent>()) {
            return;
        }

        auto& rb{ entity.GetComponent<RigidBodyComponent>() };

        if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen)) {

            // --- Body Type ---
            {
                std::array bodyTypes{ "Static", "Kinematic", "Dynamic" };
                Int32 currentType{ static_cast<int>( rb.GetBodyType() ) };

                ImGui::Text("Body Type");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if ( ImGui::Combo( "##BodyType", &currentType, bodyTypes.data(), bodyTypes.size()) ) {
                    rb.SetBodyType(static_cast<RigidBodyComponent::BodyType>(currentType));
                }
            }

            // --- Use Gravity ---
            {
                bool useGravity{ rb.UseGravity() };
                if (ImGui::Checkbox("Use Gravity", &useGravity)) {
                    rb.SetUseGravity(useGravity);
                }
            }

            // --- Mass ---
            {
                float mass{ rb.GetMass() };
                ImGui::Text("Mass");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if (ImGui::DragFloat("##Mass", &mass, 0.1f, 0.0f, 1000.0f, "%.2f")) {
                    rb.SetMass(mass);
                }
            }

            // --- Friction ---
            {
                float friction{ rb.GetFriction() };
                ImGui::Text("Friction");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if (ImGui::SliderFloat("##Friction", &friction, 0.0f, 1.0f, "%.2f")) {
                    rb.SetFriction(friction);
                }
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

            {
                Vec3F linearVel{ rb.GetLinearVelocity() };
                ImGui::Text("Linear Velocity");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if (ImGui::DragFloat3("##LinearVelocity", &linearVel.x, 0.1f)) {
                    rb.SetLinearVelocity(linearVel);
                }
            }

            // --- Angular Velocity ---
            {
                Vec3F angularVel{ rb.GetAngularVelocity() };
                ImGui::Text("Angular Velocity");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if (ImGui::DragFloat3("##AngularVelocity", &angularVel.x, 0.1f)) {
                    rb.SetAngularVelocity(angularVel);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
        }
    }

    static auto SetupRenderComponentTab( Entity& entity, Scene* scene ) -> void {
        MeshComponent& component{ entity.GetComponent<MeshComponent>() };

        ImGui::Unindent();
        ImGui::Spacing();

        ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
        ImGui::Button( fmt::format( " {} Source ", ICON_MD_ARCHIVE ).c_str() );
        ImGui::PopItemFlag();

        ImGui::SameLine();

        Path path{ "" };

        const MeshNode* mesh{ component.GetMesh() };

        if ( mesh != nullptr ) {
            path = component.GetModel()->GetDirectory();
        }

        // Imgui Will need this later, so the buffer must still exist
        // can't be made a with automatic storage duration
        static std::string formatedPath{};
        formatedPath = fmt::format( "{}", path.string() );

        // See imgui assert on the size of the buffer
        // formatedPath.size() already includes the terminator
        ImGui::InputText( "##PathToModel", formatedPath.data(), formatedPath.size() + 1, ImGuiInputTextFlags_ReadOnly );

        ImGui::SameLine();

        static bool loading{ false };
        if ( ImGui::Button( fmt::format( " {} Load ", ICON_MD_SEARCH ).c_str() ) ) {

            if ( !loading ) {

                loading = true;

                TaskService::Get()->Submit( [rootEntity = std::addressof(entity), scene ]() -> void {
                    const std::initializer_list<std::pair<std::string, std::string>> filters{
                        { "Model files", "obj,gltf,fbx,glb" },
                        { "OBJ files", "obj" },
                        { "glTF files", "gltf" },
                        { "FBX files", "fbx" },
                        { "GLB files", "glb" },
                    };

                    Path targetModelPath{ FileService::Get()->OpenDialog( filters ).string() };

                    if ( !targetModelPath.empty() ) {
                        ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( targetModelPath.string() ) };

                        const EntityCreateInfo entityCreateInfo{
                            .Root{ rootEntity },
                            .Name{ model->GetName().c_str() },
                            .Model{ model },
                        };

                        scene->QueueCreateEntity( entityCreateInfo );
                    }

                    loading = false;
                } );
            }
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

    static auto SetupDirectionalLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "DirectionalLightEditTable", 2, tableFlags ) ) {

            auto& direLightData{ lightComponent.Get<DirectionalLight>() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Intensity" );

            ImGui::TableSetColumnIndex( 1 );
            float intensity{ direLightData.GetIntensity() };
            if ( ImGuiUtils::Slider( "##Intensity", intensity, { 1.0f, 10.0f } ) ) {
                direLightData.SetIntensity( intensity );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Color" );

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 diffuse{ direLightData.GetColor(), 1.0f };
            if ( ImGuiUtils::ColorEdit4( "##DirectionalLightDiffuse", diffuse ) ) {
                direLightData.SetColor( diffuse );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Direction" );

            ImGui::SameLine();

            ImGuiUtils::HelpMarker(
                    "In the case of the fourth component having a value of 1.0f\n"
                    "we do light calculations using the light's position instead\n"
                    "which is the position of the game object." );

            ImGui::TableSetColumnIndex( 1 );

            constexpr float PI{ std::numbers::pi_v<float> };
            glm::vec3 direction{ direLightData.GetDirection() };

            if ( ImGuiUtils::DragFloat3( "##DirectionalLightDirection", "%.2f", direction, 0.01f, -PI, PI ) ) {
                direLightData.SetDirection( direction );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Cast shadows" );

            ImGui::TableSetColumnIndex( 1 );
            static bool castShadows{};
            ImGuiUtils::CheckBox( "##DirectionalLightShadows", castShadows );

            ImGui::EndTable();
        }
    }

    static auto SetupPointLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "PointLightMainTable", 2, tableFlags ) ) {
            auto& pointLightData{ lightComponent.Get<PointLight>() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            glm::vec3 diffuseComponent{ pointLightData.GetColor() };
            if ( ImGuiUtils::ColorEdit3( "Color", diffuseComponent ) ) {
                pointLightData.SetColor( diffuseComponent );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float intensity{ pointLightData.GetIntensity() };
            if ( ImGuiUtils::Slider( "Intensity", intensity, { 1.0f, 1000.0f } ) ) {
                pointLightData.SetIntensity( intensity );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{ pointLightData.GetRadius() };
            if ( ImGuiUtils::Slider( "Radius", radius, { 1.0f, 500.0f } ) ) {
                pointLightData.SetRadius( radius );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            static bool castShadows{};
            if ( ImGuiUtils::CheckBox( "Cast shadows", castShadows ) ) {
            }

            ImGui::EndTable();
        }
    }

    static auto SetupSpotLightLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "SpotLightEditTable", 2, tableFlags ) ) {
            auto& spotLightData{ lightComponent.Get<SpotLight>() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            glm::vec3 direction{ spotLightData.GetDirection() };
            if ( ImGuiUtils::DragFloat3( "Direction", "%.2f", direction, 0.01f, -Math::Constants::PI, Math::Constants::PI ) ) {
                spotLightData.SetDirection( direction );
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "The spot position is determined by the objects position." );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            Vec3F color{ spotLightData.GetColor() };
            if ( ImGuiUtils::ColorEdit3( "Color", color ) ) {
                spotLightData.SetColor( color );
            }
            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float intensity{ spotLightData.GetIntensity() };
            if ( ImGuiUtils::Slider( "Intensity", intensity, { 1.0f, 1000.0f } ) ) {
                spotLightData.SetIntensity( intensity );
            }
            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{ spotLightData.GetRadius() };
            if ( ImGuiUtils::Slider( "Attenuation Radius", radius, { 1.0f, 500.0f } ) ) {
                spotLightData.SetRadius( radius );
            }
            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float angle{ spotLightData.GetAngle() };
            if ( ImGuiUtils::Slider( "Angle", angle, { 1.0f, SpotLight::GetMaxAngle() } ) ) {
                spotLightData.SetAngle( angle );
            }
            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Cone angle in degrees" );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float softness{ spotLightData.GetSoftness() };
            if ( ImGuiUtils::Slider( "Softness", softness, { 0.0f, SpotLight::GetMaxSoftness() } ) ) {
                spotLightData.SetSoftness( softness );
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Edge softness of the spotlight" );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            static bool castShadows{};
            ImGuiUtils::CheckBox( "Cast shadows", castShadows );

            ImGui::EndTable();
        }
    }

    static auto SetupLightComponentTab( Entity& entity ) -> void {
        LightComponent& lightComponent{ entity.GetComponent<LightComponent>() };

        static constexpr std::array<std::string_view, 3> lightTypes{ "Directional light", "Point light", "Spot light" };

        const LightType lightType{ lightComponent.GetActiveType() };

        ImGui::TextUnformatted( "Light type " );
        ImGui::SameLine();

        if ( ImGui::BeginCombo( "##LightType", lightTypes[static_cast<Size>( lightType )].data() ) ) {
            Size lightTypeIndex{};
            for ( const auto& currentType: lightTypes ) {
                // Tells whether we want to highlight this light type in the ImGui combo.
                // This will be the case if the current type of light is the same as the component
                const bool isSelected{ currentType == lightTypes[static_cast<Size>( lightType )] };

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
            case LightType::DIRECTIONAL_LIGHT_TYPE:
                SetupDirectionalLightOptions( lightComponent );
                break;

            case LightType::POINT_LIGHT_TYPE:
                SetupPointLightOptions( lightComponent );
                break;

            case LightType::SPOT_LIGHT_TYPE:
                SetupSpotLightLightOptions( lightComponent );
                break;
        }
    }

    static auto SetupTextComponentTab( Entity& entity ) -> void {
        TextComponent& textComponent{ entity.GetComponent<TextComponent>() };

        glm::vec4 color{ textComponent.GetColor() };
        constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

        if ( ImGui::ColorEdit4( "Color", glm::value_ptr( color ), colorEditFlags ) ) {
            textComponent.SetColor( color );
        }

        ImGuiUtils::HelpMarker( "Select the current font.", "(?)", true );

        std::string fontPath{ "Select font" };

        if (textComponent.HasFont()) {
            fontPath = textComponent.GetFont()->GetName();
        }

        ImGui::InputText( "##FontPath", fontPath.data(), fontPath.size() + 1, ImGuiInputTextFlags_ReadOnly );
        ImGui::SameLine();
        if ( ImGui::Button( "Load Font" ) ) {
            static bool loading{ false };
            if ( !loading ) {
                loading = true;

                TaskService::Get()->Submit( [&]() -> void {
                    const std::initializer_list<std::pair<std::string, std::string>> filters{
                        { "Font Files", "ttf" }
                    };

                    Path path{ FileService::Get()->OpenDialog( filters ) };
                    if ( !path.empty() ) {
                        FontHandle newFont{ AssetsService::Get()->LoadAsset<Font>( path ) };

                        if ( !newFont.IsEmpty() ) {
                            textComponent.SetFont( newFont );
                        }
                    }

                    loading = false;
                } );
            }
        }

        ImGui::Spacing();
        static std::array textAlignment{ "Center", "Left", "Right" };

        static std::string currentAlignment{ textAlignment[0] };
        ImGuiUtils::ComboList( textAlignment.begin(), textAlignment.end(), currentAlignment, [&]( const std::string_view target ) -> bool { return StringUtils::Equal( currentAlignment, target ); }, "SetupTextComponentTab:Alignment" );

        ImGuiUtils::HelpMarker( "Text alignment.", "(?)", true );

        // Slider float font size
        float currentSize{ textComponent.GetSize() };
        ImGui::Spacing();
        if ( ImGuiUtils::Slider( "##WorldSize", currentSize, { TextComponent::GetMinLetterSize(), 500.0f } ) ) {
            textComponent.SetSize( currentSize );
        }
        ImGuiUtils::HelpMarker( "Text size in world space.", "(?)", true );

        // Slider float letter spacing
        float spacing{ textComponent.GetSpacing() };
        ImGui::Spacing();
        if ( ImGuiUtils::Slider( "##Spacing", spacing, { TextComponent::GetMinLetterSpacing(), 35.0f } ) ) {
            textComponent.SetSpacing( spacing );
        }

        ImGuiUtils::HelpMarker( "Text inner spacing.", "(?)", true );

        // Slider float letter spacing
        bool isWorldText{ textComponent.IsWorldText() };
        ImGui::Spacing();
        if ( ImGuiUtils::CheckBox( "Is World Text", isWorldText) ) {
            textComponent.SetIsWorldText( isWorldText );
        }

        ImGui::Spacing();

        std::string content{ textComponent.GetContents() };
        if ( ImGuiUtils::TextArea( content ) ) {
            textComponent.SetContents( content );
        }

        // Optionally show atlas info
        if ( ImGui::CollapsingHeader( "Font Atlas Info", ImGuiTreeNodeFlags_DefaultOpen ) ) {
            const Font* font{ textComponent.GetFont() };
            if ( font != nullptr ) {
                TextureHandle atlas{ font->GetAtlas() };

                if ( ImGuiUtils::PushImageButton( atlas->GetHandle(), ImGuiService::Get()->GetTextureID( atlas ), ImVec2{ 256, 256 } ) ) {

                }

                ImGuiUtils::ToolTip( [&]() -> void {
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

    static auto SetupAudioListenerComponentTab( Entity& entity ) -> void {
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
            Vec3F forward{ listener.GetForward() };

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
            Vec3F up{ listener.GetUp() };

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
            Vec3F vel{ listener.GetVelocity() };

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
            listener.SetForward( Vec3F{ 0.0f, 0.0f, -1.0f } );
            listener.SetUp( Vec3F{ 0.0f, 1.0f, 0.0f } );
            listener.SetVelocity( Vec3F{ 0.0f, 0.0f, 0.0f } );
        }
    }

    static auto SetupAudioComponentTab( Entity& entity ) -> void {
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
        static std::string clipPath{};
        clipPath = clip ? clip->GetTrackName() : "";
        ImGui::InputText( "##AudioClipPath", clipPath.data(), clipPath.size() + 1, ImGuiInputTextFlags_ReadOnly );
        ImGui::SameLine();
        if ( ImGui::Button( "Load Clip" ) ) {
            static bool loading{ false };
            if ( !loading ) {
                loading = true;

                TaskService::Get()->Submit( [&]() -> void {
                    const std::initializer_list<std::pair<std::string, std::string>> filters{
                        { "Audio Files", "wav,mp3,ogg" }
                    };

                    Path path{ FileService::Get()->OpenDialog( filters ) };
                    if ( !path.empty() ) {
                        AudioHandle newClip{ AssetsService::Get()->LoadAsset<Audio>( AudioLoadDescription{ .AudioFile{ FileService::Get()->LoadFile( path ) } } ) };

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
        ImGuiUtils::SetCursorHandOnLastItemHovered();

        // --- Muted ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Muted" );
        ImGui::TableSetColumnIndex( 1 );
        bool isMuted{ source ? source->IsMuted() : false };
        if ( ImGui::Checkbox( "##IsMutedAudio", &isMuted ) ) {
            if ( source ) source->Mute( isMuted );
        }
        ImGuiUtils::SetCursorHandOnLastItemHovered();

        // --- Loop ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Loop" );
        ImGui::TableSetColumnIndex( 1 );
        bool isLooping = source ? source->IsLooping() : false;
        if ( ImGui::Checkbox( "##IsLoopingAudio", &isLooping ) ) {
            if ( source ) source->SetLooping( isLooping );
        }
        ImGuiUtils::SetCursorHandOnLastItemHovered();

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
        ImGuiUtils::SetCursorHandOnLastItemHovered();

        // --- Playback controls ---
        if (!source.IsEmpty()) {

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Playback" );
            ImGui::TableSetColumnIndex( 1 );

            if ( ImGui::Button( StringUtils::Concat( ICON_MD_PLAY_ARROW, " Play" ).c_str() ) ) {
                source->Play();
            }
            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::SameLine();
            if ( ImGui::Button( StringUtils::Concat( ICON_MD_PAUSE, " Pause" ).c_str() ) ) {
                source->Pause();
            }
            ImGuiUtils::SetCursorHandOnLastItemHovered();

            ImGui::SameLine();
            if ( ImGui::Button( StringUtils::Concat( ICON_MD_STOP, " Stop" ).c_str() ) ) {
                source->Stop();
            }
            ImGuiUtils::SetCursorHandOnLastItemHovered();

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

    static auto SetupCameraComponentTab( Entity& entity ) -> void {
        CameraComponent& cameraComponent{ entity.GetComponent<CameraComponent>() };

        static const std::array<std::string, 2> cameraProjectionTypeNames{
            "Orthographic", "Perspective"
        };

        if ( !cameraComponent.HasCamera() ) {
            if ( !ImGuiUtils::ButtonTextIcon( StringUtils::Concat( ICON_MD_ADD, " Add camera" ).c_str() ) ) {
                return;
            }
        }

        SceneCamera& sceneCamera{ cameraComponent.GetCamera() };
        const auto cameraCurrentProjectionType{ sceneCamera.GetProjectionType() };

        constexpr ImGuiTableFlags tableFlags{
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchSame
        };

        if ( ImGui::BeginTable( "DirectionalLightEditTable", 2, tableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Projection Type" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );
            const auto& currentProjectionTypeStr{ cameraProjectionTypeNames[static_cast<UInt32>( cameraCurrentProjectionType )] };

            if ( ImGui::BeginCombo( "##Projection", currentProjectionTypeStr.c_str() ) ) {
                UInt32 projectionIndex{};
                for ( const auto& projectionType: cameraProjectionTypeNames ) {
                    // Tells whether we want to highlight this projection in the ImGui combo.
                    // This will be the case if this projection type is the current one for this camera.
                    bool isSelected{ projectionType == cameraProjectionTypeNames[static_cast<UInt32>( cameraCurrentProjectionType )] };

                    // Create a selectable combo item for each perspective
                    if ( ImGui::Selectable( projectionType.c_str(), isSelected ) ) {
                        sceneCamera.SetProjectionType( static_cast<ProjectionType>( projectionIndex ) );
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
                ImGui::TextUnformatted( "Perspective FOV" );

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
                ImGui::TextUnformatted( "Perspective Near" );

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
                ImGui::TextUnformatted( "Perspective Far" );

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

    InspectorPanel::InspectorPanel( const InspectorPanelCreateInfo& createInfo )
        : Panel{  "Inspector" }, m_State( createInfo.State ) {

        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_ERROR_OUTLINE, m_PanelName );
    }

    auto InspectorPanel::DrawComponents( Entity* entity ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        DrawComponent<TransformComponent>( fmt::format( "{} Transform", ICON_MD_DEVICE_HUB ), *entity, [&]( Entity& target ) -> void { SetupTransformComponentTab( target, m_State->ActiveEditorScene ); }, false );
        DrawComponent<MaterialComponent>( fmt::format( "{} Material", ICON_MD_INSIGHTS ), *entity, SetupMaterialComponentTab );
        DrawComponent<MeshComponent>( fmt::format( "{} Mesh", ICON_MD_VIEW_IN_AR ), *entity, [&]( Entity& target ) -> void { SetupRenderComponentTab( target, m_State->ActiveEditorScene ); } );
        DrawComponent<RigidBodyComponent>( fmt::format( "{} Physics", ICON_MD_FITNESS_CENTER ), *entity, SetupPhysicsComponentTab );
        DrawComponent<LightComponent>( fmt::format( "{} Light", ICON_MD_LIGHT ), *entity, SetupLightComponentTab );
        DrawComponent<AudioListenerComponent>( fmt::format( "{} Audio Listener", ICON_MD_AUTO_GRAPH ), *entity, SetupAudioListenerComponentTab );
        DrawComponent<AudioSourceComponent>( fmt::format( "{} Audio", ICON_MD_AUDIOTRACK ), *entity, SetupAudioComponentTab );
        DrawComponent<TextComponent>( fmt::format( "{} Text", ICON_MD_MESSAGE ), *entity, SetupTextComponentTab );
        DrawComponent<CameraComponent>( fmt::format( "{} Camera", ICON_MD_CAMERA_ALT ), *entity, SetupCameraComponentTab );
        DrawComponent<ScriptComponent>( fmt::format( "{} Script", ICON_MD_CODE ), *entity, SetupScriptingComponentTab );

        DrawComponent<AnimatorComponent>( fmt::format( "{} Animator", ICON_MD_ANIMATION ), *entity, SetupAnimatorComponentTab );
        DrawComponent<SkinnedMeshRenderer>( fmt::format( "{} SkinRenderer", ICON_MD_COOKIE ), *entity, SetupSkinMeshComponentTab );
    }

    auto InspectorPanel::OnUpdate( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_PanelIsVisible ) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

        if ( Entity* target{ m_State->SelectedEntity } ) {
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