//    Copyright 2025 ケイト
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

#include <memory>

#include <imgui.h>

#include <ImGui/IconsMaterialDesign.h>

#include <Common/Common.hh>
#include <Common/String.hh>
#include <Core/Profiler.hh>
#include <Application/EditorApp.hh>
#include <Application/EditorUtility.hh>
#include <Assets/AssetsService.hh>
#include <Core/RuntimeConsole.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>
#include <Panels/HierarchyPanel.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

#include "ImGui/IconsMaterialDesignIcons.h"

namespace Mikoto {

    auto HierarchyPanel::DrawPrefabMenu( Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( ImGui::BeginMenu( "3D Object" ) ) {

            if ( ImGui::MenuItem( "Cube" ) ) {
                AddEntityWithModel( EditorApp::GetPrefabUri( PrefabModels::CUBE ), root );
            }

            if ( ImGui::MenuItem( "Cone" ) ) {
                AddEntityWithModel( EditorApp::GetPrefabUri( PrefabModels::CONE ), root );
            }

            if ( ImGui::MenuItem( "Cylinder" ) ) {
                AddEntityWithModel( EditorApp::GetPrefabUri( PrefabModels::CYLINDER ), root );
            }

            if ( ImGui::MenuItem( "Sphere" ) ) {
                AddEntityWithModel( EditorApp::GetPrefabUri( PrefabModels::SPHERE ), root );
            }

            if ( ImGui::MenuItem( "Sponza" ) ) {
                AddEntityWithModel( EditorApp::GetPrefabUri( PrefabModels::SPONZA ), root );
            }

            ImGui::EndMenu();
        }
    }

    auto HierarchyPanel::DrawLightMenuItems( Entity* root ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        EntityCreateInfo entityCreateInfo{
            .Root{ root },
            .IsLight{ true }
        };

        ImGui::Spacing();
        ImGui::Separator();

        if ( ImGui::MenuItem( "Sky Light" ) ) {
            entityCreateInfo.Name = "Sky Light";
            entityCreateInfo.TypeLight = LightType::DIRECTIONAL_LIGHT_TYPE;
        }

        if ( ImGui::MenuItem( "Directional light" ) ) {
            entityCreateInfo.Name = "Directional light";
            entityCreateInfo.TypeLight = LightType::DIRECTIONAL_LIGHT_TYPE;
        }

        if ( ImGui::MenuItem( "Point light" ) ) {
            entityCreateInfo.Name = "Point light";
            entityCreateInfo.TypeLight = LightType::POINT_LIGHT_TYPE;
        }

        if ( ImGui::MenuItem( "Spot light" ) ) {
            entityCreateInfo.Name = "Spot light";
            entityCreateInfo.TypeLight = LightType::SPOT_LIGHT_TYPE;
        }

        if ( !entityCreateInfo.Name.empty() ) {
            m_EditorState->ActiveEditorScene->QueueCreateEntity( entityCreateInfo );
            RuntimeConsole::Get()->Debug( StringUtil::Format( "Queued create light: {}", entityCreateInfo.Name ) );
        }
    }

    HierarchyPanel::HierarchyPanel( const HierarchyPanelCreateInfo& createInfo )
        : Panel{ "Hierarchy",  },
          m_EditorState{ createInfo.State } {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_MERGE, m_PanelName );
    }

    auto HierarchyPanel::OnUpdate( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_PanelIsVisible ) {
            return;
        }

        ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

        m_PanelIsHovered = ImGui::IsWindowHovered();
        m_PanelIsFocused = ImGui::IsWindowFocused();

        auto& entityList{ m_EditorState->ActiveEditorScene->GetEntities() };

        // FIXME: segfault if we insert new entity as child for this one
        for ( auto& [entityID, entity]: entityList ) {
            const RelationComponent& relation{ entity->GetComponent<RelationComponent>() };

            // only root entities are shown at upper level
            if ( !relation.HasParent() ) {
                DrawNodeTree( entityID );
            }
        }

        if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() ) {
            m_EditorState->RemoveSingleSelection();
        }

        BlankSpacePopupMenu();

        ImGui::End();
    }


    auto HierarchyPanel::DrawNodeTree( const UInt64 entityID ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        Entity* entity{ m_EditorState->ActiveEditorScene->FindByID( entityID ) };
        if ( entity == nullptr ) {
            return;
        }

        Entity* currentSelection{ m_EditorState->SelectedEntity };

        const TagComponent& entityTag{ entity->GetComponent<TagComponent>() };
        const RelationComponent& entityRelation{ entity->GetComponent<RelationComponent>() };

        const auto thisEntityIsSelected{ currentSelection != nullptr && entityID == currentSelection->GetComponent<TagComponent>().GetGUID() };

        const ImGuiTreeNodeFlags styleFlags{
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_FramePadding |
            ( entityRelation.IsLeaf() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None ) |
            ( thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None )
        };

        const ImGuiTreeNodeFlags flags{ styleFlags | ( thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None ) };

        // Icons for ICON_MD for assets U+F1B2, U+F1B3, U+F6D1
        // TODO: find the actual ICON_MD macros
        // U+F6D1  ->  63185
        // U+F1B2  ->  61874
        // U+F1B3  ->  61875
        const std::string icon { ImGuiUtils::GetStringFromUnicode( 63185 ) };

        const bool expanded{ ImGui::TreeNodeEx( reinterpret_cast<void*>( entityTag.GetGUID() ), 
            flags, "%s", fmt::format( " {} {}",  icon.data(), entityTag.GetTag() ).c_str() ) };

        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) {
            m_EditorState->SelectedEntity = entity;
        }

        OnEntityRightClickMenu( entity );

        if ( expanded ) {
            for ( auto& childID: entityRelation.GetChildren() ) {
                DrawNodeTree( childID );
            }

            ImGui::TreePop();
        }
    }


    auto HierarchyPanel::OnEntityRightClickMenu( Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::BeginPopupContextItem( nullptr, popupWindowFlags ) ) {
            if ( ImGui::BeginMenu( "Add component" ) ) {
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
                    entity->AddComponent<CameraComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Lighting", menuItemShortcut, menuItemSelected, !IsPresent<LightComponent>( entity ) ) ) {
                    entity->AddComponent<LightComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Rigid Body", menuItemShortcut, menuItemSelected, !IsPresent<RigidBodyComponent>( entity ) ) ) {
                    entity->AddComponent<RigidBodyComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Audio source", menuItemShortcut, menuItemSelected, !IsPresent<AudioSourceComponent>( entity ) ) ) {
                    entity->AddComponent<AudioSourceComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Audio listener", menuItemShortcut, menuItemSelected, !IsPresent<AudioListenerComponent>( entity ) ) ) {
                    entity->AddComponent<AudioListenerComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Text", menuItemShortcut, menuItemSelected, !IsPresent<TextComponent>( entity ) ) ) {
                    TextComponent& textComponent{ entity->AddComponent<TextComponent>() };

                    // TODO: Load font logic

                    textComponent.SetSize( 12 );
                    textComponent.SetContents( "Example" );
                    textComponent.SetSpacing( 1 );

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if ( ImGui::MenuItem( "Remove object" ) ) {
                m_EditorState->ActiveEditorScene->RemoveEntity( entity->GetComponent<TagComponent>().GetGUID() );
                m_EditorState->RemoveSingleSelection();

                RuntimeConsole::Get()->Debug( fmt::format( "Removing entity: {}", entity->GetComponent<TagComponent>().GetTag() ) );
            }

            if ( ImGui::MenuItem( "Create empty object" ) ) {
                EntityCreateInfo createInfo{
                    .Root{ entity },
                    .Name{ "Empty object" },
                };

                m_EditorState->ActiveEditorScene->QueueCreateEntity( createInfo );
                RuntimeConsole::Get()->Debug( StringUtil::Format( "Queued create Empty object" ) );
            }

            DrawPrefabMenu( entity );
            DrawModelLoadMenu( entity );
            DrawLightMenuItems( entity );
            DrawTextMenu( entity );

            ImGui::EndPopup();
        }
    }

    auto HierarchyPanel::DrawModelLoadMenu( Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( ImGui::MenuItem( "Load model" ) ) {
            AddEntityWithModel( root );
        }
    }

    auto HierarchyPanel::DrawTextMenu( Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGui::Spacing();
        ImGui::Separator();

        if ( ImGui::MenuItem( "Text" ) ) {
            const EntityCreateInfo entityCreateInfo{
                .Root{ root },
                .Name{ "Text" },
                .IsText{ true },
                .IsWorldText{ false },
                .TextSize{ TextComponent::GetMinLetterSize() },
                .TextSpacing{ TextComponent::GetMinLetterSpacing() },
                .InitialContents{ "Text" },
            };

            m_EditorState->ActiveEditorScene->QueueCreateEntity( entityCreateInfo );
            RuntimeConsole::Get()->Debug( StringUtil::Format( "Queued create entity {}", entityCreateInfo.Name ) );
        }

        if ( ImGui::MenuItem( "Text 3D" ) ) {
            const EntityCreateInfo entityCreateInfo{
                .Root{ root },
                .Name{ "Text" },
                .IsText{ true },
                .IsWorldText{ true },
                .TextSize{ TextComponent::GetMinLetterSize() },
                .TextSpacing{ TextComponent::GetMinLetterSpacing() },
                .InitialContents{ "Text" },
            };

            m_EditorState->ActiveEditorScene->QueueCreateEntity( entityCreateInfo );
            RuntimeConsole::Get()->Debug( StringUtil::Format( "Queued create entity {}", entityCreateInfo.Name ) );
        }
    }

    auto HierarchyPanel::AddEntityWithModel( const std::string_view uri, Entity* root ) -> void {
        // See comment in AddEntityWithModel( Entity* root )
        static std::atomic_bool loading{ false };

        if ( !loading ) {
            loading = true;
            TaskService::Get()->Submit( [this, root, path = std::string{ uri }]() -> void {

                ModelLoadDescription description{
                    .ModelFile{ FileService::Get()->LoadFile( path ) },
                    .WantTextures{ true }
                };

                const std::string name{ Path{ path }.stem().string() };
                const ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( description ) };

                const EntityCreateInfo entityCreateInfo{
                    .Root{ root },
                    .Name =  name,
                    .Model = model,
                };

                m_EditorState->ActiveEditorScene->QueueCreateEntity( entityCreateInfo );

                loading = false;
            } );
        }
    }

    auto HierarchyPanel::AddEntityWithModel( Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // TODO: Use the asset cache service to load asynchronously and the asset cache to avoid loading the same model multiple times
        // and be notified when the model is loaded to create the entity with it, instead of blocking the main thread
        static std::atomic_bool loading{ false };

        if ( !loading ) {
            loading = true;

            TaskService::Get()->Submit( [this, rootEntity = root]() -> void {
                const std::initializer_list<std::pair<std::string, std::string>> filters{
                    { "Model files", "obj,gltf,fbx,glb" },
                    { "OBJ files", "obj" },
                    { "glTF files", "gltf" },
                    { "FBX files", "fbx" },
                    { "GLB files", "glb" },
                };

                const std::string path{ FileService::Get()->OpenDialog( filters ).string() };
                if (!path.empty()) {
                    AddEntityWithModel(path, rootEntity);
                }

                loading = false;
            });
        }
    }

    auto HierarchyPanel::BlankSpacePopupMenu() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::BeginPopupContextWindow( "##HierarchyPanel::BlankSpacePopupMenu:HierarchyMenuOptions", popupWindowFlags ) ) {

            if ( ImGui::MenuItem( "Empty Object" ) ) {
                m_EditorState->ActiveEditorScene->QueueCreateEntity( "Empty Object" );
                RuntimeConsole::Get()->Debug( fmt::format( "New entity queued {}", "Empty Object" ) );
            }

            DrawPrefabMenu();
            DrawModelLoadMenu();
            DrawLightMenuItems();
            DrawTextMenu();

            ImGui::EndPopup();
        }
    }
}// namespace Mikoto
