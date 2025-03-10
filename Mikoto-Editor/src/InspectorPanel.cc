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
#include <GUI/Icons/IconsMaterialDesign.h>
#include <imgui_impl_vulkan.h>

#include <Common/Common.hh>
#include <Core/System/AssetsSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Core/System/GUISystem.hh>
#include <Core/System/RenderSystem.hh>
#include <GUI/ImGuiUtils.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/Math/Math.hh>
#include <Material/Material/PBRMaterial.hh>
#include <Material/Material/StandardMaterial.hh>
#include <Panels/InspectorPanel.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>
#include <Scene/Scene/Component.hh>
#include <Scene/Scene/Entity.hh>
#include <Scene/Scene/Scene.hh>

namespace Mikoto {

    static auto CreateBasicSampler() -> VkSampler {
        VulkanDevice& device{ VulkanContext::Get().GetDevice() };

        VkSamplerCreateInfo samplerInfo{ VulkanHelpers::Initializers::SamplerCreateInfo() };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        const VkPhysicalDeviceProperties& properties{ device.GetPhysicalDeviceProperties() };

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VkSampler sampler{ VK_NULL_HANDLE };

        if ( vkCreateSampler( device.GetLogicalDevice(), &samplerInfo, nullptr, &sampler ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create texture sampler!" );
        }

        VulkanDeletionQueue::Push( [sampler = sampler, device = device.GetLogicalDevice()]() -> void {
            vkDestroySampler( device, sampler, nullptr );
        } );

        return sampler;
    }

    static auto GetDescriptorSetById( const Texture2D* texture ) -> VkDescriptorSet {
        static std::unordered_map<UInt32_T, VkDescriptorSet> dsets{};

        VkDescriptorSet result{ VK_NULL_HANDLE };

        auto itFind{ dsets.find( texture->GetID().Get() ) };

        if ( itFind == dsets.end() ) {
            auto [itInsert, success]{ dsets.try_emplace( texture->GetID().Get(),
                                                         ImGui_ImplVulkan_AddTexture( CreateBasicSampler(), dynamic_cast<const VulkanTexture2D*>( texture )->GetImage().GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) ) };

            if ( success ) {
                result = itInsert->second;

                GUISystem& guiSystem{ Engine::GetSystem<GUISystem>() };

                guiSystem.AddShutdownCallback( [textureDset = itInsert->second]() -> void {
                    ImGui_ImplVulkan_RemoveTexture( textureDset );
                } );
            }
        } else {
            result = itFind->second;
        }

        return result;
    }

    template<typename ComponentType, typename UIFunction, typename... Args>
    static auto DrawComponent( const std::string_view componentLabel, Entity& entity, const UIFunction& uiFunc, const bool hasRemoveButton = true, Args&&... args ) -> void {
        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding };

        if ( entity.HasComponent<ComponentType>() ) {
            bool removeComponent{ false };
            const ImVec2 contentRegionAvailable{ ImGui::GetContentRegionAvail() };

            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f } );

            // See ImGui implementation for button dimensions computation
            const float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f };

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

    static auto ShowTextureHoverTooltip( const Texture2D* texture ) -> void {
        if (ImGuiUtils::PushImageButton( texture->GetID().Get(), GetDescriptorSetById( texture ), ImVec2{ 128, 128 } )) {

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
            UInt32_T width{ static_cast<UInt32_T>( texture->GetWidth() ) };
            UInt32_T height{ static_cast<UInt32_T>( texture->GetHeight() ) };
            ImGui::TextUnformatted( fmt::format( "{} x {}", width, height ).c_str() );

            // Second row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Type" );

            // Second row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( GetFileExtensionName( texture->GetFile()->GetType() ).data() );

            // Third row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "File size" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( fmt::format( "{} MB", Math::Round( texture->GetFile()->GetSize(), 2 ) ).c_str() );

            ImGui::EndTable();
        }

        if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }
    }

    static auto UpdateMaterialTexture( Material& standardMat, MapType mapType ) -> void {
        // Load a new texture
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };
        AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

        const std::initializer_list<std::pair<std::string, std::string>> filters{
            { "Textures", "jpg, jpeg, png" },
            { "JPG", "jpg" },
            { "JPEG", "jpeg" },
            { "PNG", "png" }
        };

        const Path_T path{ fileSystem.OpenDialog( filters ) };

        if ( !path.empty() ) {
            Texture* loadedTexture{ assetsSystem.LoadTexture( TextureLoadInfo{
                    .Path{ path },
                    .Type{ mapType },
            } ) };

            if ( loadedTexture != nullptr ) {
                standardMat.SetTexture( loadedTexture, mapType );
            }
        }
    }

    static auto EditStandardMaterial( StandardMaterial& standardMat ) -> void {
        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding };
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::TreeNodeEx( reinterpret_cast<const void*>( "EditStandardMaterialDiffuseTreeNode" ), treeNodeFlags, "Diffuse" ) ) {
            // We use the standard default font with FONT_ICON_FILE_NAME_MD font icons
            // since the other fonts don't correctly display these icons
            ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
            ImGui::SameLine();
            ImGui::TextUnformatted( " Albedo" );

            // TODO: will need to update descriptor sets with new texture handles same for specular map because we use the same desc set
            // or temporarily make this function a member of inspector channel and create an map with texture id and its descriptor set
            Texture2D* diffuseMap{ standardMat.GetDiffuseMap() };
            if (ImGuiUtils::PushImageButton( diffuseMap->GetID().Get(), GetDescriptorSetById( diffuseMap ), ImVec2{ 64, 64 } ) ) {
                UpdateMaterialTexture( standardMat, MapType::TEXTURE_2D_DIFFUSE );
            }

            if ( standardMat.HasDiffuseMap() ) {
                ImGuiUtils::ToolTip( [&]() -> void {
                    ShowTextureHoverTooltip( diffuseMap );
                }, ImGui::IsItemHovered() );
            }

            if ( ImGui::IsItemHovered()) {

                if ( !standardMat.HasDiffuseMap() ) {
                    ImGuiUtils::ToolTip( "Click me to load a texture." );
                }

                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::SameLine();

            // Table to control albedo mix color and ambient value
            // Table has two rows and one colum
            constexpr auto columnIndex{ 0 };
            constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_None };

            if ( ImGui::BeginTable( "DiffuseEditContentsTable", 1, tableFlags ) ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( columnIndex );

                glm::vec4 color{ standardMat.GetColor() };
                constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

                if ( ImGui::ColorEdit4( "Color", glm::value_ptr( color ), colorEditFlags ) ) {
                    standardMat.SetColor( color );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( columnIndex );

                static float mixing{};
                ImGui::SliderFloat( "Mix", std::addressof( mixing ), 0.0f, 1.0f );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( columnIndex );
                ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
                ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };
                if (ImGui::Button( "Remove Texture" )) {
                    standardMat.RemoveMap( MapType::TEXTURE_2D_DIFFUSE );
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::Spacing();

        if ( ImGui::TreeNodeEx( reinterpret_cast<const void*>( "EditStandardMaterialSpecularTreeNode" ), treeNodeFlags, "Specular" ) ) {
            ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
            ImGui::SameLine();
            ImGui::TextUnformatted( " Specular" );

            Texture2D* specularMap{ standardMat.GetSpecularMap() };

            if ( ImGuiUtils::PushImageButton( specularMap->GetID().Get(), GetDescriptorSetById( specularMap ), ImVec2{ 64, 64 } ) ) {
                UpdateMaterialTexture( standardMat, MapType::TEXTURE_2D_SPECULAR );
            }

            if ( standardMat.HasSpecularMap() ) {
                ImGuiUtils::ToolTip( [&]() -> void {
                    ShowTextureHoverTooltip( specularMap );
                }, ImGui::IsItemHovered() );
            }

            if ( ImGui::IsItemHovered()) {

                if (!standardMat.HasSpecularMap()) {
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

            if ( ImGui::BeginTable( "SpecularEditContentsTable", columnCount, specularTableFlags ) ) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( columnIndexSpecular );

                static float strength{};

                ImGui::SliderFloat( "Strength", std::addressof( strength ), 0.0f, 32.0f );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( columnIndexSpecular );
                ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
                ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };
                if (ImGui::Button( "Remove Texture" )) {
                    standardMat.RemoveMap( MapType::TEXTURE_2D_SPECULAR );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
    }

    static auto DisplayTextureEditTreeNode(std::string_view title, PBRMaterial& standardMat, const std::function<void(PBRMaterial& standardMat)>& func) -> void {
        constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding };
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if ( ImGui::TreeNodeEx( fmt::format( "##{}:{}", "DisplayTextureEditTreeNode", title.data()).c_str(), treeNodeFlags, title.data() ) ) {

            func(standardMat);

            ImGui::TreePop();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
    }

    static auto EditPBRMaterial_AlbedoMap(PBRMaterial& material) -> void {
        // We use the standard default font with FONT_ICON_FILE_NAME_MD font icons
            // since the other fonts don't correctly display these icons
            ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
            ImGui::SameLine();
            ImGui::TextUnformatted( " Albedo" );

            Texture2D* diffuseMap{ material.GetAlbedoMap() };
            if (ImGuiUtils::PushImageButton( diffuseMap->GetID().Get(), GetDescriptorSetById( diffuseMap ), ImVec2{ 64, 64 } ) ) {
                UpdateMaterialTexture( material, MapType::TEXTURE_2D_DIFFUSE );
            }

            if ( material.HasAlbedoMap() ) {
                ImGuiUtils::ToolTip( [&]() -> void {
                    ShowTextureHoverTooltip( diffuseMap );
                }, ImGui::IsItemHovered() );
            }

            if ( ImGui::IsItemHovered()) {

                if ( !material.HasAlbedoMap() ) {
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

                glm::vec4 color{ material.GetAlbedoFactors() };
                constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

                if ( ImGui::ColorEdit4( "Color", glm::value_ptr( color ), colorEditFlags ) ) {
                    material.SetAlbedoFactors( color );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( columnIndex );

                float mixing{};
                if (ImGuiUtils::Slider( "Mix", mixing, { 0.0f, 1.0f } ) ) {

                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( columnIndex );

                ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
                ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

                if (ImGui::Button( "Remove Texture" )) {
                    material.RemoveMap( MapType::TEXTURE_2D_DIFFUSE );
                }

                ImGui::EndTable();
            }
    }

    static auto EditPBRMaterial_MetallicMap( PBRMaterial& material ) -> void {
        ImGui::TextUnformatted( fmt::format( "{}", ICON_MD_TEXTURE ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( " Metallic" );

        Texture2D* metallicMap{ material.GetMetallicMap() };

        if ( ImGuiUtils::PushImageButton( metallicMap->GetID().Get(), GetDescriptorSetById( metallicMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::TEXTURE_2D_METALLIC );
        }

        if ( material.HasMetallicMap() ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( metallicMap );
            },ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasMetallicMap() ) {
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

            if (ImGuiUtils::Slider( "Metal factor", strength, { 0.0f, 10.0f } )) {
                material.SetMetallicFactor( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveMap( MapType::TEXTURE_2D_METALLIC );
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

        Texture2D* normalMap{ material.GetNormalMap() };

        if ( ImGuiUtils::PushImageButton( normalMap->GetID().Get(), GetDescriptorSetById( normalMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::TEXTURE_2D_NORMAL );
        }

        if ( material.HasNormalMap() ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( normalMap );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasNormalMap() ) {
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

            if (ImGuiUtils::Slider( "Strength", strength, { 0.0f, 10.0f } )) {

            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );
            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };
            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveMap( MapType::TEXTURE_2D_NORMAL );
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

        Texture2D* roughnessMap{ material.GetRoughnessMap() };

        if ( ImGuiUtils::PushImageButton( roughnessMap->GetID().Get(), GetDescriptorSetById( roughnessMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::TEXTURE_2D_ROUGHNESS );
        }

        if ( material.HasRoughnessMap() ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( roughnessMap );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasRoughnessMap() ) {
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

            if (ImGuiUtils::Slider( "Roughness factor", strength, { 0.0f, 10.0f } ) ) {
                material.SetRoughnessFactor( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveMap( MapType::TEXTURE_2D_ROUGHNESS );
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

        Texture2D* specularMap{ material.GetAOMap() };

        if ( ImGuiUtils::PushImageButton( specularMap->GetID().Get(), GetDescriptorSetById( specularMap ), ImVec2{ 64, 64 } ) ) {
            UpdateMaterialTexture( material, MapType::TEXTURE_2D_AMBIENT_OCCLUSION );
        }

        if ( material.HasAmbientOcclusionMap() ) {
            ImGuiUtils::ToolTip( [&]() -> void {
                ShowTextureHoverTooltip( specularMap );
            },
                                 ImGui::IsItemHovered() );
        }

        if ( ImGui::IsItemHovered() ) {

            if ( !material.HasAmbientOcclusionMap() ) {
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

            float strength{ material.GetAmbientOcclusionFactor() };

            if (ImGuiUtils::Slider( "Strength", strength, { 0.0f, 10.0f } ) ) {
                material.SetAmbientOcclusionFactor( strength );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( columnIndexSpecular );

            ImGuiUtils::ImGuiScopedStyleVar borderSize{ ImGuiStyleVar_FrameBorderSize, 1.5f };
            ImGuiUtils::ImGuiScopedStyleVar innerSpacing{ ImGuiStyleVar_FramePadding, ImVec2{ 5.0f, 5.0f } };

            if ( ImGui::Button( "Remove Texture" ) ) {
                material.RemoveMap( MapType::TEXTURE_2D_AMBIENT_OCCLUSION );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto EditPBRMaterial( PBRMaterial& material ) -> void {
        DisplayTextureEditTreeNode( "Albedo", material, EditPBRMaterial_AlbedoMap );
        DisplayTextureEditTreeNode( "Metallic", material, EditPBRMaterial_MetallicMap );
        DisplayTextureEditTreeNode( "Roughness", material, EditPBRMaterial_RoughnessMap );
        DisplayTextureEditTreeNode( "Ambient Occlusion", material, EditPBRMaterial_AmbientOcclusion );
        DisplayTextureEditTreeNode( "Normal", material, EditPBRMaterial_NormalMap );
    }

    static auto DrawComponentButton( Entity& entity ) -> void {
        if ( !entity.IsValid() ) {
            return;
        }

        ImGui::SameLine();
        ImGui::PushItemWidth( -1.0f );

        if ( ImGui::Button( "Add component" ) ) {
            ImGui::OpenPopup( "AddComponentButtonPopup" );
        }

        if ( ImGui::BeginPopup( "AddComponentButtonPopup" ) ) {
            constexpr bool menuItemSelected{ false };
            const char* menuItemShortcut{ nullptr };

            if ( ImGui::MenuItem( "Material", menuItemShortcut, menuItemSelected,
                                  !entity.HasComponent<MaterialComponent>() ) ) {
                entity.AddComponent<MaterialComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Script", menuItemShortcut, menuItemSelected,
                                  !entity.HasComponent<NativeScriptComponent>() ) ) {
                entity.AddComponent<NativeScriptComponent>("TODO: PATH");
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Mesh", menuItemShortcut, menuItemSelected,
                !entity.HasComponent<RenderComponent>() ) ) {
                entity.AddComponent<RenderComponent>();

                // If we add a render component, we also need to add a material component
                // which determines how this objects will be rendered

                if ( !entity.HasComponent<MaterialComponent>() ) {
                    entity.AddComponent<MaterialComponent>();
                }

                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Camera", menuItemShortcut, menuItemSelected,
                                  !entity.HasComponent<CameraComponent>() ) ) {
                entity.AddComponent<CameraComponent>( CreateScope<SceneCamera>() );
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Lighting", menuItemShortcut, menuItemSelected,
                                  !entity.HasComponent<LightComponent>() ) ) {
                entity.AddComponent<LightComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Physics", menuItemShortcut, menuItemSelected,
                                  !entity.HasComponent<PhysicsComponent>() ) ) {
                entity.AddComponent<PhysicsComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Audio", menuItemShortcut, menuItemSelected, !entity.HasComponent<AudioComponent>() ) ) {
                entity.AddComponent<AudioComponent>();
                ImGui::CloseCurrentPopup();
            }

            if ( ImGui::MenuItem( "Text", menuItemShortcut, menuItemSelected, !entity.HasComponent<TextComponent>() ) ) {
                TextComponent& textComponent{ entity.AddComponent<TextComponent>() };

                textComponent.LoadFont( "TODO" );
                textComponent.SetFontSize( 12 );
                textComponent.SetTextContent( "Example" );
                textComponent.SetLetterSpacing( 1 );

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopItemWidth();
    }

    static auto DisplayMapInformation( const Texture2D* map, const std::string_view mapName ) -> void {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted( fmt::format( "{} ", ICON_MD_PANORAMA ).c_str() );
        ImGui::SameLine();
        ImGui::TextUnformatted( mapName.data() );

        ImGui::Spacing();

        if (ImGuiUtils::PushImageButton( map->GetID().Get(), GetDescriptorSetById( map ), ImVec2{ 64, 64 } )) {

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
            UInt32_T width{ static_cast<UInt32_T>( map->GetWidth() ) };
            UInt32_T height{ static_cast<UInt32_T>( map->GetHeight() ) };
            ImGui::TextUnformatted( fmt::format( "{} x {}", width, height ).c_str() );

            // Second row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Type" );

            // Second row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( GetFileExtensionName( map->GetFile()->GetType() ).data() );

            // Third row - first colum
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "File size" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( fmt::format( "{} MB", Math::Round( map->GetFile()->GetSize(), 2 ) ).c_str() );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Channels" );

            // Third row - second colum
            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( fmt::format( "{}", map->GetChannels() ).c_str() );

            ImGui::EndTable();
        }
    }

    static auto ShowGameObjectMaterialInfo( const Mesh& meshTarget ) -> void {
        ImGui::Spacing();
        ImGui::TextUnformatted( "Mesh Info" );
        ImGui::SameLine();

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Assumes we have one type of each map for a single mesh!
        // A mesh cannot have more than one diffuse map, for example.
        Int32_T diffuseIndex{ -1 };
        Int32_T specularIndex{ -1 };
        Int32_T emissionIndex{ -1 };
        Int32_T normalIndex{ -1 };
        Int32_T roughnessIndex{ -1 };
        Int32_T metallicIndex{ -1 };
        Int32_T aoIndex{ -1 };

        // Get the texture indices
        for ( Int32_T index{}; index < static_cast<Int32_T>( meshTarget.GetTextures().size() ); ++index ) {
            switch ( meshTarget.GetTextures()[index]->GetType() ) {
                case MapType::TEXTURE_2D_DIFFUSE:
                    diffuseIndex = index;
                    break;
                case MapType::TEXTURE_2D_SPECULAR:
                    specularIndex = index;
                    break;
                case MapType::TEXTURE_2D_EMISSIVE:
                    emissionIndex = index;
                    break;
                case MapType::TEXTURE_2D_NORMAL:
                    normalIndex = index;
                    break;
                case MapType::TEXTURE_2D_ROUGHNESS:
                    roughnessIndex = index;
                    break;
                case MapType::TEXTURE_2D_METALLIC:
                    metallicIndex = index;
                    break;
                case MapType::TEXTURE_2D_AMBIENT_OCCLUSION:
                    aoIndex = index;
                    break;

                case MapType::TEXTURE_2D_INVALID:
                case MapType::TEXTURE_2D_COUNT:
                default:
                    MKT_CORE_LOGGER_ERROR( "Error, type of texture not valid!" );
                    break;
            }
        }

        // Diffuse
        if ( diffuseIndex != -1 ) {
            DisplayMapInformation( meshTarget.GetTextures()[diffuseIndex], "Albedo" );
        }

        // Specular
        if ( specularIndex != -1 ) {
            DisplayMapInformation( meshTarget.GetTextures()[specularIndex], "Specular" );
        }

        // Normal
        if ( normalIndex != -1 ) {
            DisplayMapInformation( meshTarget.GetTextures()[normalIndex], "Normal" );
        }

        // Emissive
        if ( emissionIndex != -1 ) {
            DisplayMapInformation( meshTarget.GetTextures()[emissionIndex], "Emmissive" );
        }

        // Roughness
        if ( roughnessIndex != -1 ) {
            DisplayMapInformation( meshTarget.GetTextures()[roughnessIndex], "Roughness" );
        }

        // Metallic
        if ( metallicIndex != -1 ) {
            DisplayMapInformation( meshTarget.GetTextures()[metallicIndex], "Metal" );
        }

        // Ao
        if ( aoIndex != -1 ) {
            DisplayMapInformation( meshTarget.GetTextures()[aoIndex], "Ambient Occlusion" );
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

        const float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 3.0f };
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

    static auto DrawVisibilityCheckBox( Entity& entity ) -> void {
        if ( !entity.IsValid() ) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity.GetComponent<TagComponent>() };

        bool wantToRenderActiveEntity{ tag.IsVisible() };
        if ( ImGui::Checkbox( "##DrawVisibilityCheckBox::Checkbox", std::addressof( wantToRenderActiveEntity ) ) ) {
            tag.SetVisibility( wantToRenderActiveEntity );
        }
    }

    static auto DrawNameTextInput( Entity& entity ) -> void {
        if ( !entity.IsValid() ) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity.GetComponent<TagComponent>() };

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
        ImGui::SameLine(  );

        if (ImGuiUtils::CheckBox("##SetupTransformComponentTab:UniformScale", uniformScale)) {
            entity.GetComponent<TransformComponent>().WantUniformSale(uniformScale);
        }

        if (ImGui::IsItemHovered(  )) {
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

        auto& hierarchy{ scene->GetHierarchy() };
        hierarchy.ForAllChildren( [&]( Entity* child ) -> void {
            TransformComponent& childTransform{ child->GetComponent<TransformComponent>() };

            childTransform.SetTranslation( childTransform.GetTranslation() + offsetTranslation );
            childTransform.SetRotation( childTransform.GetRotation() + offsetRotation );
            childTransform.SetScale( childTransform.GetScale() + offsetScale );

        } , [&](Entity* target) -> bool {
            return target->GetComponent<TagComponent>().GetGUID() ==
                entity.GetComponent<TagComponent>().GetGUID();
        });

    }

    static auto SetupNativeScriptingComponentTab( Entity& entity ) -> void {
    }

    static auto SetupMaterialComponentTab( Entity& entity ) -> void {
        // ImGui by default will indent because the items in this function are supposed to be
        // within a Tree Node, items within a tree node appear indented by default when you expand it
        ImGui::Unindent();


        MaterialComponent& materialComponent{ entity.GetComponent<MaterialComponent>() };

        if (materialComponent.HasMaterial()) {
            Material& material{ materialComponent.GetMaterial() };
            if (material.GetType() == MaterialType::STANDARD) {
                EditStandardMaterial( dynamic_cast<StandardMaterial&>( material ) );
            }

            if (material.GetType() == MaterialType::PBR) {
                EditPBRMaterial( dynamic_cast<PBRMaterial&>( material ) );
            }
        }

        ImGui::Indent();
    }

    static auto SetupPhysicsComponentTab( Entity& entity ) -> void {
    }

    static auto SetupRenderComponentTab( Entity& entity, Scene* scene ) -> void {
        RenderComponent& component{ entity.GetComponent<RenderComponent>() };

        ImGui::Unindent();
        ImGui::Spacing();

        ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
        ImGui::Button( fmt::format( " {} Source ", ICON_MD_ARCHIVE ).c_str() );
        ImGui::PopItemFlag();

        ImGui::SameLine();

        Path_T path{ "" };

        const Mesh* mesh{ component.GetMesh() };

        if ( mesh != nullptr ) {
            path = mesh->GetDirectory().string();
        }

        // Imgui Will need this later, so the buffer must still exist
        // can't be made a with automatic storage duration
        static std::string formatedPath{  };
        formatedPath = fmt::format( "{}", path.string());

        // See imgui assert on the size of the buffer
        // formatedPath.size() already includes the terminator
        ImGui::InputText( "##PathToModel", formatedPath.data(), formatedPath.size() + 1, ImGuiInputTextFlags_ReadOnly );

        ImGui::SameLine();

        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };
        RenderSystem& renderSystem{ Engine::GetSystem<RenderSystem>() };
        AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

        if ( ImGui::Button( fmt::format( " {} Load ", ICON_MD_SEARCH ).c_str() ) ) {
            const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Model files", "obj, gltf, fbx" },
                { "OBJ files", "obj" },
                { "glTF files", "gltf" },
                { "FBX files", "fbx" }
            };

            path = fileSystem.OpenDialog( filters ).string();

            if ( !path.empty() ) {
                const ModelLoadInfo modelLoadInfo{
                    .Path = path,
                    .InvertedY = renderSystem.GetDefaultApi() == GraphicsAPI::VULKAN_API,
                    .WantTextures = true,
                };

                const Model* model{ assetsSystem.LoadModel( modelLoadInfo ) };

                const EntityCreateInfo entityCreateInfo{
                    .Name = path.stem().string(),
                    .Root = std::addressof( entity ),
                    .ModelMesh = model,
                };

                scene->CreateEntity( entityCreateInfo );
            }
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (mesh != nullptr) {
            ShowGameObjectMaterialInfo( *mesh );
        }

        ImGui::Indent();
    }

    static auto SetupDirectionalLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "DirectionalLightEditTable", 2, tableFlags ) ) {

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Color" );

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 diffuse{ lightComponent.GetDirLightData().Diffuse };
            if ( ImGuiUtils::ColorEdit4( "##DirectionalLightDiffuse", diffuse  ) ) {
                lightComponent.GetDirLightData().Diffuse = diffuse;
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

            glm::vec4 direction{ lightComponent.GetDirLightData().Direction };
            if ( ImGuiUtils::DragFloat4( "##DirectionalLightDirection", "%.2f", direction, 0.1f, 0.0f, 512.0f) ) {
                lightComponent.GetDirLightData().Direction = direction;
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
            auto& pointLightData{ lightComponent.GetPointLightData() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            glm::vec4 diffuseComponent{ pointLightData.Diffuse };
            if ( ImGuiUtils::ColorEdit4( "Color", diffuseComponent ) ) {
                pointLightData.Diffuse = diffuseComponent;
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float intensity{ pointLightData.AttenuationParams.x };

            if ( ImGuiUtils::Slider( "Intensity", intensity, { 1.0f, 	30000.0f } ) ) {
                pointLightData.AttenuationParams.x = intensity;
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{ pointLightData.AttenuationParams.y };

            if ( ImGuiUtils::Slider( "Radius", radius, { 1.0f, 500.0f }) ) {
                pointLightData.AttenuationParams.y = radius;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            static bool castShadows{};
            if (ImGuiUtils::CheckBox( "Cast shadows", castShadows )) {

            }

            ImGui::EndTable();
        }
    }

    static auto SetupSpotLightLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "SpotLightEditTable", 2, tableFlags ) ) {
            auto& spotLightData{ lightComponent.GetSpotLightData() };

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            glm::vec4 direction{ spotLightData.Direction };
            if ( ImGuiUtils::DragFloat4( "Direction", "%.2f", direction, 0.01f, -1.0f, 1.0f) ) {
                spotLightData.Direction = direction;
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "The spot position is determined by the objects position." );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            glm::vec4 diffuseComponent{ spotLightData.Diffuse };
            if ( ImGuiUtils::ColorEdit4( "Color", diffuseComponent ) ) {
                spotLightData.Diffuse = diffuseComponent;
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float intensity{ spotLightData.Params.z };

            if ( ImGuiUtils::Slider( "Intensity", intensity, { 1.0f, 	30000.0f } ) ) {
                spotLightData.Params.z = intensity;
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float radius{ spotLightData.Params.w };

            if ( ImGuiUtils::Slider( "Radius", radius, { 1.0f, 500.0f }) ) {
                spotLightData.Params.w = radius;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float cutOff{ glm::degrees( spotLightData.Params.x ) };
            if ( ImGuiUtils::Slider( "Cut-off", cutOff, { 0.0f, 180.0f })  ) {
                spotLightData.Params.x = glm::radians( cutOff );
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker( "Angles in degrees" );

            ImGui::Spacing();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            float outerCutOff{ glm::degrees( spotLightData.Params.y ) };
            if ( ImGuiUtils::Slider( "Outer cut-off", outerCutOff, { 0.0f, 180.0f }) ) {
                spotLightData.Params.y = glm::radians( outerCutOff );

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

        const LightType lightType{ lightComponent.GetType() };

        ImGui::TextUnformatted( "Light type " );
        ImGui::SameLine();

        if ( ImGui::BeginCombo( "##LightType", lightTypes[static_cast<Size_T>( lightType )].data() ) ) {
            Size_T lightTypeIndex{};
            for ( const auto& currentType: lightTypes ) {
                // Tells whether we want to highlight this light type in the ImGui combo.
                // This will be the case if the current type of light is the same as the component
                const bool isSelected{ currentType == lightTypes[static_cast<Size_T>( lightType )] };

                // This cast is valid because lightTypeIndex is always in the range [0, 2]
                // where each index indicates a type of light, see LightType definition.
                const LightType selectedType{ static_cast<LightType>( lightTypeIndex ) };

                // Create a selectable combo item for each light type
                if ( ImGui::Selectable( currentType.data(), isSelected ) ) {
                    // Update the type of light for this component
                    lightComponent.SetType( selectedType );
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

        switch ( lightComponent.GetType() ) {
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

    static auto SetupTextComponentTab(Entity& entity) -> void {
        TextComponent& textComponent{ entity.GetComponent<TextComponent>() };

        std::string content( '0', 4096 );

        std::ranges::copy(textComponent.GetTextContent(), content.begin());

        if (ImGuiUtils::TextArea( content )) {
            textComponent.SetTextContent( content );
        }
    }

    static auto SetupAudioComponentTab( Entity& entity ) -> void {
        AudioComponent& audioComponent{ entity.GetComponent<AudioComponent>() };

        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp };

        if ( ImGui::BeginTable( "MaterialEditorDiffusePropertiesTable", 2, tableFlags ) ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Audio clip" );

            ImGui::TableSetColumnIndex( 1 );

            std::string path{ audioComponent.GetSourcePath().string() };
            ImGui::InputText( "##AudioClipSource", path.data(), path.size(), ImGuiInputTextFlags_ReadOnly );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Muted" );

            ImGui::TableSetColumnIndex( 1 );
            bool isMuted{ audioComponent.IsMuted() };
            if ( ImGui::Checkbox( "##IsMutedAudio", std::addressof( isMuted ) ) ) {
                audioComponent.Mute( isMuted );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Loop" );

            ImGui::TableSetColumnIndex( 1 );
            bool isLooping{ audioComponent.IsLooping() };
            if ( ImGui::Checkbox( "##IsLoopingAudio", std::addressof( isLooping ) ) ) {
                audioComponent.SetLooping( isLooping );
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto SetupCameraComponentTab( Entity& entity ) -> void {
        CameraComponent& cameraComponent{ entity.GetComponent<CameraComponent>() };

        static const std::array<std::string, 2> CAMERA_PROJECTION_TYPE_NAMES{
            "Orthographic", "Perspective"
        };

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
            const auto& currentProjectionTypeStr{ CAMERA_PROJECTION_TYPE_NAMES[cameraCurrentProjectionType] };

            if ( ImGui::BeginCombo( "##Projection",
                                    currentProjectionTypeStr.c_str() ) ) {
                UInt32_T projectionIndex{};
                for ( const auto& projectionType: CAMERA_PROJECTION_TYPE_NAMES ) {
                    // Tells whether we want to highlight this projection in the ImGui combo.
                    // This will be the case if this projection type is the current one for this camera.
                    bool isSelected{ projectionType == CAMERA_PROJECTION_TYPE_NAMES[cameraCurrentProjectionType] };

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

        if ( sceneCamera.GetProjectionType() == PERSPECTIVE ) {

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

    static constexpr auto GetInspectorPanelName() -> std::string_view {
        return "Inspector";
    }

    InspectorPanel::InspectorPanel( const InspectorPanelCreateInfo& createInfo )
        : Panel{ StringUtils::MakePanelName( ICON_MD_ERROR_OUTLINE, GetInspectorPanelName() ) },
          m_TargetScene{ createInfo.TargetScene },
          m_GetActiveEntityCallback{ createInfo.GetActiveEntityCallback },
          m_SetActiveEntityCallback{ createInfo.SetActiveEntityCallback }
    {}

    auto InspectorPanel::DrawComponents( Entity& entity ) const -> void {
        if ( !entity.IsValid() ) {
            return;
        }

        DrawComponent<TransformComponent>( fmt::format( "{} Transform", ICON_MD_DEVICE_HUB ), entity, [&]( Entity& target ) -> void { SetupTransformComponentTab( target, m_TargetScene ); }, false );

        DrawComponent<MaterialComponent>( fmt::format( "{} Material", ICON_MD_INSIGHTS ), entity, SetupMaterialComponentTab );

        DrawComponent<PhysicsComponent>( fmt::format( "{} Physics", ICON_MD_FITNESS_CENTER ), entity, SetupPhysicsComponentTab );

        DrawComponent<RenderComponent>( fmt::format( "{} Mesh", ICON_MD_VIEW_IN_AR ), entity, [&]( Entity& target ) -> void { SetupRenderComponentTab( target, m_TargetScene ); }, false );

        DrawComponent<LightComponent>( fmt::format( "{} Light", ICON_MD_LIGHT ), entity, SetupLightComponentTab );

        DrawComponent<AudioComponent>( fmt::format( "{} Audio", ICON_MD_AUDIOTRACK ), entity, SetupAudioComponentTab );

        DrawComponent<TextComponent>( fmt::format( "{} Text", ICON_MD_MESSAGE ), entity, SetupTextComponentTab );

        DrawComponent<CameraComponent>( fmt::format( "{} Camera", ICON_MD_CAMERA_ALT ), entity, SetupCameraComponentTab );

        DrawComponent<NativeScriptComponent>( fmt::format( "{} Script", ICON_MD_CODE ), entity, SetupNativeScriptingComponentTab );
    }

    auto InspectorPanel::OnUpdate( MKT_UNUSED_VAR float timeStep ) -> void {
        if ( m_PanelIsVisible ) {
            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

            Entity* target{ m_GetActiveEntityCallback() };

            if ( target != nullptr ) {
                DrawVisibilityCheckBox( *target );

                ImGui::SameLine();

                DrawNameTextInput( *target );
                DrawComponentButton( *target );

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                DrawComponents( *target );
            }

            ImGui::End();
        }
    }
}// namespace Mikoto