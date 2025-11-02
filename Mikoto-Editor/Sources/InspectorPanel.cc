/**
 * InspectorPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <algorithm>
#include <array>
#include <iterator>

// Third-Party Libraries
#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <Common/Common.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Library/Math/Math.hh>
#include <Material/PBRMaterial.hh>
#include <Panels/InspectorPanel.hh>
#include <Scene/Component.hh>
#include <Scene/Entity.hh>

#include "EditorUtility.hh"

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

        // Table showings texture properties
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
            ImGui::TextUnformatted( fmt::format( "{} MB", Math::Round( 2.33, 2 ) ).c_str() );

            ImGui::EndTable();
        }

        if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }
    }

    static auto UpdateMaterialTexture( PBRMaterial& standardMat, MapType mapType ) -> void {
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
                    standardMat.SetTextureType( mapType, texture );
                }

                loading = false;

                TaskService::Get()->Submit( [&]() -> void {

                } );
            }
        }
    }

    static auto DisplayTextureEditTreeNode( std::string_view title, PBRMaterial& standardMat, const std::function<void( PBRMaterial& standardMat )>& func ) -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                    ImGuiTreeNodeFlags_Framed |
                                                    ImGuiTreeNodeFlags_SpanAvailWidth |
                                                    ImGuiTreeNodeFlags_FramePadding };
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::TreeNodeEx( fmt::format( "##{}:{}", "DisplayTextureEditTreeNode", title.data() ).c_str(), treeNodeFlags, title.data() ) ) {

            func( standardMat );

            ImGui::TreePop();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
    }

    static auto EditPBRMaterial_AlbedoMap( PBRMaterial& material ) -> void {
        // We use the standard default font with FONT_ICON_FILE_NAME_MD font icons
        // since the other fonts don't correctly display these icons
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Albedo" );

        TextureHandle diffuseMap{ material.GetTextureType( MapType::ALBEDO_TEXTURE ) };
        if ( diffuseMap.IsEmpty() ) {
            diffuseMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( diffuseMap->GetHandle(), ImGuiService::Get()->GetTextureID( diffuseMap.GetRaw() ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::ALBEDO_TEXTURE );
        }

        if ( material.HasTextureType( MapType::ALBEDO_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( material.GetTextureType( MapType::ALBEDO_TEXTURE ).GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTextureType( MapType::ALBEDO_TEXTURE ) ) {
                ImGuiUtils::ToolTip( "Click me to load a texture." );
            }

            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::SameLine();

        // Table to control albedo mix color and ambient value
        // Table has two rows and one colum
        constexpr auto columnIndex{ 0 };
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_None };

        if ( ImGui::BeginTable( "AlbedoMapEditContentsTable", 1, tableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            glm::vec4 color{ material.GetColor() };
            constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

            if ( ImGui::ColorEdit3( "Color", glm::value_ptr( color ), colorEditFlags ) ) {
                material.SetColor( color );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            float mixing{};
            if ( ImGuiUtils::Slider( "Mix", mixing, { 0.0f, 1.0f } ) ) {
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndex );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
            }

            ImGui::EndTable();
        }
    }

    static auto EditPBRMaterial_MetallicMap( PBRMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Metallic" );

        TextureHandle metallicMap{ material.GetTextureType( MapType::METALLIC_TEXTURE ) };
        if ( metallicMap.IsEmpty() ) {
            metallicMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( metallicMap->GetHandle(), ImGuiService::Get()->GetTextureID( metallicMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::METALLIC_TEXTURE );
        }

        if ( material.HasTextureType( MapType::METALLIC_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( metallicMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTextureType( MapType::METALLIC_TEXTURE ) ) {
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

            if ( ImGuiUtils::Slider( "Metal factor", strength, { 0.0f, 10.0f } ) ) {
                material.SetMetallicFactor( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditPBRMaterial_NormalMap( PBRMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Normal" );

        TextureHandle normalMap{ material.GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) };
        if ( normalMap.IsEmpty() ) {
            normalMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( normalMap->GetHandle(), ImGuiService::Get()->GetTextureID( normalMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::NORMAL_TEXTURE );
        }

        if ( material.HasTextureType( MapType::NORMAL_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( normalMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTextureType( MapType::NORMAL_TEXTURE ) ) {
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

            float strength{ /* TODO */ };

            if ( ImGuiUtils::Slider( "Strength", strength, { 0.0f, 10.0f } ) ) {
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );
            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };
            if ( ImGui::Button( "Remove Texture" ) ) {
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditPBRMaterial_RoughnessMap( PBRMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Roughness" );

        TextureHandle roughnessMap{ material.GetTextureType( MapType::ROUGHNESS_TEXTURE ) };
        if ( roughnessMap.IsEmpty() ) {
            roughnessMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( roughnessMap->GetHandle(), ImGuiService::Get()->GetTextureID( roughnessMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::ROUGHNESS_TEXTURE );
        }

        if ( material.HasTextureType( MapType::ROUGHNESS_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( roughnessMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTextureType( MapType::ROUGHNESS_TEXTURE ) ) {
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

            if ( ImGuiUtils::Slider( "Roughness factor", strength, { 0.0f, 10.0f } ) ) {
                material.SetRoughnessFactor( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditPBRMaterial_AmbientOcclusion( PBRMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Ambient Occlusion" );

        TextureHandle aoMap{ material.GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) };
        if ( aoMap.IsEmpty() ) {
            aoMap = AssetsService::Get()->GetDummyTexture();
        }

        if ( ImGuiUtils::PushImageButton( aoMap->GetHandle(), ImGuiService::Get()->GetTextureID( aoMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::AMBIENT_OCCLUSION_TEXTURE );
        }

        if ( material.HasTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( aoMap.GetRaw() );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) ) {
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

            float strength{};

            if ( ImGuiUtils::Slider( "Strength", strength, { 0.0f, 10.0f } ) ) {
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditPBRMaterial( PBRMaterial* material ) -> void {
        if ( material == nullptr ) {
            return;
        }

        DisplayTextureEditTreeNode( "Albedo", *material, EditPBRMaterial_AlbedoMap );
        DisplayTextureEditTreeNode( "Metallic", *material, EditPBRMaterial_MetallicMap );
        DisplayTextureEditTreeNode( "Roughness", *material, EditPBRMaterial_RoughnessMap );
        DisplayTextureEditTreeNode( "Ambient Occlusion", *material, EditPBRMaterial_AmbientOcclusion );
        DisplayTextureEditTreeNode( "Normal", *material, EditPBRMaterial_NormalMap );
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
                entity->AddComponent<ScriptComponent>( "TODO: PATH" );
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Mesh", menuItemShortcut, menuItemSelected, !IsPresent<MeshComponent>( entity ) ) ) {
                entity->AddComponent<MeshComponent>();

                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Camera", menuItemShortcut, menuItemSelected, !IsPresent<CameraComponent>( entity ) ) ) {
                entity->AddComponent<CameraComponent>( CreateScope<SceneCamera>() );
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

            if ( ImGui::MenuItem( "Text", menuItemShortcut, menuItemSelected, !IsPresent<TextComponent>( entity ) ) ) {
                TextComponent& textComponent{ entity->AddComponent<TextComponent>() };

                // TODO: logic to load new font

                textComponent.SetSize( 12 );
                textComponent.SetContents( "Example" );
                textComponent.SetSpacing( 1 );

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

        for ( auto& texture: meshTarget.GetTextures() ) {
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

        bool wantToRenderActiveEntity{ tag.IsActive() };
        if ( ImGuiUtils::CheckBox( "##DrawVisibilityCheckBox::Checkbox", wantToRenderActiveEntity ) ) {
            tag.SetActive( wantToRenderActiveEntity );
        }
    }

    static auto DrawNameTextInput( Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity->GetComponent<TagComponent>() };

        constexpr ImGuiTextFlags flags{ ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll };

        // Copy the entity's name into the array we will modify
        std::array<char, 1024> name{};
        std::ranges::copy( tag.GetTag(), name.data() );

        if ( ImGui::InputText( "##DrawNameTextInputTag", name.data(), name.max_size(), flags ) ) {
            tag.SetTag( name.data() );
        }
    }

    static auto SetupTransformComponentTab( Entity& entity, Scene* scene ) -> void {
        TransformComponent& transformComponent{ entity.GetComponent<TransformComponent>() };

        glm::vec3 newTranslation{ transformComponent.GetTranslation() };
        glm::vec3 newRotation{ transformComponent.GetRotation() };
        glm::vec3 newScale{ transformComponent.GetScale() };

        const glm::vec3 oldTranslation{ transformComponent.GetTranslation() };
        const glm::vec3 oldScale{ transformComponent.GetScale() };
        const glm::vec3 oldRotation{ transformComponent.GetRotation() };

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

        // Apply the transformation to the children
        // For now Guizmos only change translation so thats the only thing we handle in the children

        glm::vec3 offsetTranslation{ transformComponent.GetTranslation() - oldTranslation };
        glm::vec3 offsetRotation{ transformComponent.GetRotation() - oldRotation };
        glm::vec3 offsetScale{ transformComponent.GetScale() - oldScale };

        // propagate change to children
    }

    static auto SetupScriptingComponentTab( Entity& entity ) -> void {
        ScriptComponent& scriptComponent{ entity.GetComponent<ScriptComponent>() };

        // Static so ImGui input buffer persists
        static std::string formattedPath{};

        if ( scriptComponent.HasScript() ) {
            formattedPath = fmt::format( "{}", scriptComponent.GetScript()->GetFileContents() );
        }

        ImGui::InputText( "##PathToScript", formattedPath.data(), formattedPath.size() + 1, ImGuiInputTextFlags_ReadOnly );
        ImGui::SameLine();

        if ( ImGui::Button( fmt::format( " {} Load ", ICON_MD_SEARCH ).c_str() ) ) {
            const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "LUA Files", "lua" }
            };

            Path path{ FileService::Get()->OpenDialog( filters ).string() };
            if ( !path.empty() ) {
                scriptComponent.SetScript( FileService::Get()->LoadFile( path ) );
            }
        }

        if ( ImGui::IsItemHovered() )
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

        // ---- Script Preview ----
        if ( scriptComponent.HasScript() ) {
            ImGui::Spacing();
            ImGui::SeparatorText( "Script Preview" );

            const std::string& contents = scriptComponent.GetScript()->GetFileContents();

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
                        StringUtils::Concat( "/run_a", " code ", scriptComponent.GetScript()->GetPath() ) );
            }
        }
    }


    static auto SetupMaterialComponentTab( Entity& entity ) -> void {
        // ImGui by default will indent because the items in this function are supposed to be
        // within a Tree Node, items within a tree node appear indented by default when you expand it
        ImGui::Unindent();

        MaterialComponent& materialComponent{ entity.GetComponent<MaterialComponent>() };

        if ( materialComponent.HasMaterial() ) {
            EditPBRMaterial( dynamic_cast<PBRMaterial*>( materialComponent.GetMaterial().GetRaw() ) );
        }

        ImGui::Indent();
    }

    static auto SetupPhysicsComponentTab( Entity& entity ) -> void {
        if (!entity.HasComponent<RigidBodyComponent>())
            return;

        auto& rb = entity.GetComponent<RigidBodyComponent>();

        if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen)) {

            // --- Body Type ---
            {
                const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
                int currentType = static_cast<int>(rb.GetBodyType());

                ImGui::Text("Body Type");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if (ImGui::Combo("##BodyType", &currentType, bodyTypes, IM_ARRAYSIZE(bodyTypes))) {
                    rb.SetBodyType(static_cast<RigidBodyComponent::BodyType>(currentType));
                }
            }

            // --- Use Gravity ---
            {
                bool useGravity = rb.UseGravity();
                if (ImGui::Checkbox("Use Gravity", &useGravity)) {
                    rb.SetUseGravity(useGravity);
                }
            }

            // --- Mass ---
            {
                float mass = rb.GetMass();
                ImGui::Text("Mass");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if (ImGui::DragFloat("##Mass", &mass, 0.1f, 0.0f, 1000.0f, "%.2f")) {
                    rb.SetMass(mass);
                }
            }

            // --- Friction ---
            {
                float friction = rb.GetFriction();
                ImGui::Text("Friction");
                ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
                if (ImGui::SliderFloat("##Friction", &friction, 0.0f, 1.0f, "%.2f")) {
                    rb.SetFriction(friction);
                }
            }

            // --- Internal Handle Info (read-only) ---
            if (auto handle = rb.GetInternalBodyHandle()) {
                ImGui::Separator();
                ImGui::TextDisabled("Internal Handle:");
                ImGui::SameLine();
                ImGui::Text("%p", reinterpret_cast<void*>(handle));
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
                        { "Model files", "obj,gltf,fbx" },
                        { "OBJ files", "obj" },
                        { "glTF files", "gltf" },
                        { "FBX files", "fbx" }
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
            ImGui::TextUnformatted( "Color" );

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 diffuse{};
            if ( ImGuiUtils::ColorEdit4( "##DirectionalLightDiffuse", diffuse ) ) {
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

            glm::vec4 direction{};
            if ( ImGuiUtils::DragFloat4( "##DirectionalLightDirection", "%.2f", direction, 0.1f, 0.0f, 512.0f ) ) {
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
            if ( ImGuiUtils::Slider( "Intensity", intensity, { 1.0f, 100.0f } ) ) {
                pointLightData.SetIntensity( intensity );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{ pointLightData.GetRadius() };
            if ( ImGuiUtils::Slider( "Radius", radius, { 1.0f, 10.0f } ) ) {
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

            glm::vec4 direction{};
            if ( ImGuiUtils::DragFloat4( "Direction", "%.2f", direction, 0.01f, -1.0f, 1.0f ) ) {
                spotLightData.SetDirection( direction );
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "The spot position is determined by the objects position." );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            Vec4F color{};
            if ( ImGuiUtils::ColorEdit4( "Color", color ) ) {
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );


            float intensity{};
            if ( ImGuiUtils::Slider( "Intensity", intensity, { 1.0f, 30000.0f } ) ) {
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{};

            if ( ImGuiUtils::Slider( "Radius", radius, { 1.0f, 500.0f } ) ) {
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float cutOff{};
            if ( ImGuiUtils::Slider( "Cut-off", cutOff, { 0.0f, 180.0f } ) ) {
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Angles in degrees" );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float outerCutOff{};
            if ( ImGuiUtils::Slider( "Outer cut-off", outerCutOff, { 0.0f, 180.0f } ) ) {
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Angles in degrees" );

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

        ImGui::Spacing();
        static std::array textAlignment{ "Center", "Left", "Right" };

        static std::string currentAlignment{ textAlignment[0] };
        ImGuiUtils::ComboList( textAlignment.begin(), textAlignment.end(), currentAlignment, [&]( const std::string_view target ) -> bool { return StringUtils::Equal( currentAlignment, target ); }, "SetupTextComponentTab:Alignment" );

        ImGuiUtils::HelpMarker( "Text alignment.", "(?)", true );

        // Slider float font size
        float currentSize{ textComponent.GetSize() };
        ImGui::Spacing();
        if ( ImGuiUtils::Slider( "##WorldSize", currentSize, { TextComponent::GetMinLetterSize(), 35.0f } ) ) {
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

        std::string content{ textComponent.GetContents() };

        ImGui::Spacing();
        // Max scaling between 1 and 3
        if ( ImGuiUtils::TextArea( content ) ) {
            textComponent.SetContents( content );
        }
    }

    static auto SetupAudioComponentTab( Entity& entity ) -> void {
        AudioSourceComponent& audioComponent{ entity.GetComponent<AudioSourceComponent>() };

        AudioSourceHandle source{ audioComponent.GetSource() };
        AudioHandle clip{ audioComponent.GetClip() };

        // if ( !clip ) {
        //     if ( !ImGuiUtils::ButtonTextIcon( StringUtils::Concat( ICON_MD_ADD, " Add clip" ).c_str() ) ) {
        //         return;
        //     }
        //
        //     audioComponent.SetClip( clip );
        // }

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

        // --- Muted ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Muted" );
        ImGui::TableSetColumnIndex( 1 );
        bool isMuted = source ? source->IsMuted() : false;
        if ( ImGui::Checkbox( "##IsMutedAudio", &isMuted ) ) {
            if ( source ) source->Mute( isMuted );
        }
        if ( ImGui::IsItemHovered() ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

        // --- Loop ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Loop" );
        ImGui::TableSetColumnIndex( 1 );
        bool isLooping = source ? source->IsLooping() : false;
        if ( ImGui::Checkbox( "##IsLoopingAudio", &isLooping ) ) {
            if ( source ) source->SetLooping( isLooping );
        }
        if ( ImGui::IsItemHovered() ) ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

        // --- Volume ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Volume" );
        ImGui::TableSetColumnIndex( 1 );
        float volume{ source ? source->IsMuted() ? 0.0f : source->GetVolume() : 1.0f };
        if ( ImGui::SliderFloat( "##VolumeAudio", &volume, 0.0f, 1.0f ) ) {
            if ( source ) {
                source->SetVolume( volume );
            }
        }

        // --- Playback controls ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Playback" );
        ImGui::TableSetColumnIndex( 1 );

        if ( source ) {
            if ( ImGui::Button( StringUtils::Concat( ICON_MD_PLAY_ARROW, " Play" ).c_str() ) ) {
                source->Play();
            }

            ImGui::SameLine();
            if ( ImGui::Button( StringUtils::Concat( ICON_MD_PAUSE, " Pause" ).c_str() ) ) {
                source->Pause();
            }

            ImGui::SameLine();
            if ( ImGui::Button( StringUtils::Concat( ICON_MD_STOP, " Stop" ).c_str() ) ) {
                source->Stop();
            }
        }

        // --- Progress bar ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        ImGui::TextUnformatted( "Progress" );
        ImGui::TableSetColumnIndex( 1 );
        if ( source ) {
            float duration{ source->GetAudioDuration() };
            float progress{ source->GetCurrentProgress() };
            ImGui::ProgressBar( duration > 0.0f ? progress / duration : 0.0f, ImVec2( -1, 0 ) );
        }

        ImGui::EndTable();
    }


    static auto SetupCameraComponentTab( Entity& entity ) -> void {
        CameraComponent& cameraComponent{ entity.GetComponent<CameraComponent>() };

        static constexpr std::array<std::string, 2> CAMERA_PROJECTION_TYPE_NAMES{
            "Orthographic", "Perspective"
        };

        if ( !cameraComponent.HasCamera() ) {
            if ( !ImGuiUtils::ButtonTextIcon( StringUtils::Concat( ICON_MD_ADD, " Add camera" ).c_str() ) ) {
                return;
            }

            cameraComponent.AddCamera();
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
            const auto& currentProjectionTypeStr{ CAMERA_PROJECTION_TYPE_NAMES[static_cast<UInt32>( cameraCurrentProjectionType )] };

            if ( ImGui::BeginCombo( "##Projection", currentProjectionTypeStr.c_str() ) ) {
                UInt32 projectionIndex{};
                for ( const auto& projectionType: CAMERA_PROJECTION_TYPE_NAMES ) {
                    // Tells whether we want to highlight this projection in the ImGui combo.
                    // This will be the case if this projection type is the current one for this camera.
                    bool isSelected{ projectionType == CAMERA_PROJECTION_TYPE_NAMES[static_cast<UInt32>( cameraCurrentProjectionType )] };

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

    static auto GetInspectorPanelName() -> std::string_view {
        return "Inspector";
    }

    InspectorPanel::InspectorPanel( const InspectorPanelCreateInfo& createInfo )
        : Panel{ ImGuiUtils::MakePanelName( ICON_MD_ERROR_OUTLINE, GetInspectorPanelName() ) }, m_State( createInfo.State ) {}

    auto InspectorPanel::DrawComponents( Entity* entity ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        DrawComponent<TransformComponent>( fmt::format( "{} Transform", ICON_MD_DEVICE_HUB ), *entity, [&]( Entity& target ) -> void { SetupTransformComponentTab( target, m_State->ActiveEditorScene ); }, false );

        DrawComponent<MaterialComponent>( fmt::format( "{} Material", ICON_MD_INSIGHTS ), *entity, SetupMaterialComponentTab );

        DrawComponent<RigidBodyComponent>( fmt::format( "{} Physics", ICON_MD_FITNESS_CENTER ), *entity, SetupPhysicsComponentTab );

        DrawComponent<MeshComponent>( fmt::format( "{} Mesh", ICON_MD_VIEW_IN_AR ), *entity, [&]( Entity& target ) -> void { SetupRenderComponentTab( target, m_State->ActiveEditorScene ); }, false );

        DrawComponent<LightComponent>( fmt::format( "{} Light", ICON_MD_LIGHT ), *entity, SetupLightComponentTab );

        DrawComponent<AudioSourceComponent>( fmt::format( "{} Audio", ICON_MD_AUDIOTRACK ), *entity, SetupAudioComponentTab );

        DrawComponent<TextComponent>( fmt::format( "{} Text", ICON_MD_MESSAGE ), *entity, SetupTextComponentTab );

        DrawComponent<CameraComponent>( fmt::format( "{} Camera", ICON_MD_CAMERA_ALT ), *entity, SetupCameraComponentTab );

        DrawComponent<ScriptComponent>( fmt::format( "{} Script", ICON_MD_CODE ), *entity, SetupScriptingComponentTab );
    }

    auto InspectorPanel::OnUpdate( MKT_UNUSED_VAR float timeStep ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( m_PanelIsVisible ) {
            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

            Entity* target{ m_State->SelectedEntity };

            if ( target != nullptr ) {
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

        m_State->InspectorPanelVisible = m_PanelIsVisible;
    }
}// namespace Mikoto