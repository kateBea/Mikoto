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
#include <GUI/IconsMaterialDesign.h>
#include <imgui_impl_vulkan.h>

#include <Common/Common.hh>
#include <Core/System/AssetsSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Core/System/RenderSystem.hh>
#include <GUI/ImGuiManager.hh>
#include <GUI/ImGuiUtils.hh>
#include <Library/Filesystem/PathBuilder.hh>
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

                ImGuiManager::AddShutdownCallback( [textureDset = itInsert->second]() -> void {
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

    static auto ShowTextureHoverTooltip( Texture2D* texture ) -> void {
        if ( ImGui::IsItemHovered() &&
             ImGui::BeginTooltip() /** && has albedo map, otherwise it display info about the */ ) {

            ImGuiUtils::PushImageButton( texture->GetID().Get(), GetDescriptorSetById( texture ), ImVec2{ 128, 128 } );

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
                ImGui::TextUnformatted( fmt::format( "{:.2f} MB", 0.0f /* TODO */ ).c_str() );

                ImGui::EndTable();
            }

            ImGui::EndTooltip();
        }
        if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }
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
            ImGuiUtils::PushImageButton( diffuseMap->GetID().Get(), GetDescriptorSetById( diffuseMap ), ImVec2{ 64, 64 } );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            if ( standardMat.HasDiffuseMap() ) {
                ShowTextureHoverTooltip( diffuseMap );
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

            ImGuiUtils::PushImageButton( specularMap->GetID().Get(), GetDescriptorSetById( specularMap ), ImVec2{ 64, 64 } );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            if ( standardMat.HasSpecularMap() ) {
                ShowTextureHoverTooltip( specularMap );
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

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
        }
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
                entity.AddComponent<NativeScriptComponent>();
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

        ImGuiUtils::PushImageButton( map->GetID().Get(), GetDescriptorSetById( map ), ImVec2{ 64, 64 } );

        if ( ImGui::IsItemHovered() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
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
            ImGui::TextUnformatted( fmt::format( "{:.2f} MB", 0.0f /* TODO:*/ ).c_str() );

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

    static auto DrawVec3Transform( const std::string_view label, glm::vec3& data, const double resetValue = 0.0, const double columWidth = 100.0 ) -> void {
        // This Group is part of a unique label
        const std::string labelId{ fmt::format( "{}:{}", MKT_STRINGIFY( DrawVec3Transform ), label.data() ) };

        ImGui::PushID( labelId.data() );

        ImGui::Columns( 2 );
        ImGui::SetColumnWidth( 0, static_cast<float>( columWidth ) );
        ImGui::Text( "%s", label.data() );
        ImGui::NextColumn();
        ImGui::PushMultiItemsWidths( 3, ImGui::CalcItemWidth() );
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2{ 7.0f, 5.0f } );

        const float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f };
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

        ImGui::PopStyleVar();

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

        glm::vec3 translation{ transformComponent.GetTranslation() };
        glm::vec3 rotation{ transformComponent.GetRotation() };
        glm::vec3 scale{ transformComponent.GetScale() };

        const glm::vec3 oldTranslation{ transformComponent.GetTranslation() };
        const glm::vec3 oldScale{ transformComponent.GetScale() };
        const glm::vec3 oldRotation{ transformComponent.GetRotation() };

        ImGui::Spacing();

        DrawVec3Transform( "Translation", translation );
        DrawVec3Transform( "Rotation", rotation );
        DrawVec3Transform( "Scale", scale, 1.0 );

        transformComponent.SetTranslation( translation );
        transformComponent.SetRotation( rotation );
        transformComponent.SetScale( scale );

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

            EditStandardMaterial( dynamic_cast<StandardMaterial&>( material ) );
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

        Path_T path{ "Empty" };

        const Mesh* mesh{ component.GetMesh() };

        if ( mesh != nullptr ) {
            path = mesh->GetDirectory().string();
        }

        std::string formatedPath{ fmt::format( "{}", path.string() ) };
        ImGui::InputText( "##PathToModel", formatedPath.data(), formatedPath.size(), ImGuiInputTextFlags_ReadOnly );

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
                    .Name = component.GetName(),
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
            constexpr ImGuiColorEditFlags colorEditFlags{
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview
            };

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
            if ( ImGui::DragFloat4( "##DirectionalLightDirection", glm::value_ptr( direction ), 0.1f, 0.0f, 512.0f, "%.2f" ) ) {
                lightComponent.GetDirLightData().Direction = direction;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Ambient" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 ambient{ lightComponent.GetDirLightData().Ambient };
            if ( ImGui::ColorEdit4( "##DirectionalLightAmbient", glm::value_ptr( ambient ), colorEditFlags ) ) {
                lightComponent.GetDirLightData().Ambient = ambient;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Diffuse" );

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 diffuse{ lightComponent.GetDirLightData().Diffuse };
            if ( ImGui::ColorEdit4( "##DirectionalLightDiffuse", glm::value_ptr( diffuse ), colorEditFlags ) ) {
                lightComponent.GetDirLightData().Diffuse = diffuse;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Specular" );

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 specular{ lightComponent.GetDirLightData().Specular };
            if ( ImGui::ColorEdit4( "##DirectionalLightSpecular", glm::value_ptr( specular ), colorEditFlags ) ) {
                lightComponent.GetDirLightData().Specular = specular;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Cast shadows" );

            ImGui::TableSetColumnIndex( 1 );
            static bool castShadows{};
            ImGui::Checkbox( "##DirectionalLightSahdows", std::addressof( castShadows ) );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto SetupPointLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "PointLightMainTable", 2, tableFlags ) ) {
            auto& pointLightData{ lightComponent.GetPointLightData() };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Ambient" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );
            static constexpr ImGuiColorEditFlags colorEditFlags{
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview
            };

            glm::vec4 ambientComponent{ pointLightData.Ambient };
            if ( ImGui::ColorEdit3( "##PointAmbientComponent", glm::value_ptr( ambientComponent ),
                                    colorEditFlags ) ) {
                pointLightData.Ambient = ambientComponent;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Diffuse" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 diffuseComponent{ pointLightData.Diffuse };
            if ( ImGui::ColorEdit3( "##PointDiffuseComponent", glm::value_ptr( diffuseComponent ),
                                    colorEditFlags ) ) {
                pointLightData.Diffuse = diffuseComponent;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Specular" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 specularComponent{ pointLightData.Specular };
            if ( ImGui::ColorEdit3( "##PointSpecularComponent", glm::value_ptr( specularComponent ),
                                    colorEditFlags ) ) {
                pointLightData.Specular = specularComponent;
            }


            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Constant" );
            if ( ImGui::IsItemHovered() ) { ImGui::SetMouseCursor( ImGuiMouseCursor_Hand ); }

            ImGui::TableSetColumnIndex( 1 );

            float constant{ pointLightData.Components.x };

            if ( ImGui::SliderFloat( "##PointConstantComponent", std::addressof( constant ), 0.1f, 1.0f ) ) {
                pointLightData.Components.x = constant;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Linear" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );

            float linear{ pointLightData.Components.y };

            if ( ImGui::SliderFloat( "##PointLinearComponent", std::addressof( linear ), 0.0f, 2.0f ) ) {
                pointLightData.Components.y = linear;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Quadratic" );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );

            float quadratic{ pointLightData.Components.z };

            if ( ImGui::SliderFloat( "##PointQuadraticComponent", std::addressof( quadratic ), 0.0f, 1.0f ) ) {
                pointLightData.Components.z = quadratic;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::TableSetColumnIndex( 1 );

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Cast shadows" );

            static bool castShadows{};
            ImGui::TableSetColumnIndex( 1 );
            ImGui::Checkbox( "##PointLightSahdows", std::addressof( castShadows ) );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::EndTable();
        }
    }

    static auto SetupSpotLightLightOptions( LightComponent& lightComponent ) -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

        if ( ImGui::BeginTable( "SpotLightEditTable", 2, tableFlags ) ) {
            auto& spotLightData{ lightComponent.GetSpotLightData() };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Direction" );

            ImGui::SameLine();

            ImGuiUtils::HelpMarker( "The spot position is determined by the objects position." );

            ImGui::TableSetColumnIndex( 1 );

            glm::vec4 direction{ spotLightData.Direction };
            if ( ImGui::DragFloat3( "##SpotLightDirection", glm::value_ptr( direction ), 0.01f, -1.0f, 1.0f,
                                    "%.2f" ) ) {
                spotLightData.Direction = direction;
            }

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            // Constants
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Constant" );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableSetColumnIndex( 1 );

                float constant{ spotLightData.Components.x };

                if ( ImGui::SliderFloat( "##SpotConstantComponent", std::addressof( constant ), 0.1f, 1.0f,
                                         "%.6f" ) ) {
                    spotLightData.Components.x = constant;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Linear" );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableSetColumnIndex( 1 );

                float linear{ spotLightData.Components.y };
                if ( ImGui::SliderFloat( "##SpotLinearComponent", std::addressof( linear ), 0.0014f, 1.8f,
                                         "%.6f" ) ) {
                    spotLightData.Components.y = linear;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Quadratic" );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableSetColumnIndex( 1 );

                float quadratic{ spotLightData.Components.z };

                if ( ImGui::SliderFloat( "##SpotQuadraticComponent", std::addressof( quadratic ), 0.000007f, 1.8f,
                                         "%.6f" ) ) {
                    spotLightData.Components.z = quadratic;
                }
            }

            // Components
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Ambient" );

                ImGui::TableSetColumnIndex( 1 );
                static constexpr ImGuiColorEditFlags colorEditFlags{
                    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview
                };

                glm::vec4 ambientComponent{ spotLightData.Ambient };
                if ( ImGui::ColorEdit3( "##SpotAmbientComponent", glm::value_ptr( ambientComponent ),
                                        colorEditFlags ) ) {
                    spotLightData.Ambient = ambientComponent;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Diffuse" );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableSetColumnIndex( 1 );

                glm::vec4 diffuseComponent{ spotLightData.Diffuse };
                if ( ImGui::ColorEdit3( "##SpotDiffuseComponent", glm::value_ptr( diffuseComponent ),
                                        colorEditFlags ) ) {
                    spotLightData.Diffuse = diffuseComponent;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Specular" );

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableSetColumnIndex( 1 );

                glm::vec4 specularComponent{ spotLightData.Specular };
                if ( ImGui::ColorEdit3( "##SpotSpecularComponent", glm::value_ptr( specularComponent ),
                                        colorEditFlags ) ) {
                    spotLightData.Specular = specularComponent;
                }
            }

            // angles
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Cut-off" );

                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Angles in degrees" );

                ImGui::TableSetColumnIndex( 1 );
                float cutOff{ glm::degrees( spotLightData.CutOffValues.x ) };
                if ( ImGui::SliderFloat( "##SpotLightCutoff", std::addressof( cutOff ), 0.0f, 360.0f, "%.1f" ) ) {
                    spotLightData.CutOffValues.x = glm::radians( cutOff );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex( 0 );
                ImGui::TextUnformatted( "Outer cut-off" );

                ImGui::SameLine();
                ImGuiUtils::HelpMarker( "Angles in degrees" );

                ImGui::TableSetColumnIndex( 1 );
                float outerCutOff{ glm::degrees( spotLightData.CutOffValues.y ) };
                if ( ImGui::SliderFloat( "##SpotLightOuterCutOff", std::addressof( outerCutOff ), 0.0f, 360.0f,
                                         "%.1f" ) ) {
                    spotLightData.CutOffValues.y = glm::radians( outerCutOff );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::TextUnformatted( "Cast shadows" );

            static bool castShadows{};
            ImGui::TableSetColumnIndex( 1 );
            ImGui::Checkbox( "##SpotLightSahdows", std::addressof( castShadows ) );

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

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

                //component.GetCameraPtr()->SetPerspective(nearPlane, farPlane, fov);

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