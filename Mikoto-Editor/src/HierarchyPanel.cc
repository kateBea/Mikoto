/**
 * HierarchyPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/StringUtils.hh>

#include <Core/Logger.hh>

#include <GUI/IconsMaterialDesign.h>

#include <Panels/HierarchyPanel.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>
#include <Scene/SceneManager.hh>

namespace Mikoto {
    static constexpr auto GetHierarchyName() -> std::string_view {
        return "Hierarchy";
    }


    HierarchyPanel::HierarchyPanel()
        :   Panel{}
    {
        m_PanelHeaderName = StringUtils::MakePanelName( ICON_MD_MERGE, GetHierarchyName() );
    }


    auto HierarchyPanel::OnUpdate( MKT_UNUSED_VAR float ts ) -> void {
        if ( m_PanelIsVisible ) {
            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ) );

            m_PanelIsHovered = ImGui::IsWindowHovered();
            m_PanelIsFocused = ImGui::IsWindowFocused();

            SceneManager::ForEachWithComponents<TagComponent>(
                    []( Entity& entity ) -> void {
                        DrawEntityNode( entity );
                        OnEntityRightClickMenu( entity );
                    } );

            if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() ) {
                SceneManager::DisableActiveSelection();
            }

            BlankSpacePopupMenu();

            ImGui::End();
        }
    }


    auto HierarchyPanel::DrawEntityNode( Entity& target ) -> void {
        TagComponent& tag{ target.GetComponent<TagComponent>() };
        auto ent{ SceneManager::GetCurrentSelection() };

        bool thisEntityIsSelected{ ent.has_value() && ent->get() == target };
        static constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_AllowItemOverlap |
                                                        ImGuiTreeNodeFlags_Framed |
                                                        ImGuiTreeNodeFlags_SpanAvailWidth |
                                                        ImGuiTreeNodeFlags_FramePadding };

        const ImGuiTreeNodeFlags flags{ styleFlags | ( thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : 0 ) };
        constexpr ImGuiTreeNodeFlags childNodeFlags{ styleFlags | ImGuiTreeNodeFlags_DefaultOpen };

        bool expanded{ ImGui::TreeNodeEx( ( void* )( target.m_EntityHandle ), flags, "%s", fmt::format( " {} {}", ICON_MD_WIDGETS, tag.GetTag() ).c_str() ) };

        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) {
            SceneManager::SetCurrentSelection(target);
        }

        if ( expanded ) {
            // Temporary just to test nested nodes
            if ( ImGui::TreeNodeEx( ( void* )83124423, childNodeFlags, "%s", "Entity item" ) ) {

                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }


    auto HierarchyPanel::OnEntityRightClickMenu( Entity& target ) -> void {
        static constexpr ImGuiPopupFlags popupItemFlags{ ImGuiPopupFlags_MouseButtonRight };

        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if ( ImGui::BeginPopupContextItem( nullptr, popupItemFlags ) ) {
            if ( ImGui::BeginMenu( "Add component" ) ) {
                constexpr bool menuItemSelected{ false };         // these menu items should not display as selected
                constexpr const char* menuItemShortcut{ nullptr };// no shortcuts for now

                if ( ImGui::MenuItem( "Material", menuItemShortcut, menuItemSelected, !target.HasComponent<MaterialComponent>() ) ) {
                    target.AddComponent<MaterialComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Camera", menuItemShortcut, menuItemSelected, !target.HasComponent<CameraComponent>() ) ) {
                    target.AddComponent<CameraComponent>( std::make_shared<SceneCamera>() );
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Script", menuItemShortcut, menuItemSelected, !target.HasComponent<NativeScriptComponent>() ) ) {
                    target.AddComponent<NativeScriptComponent>();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if ( ImGui::MenuItem( "Remove object" ) ) {
                SceneManager::DestroyEntity(SceneManager::GetActiveScene(), target);
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
    }


    auto HierarchyPanel::BlankSpacePopupMenu() -> void {
        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if ( ImGui::BeginPopupContextWindow( "##HierarchyMenuOptions", popupWindowFlags ) ) {
            EntityCreateInfo entityCreateInfo{};

            if ( ImGui::MenuItem( "Empty Object" ) ) {
                entityCreateInfo.Name = "Empty Object";
                entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;
                SceneManager::AddEntity( entityCreateInfo );

                MKT_CORE_LOGGER_INFO( "Added new empty object" );
            }

            if ( ImGui::BeginMenu( "3D Object" ) ) {
                if ( ImGui::MenuItem( "Cube" ) ) {
                    entityCreateInfo.Name = "Cube Object";
                    entityCreateInfo.PrefabType = PrefabSceneObject::CUBE_PREFAB_OBJECT;
                    SceneManager::AddEntity( entityCreateInfo );

                    MKT_CORE_LOGGER_INFO( "Added new cube prefab" );
                }

                if ( ImGui::MenuItem( "Sprite" ) ) {
                    entityCreateInfo.Name = "Sprite Object";
                    entityCreateInfo.PrefabType = PrefabSceneObject::SPRITE_PREFAB_OBJECT;
                    SceneManager::AddEntity( entityCreateInfo );

                    MKT_CORE_LOGGER_INFO( "Added new sprite prefab" );
                }

                if ( ImGui::MenuItem( "Cone" ) ) {
                    entityCreateInfo.Name = "Cone Object";
                    entityCreateInfo.PrefabType = PrefabSceneObject::CONE_PREFAB_OBJECT;
                    SceneManager::AddEntity( entityCreateInfo );

                    MKT_CORE_LOGGER_INFO( "Added new cone prefab" );
                }

                if ( ImGui::MenuItem( "Cylinder" ) ) {
                    entityCreateInfo.Name = "Cylinder Object";
                    entityCreateInfo.PrefabType = PrefabSceneObject::CYLINDER_PREFAB_OBJECT;
                    SceneManager::AddEntity( entityCreateInfo );

                    MKT_CORE_LOGGER_INFO( "Added new cylinder prefab" );
                }

                if ( ImGui::MenuItem( "Sphere" ) ) {
                    entityCreateInfo.Name = "Sphere Object";
                    entityCreateInfo.PrefabType = PrefabSceneObject::SPHERE_PREFAB_OBJECT;
                    SceneManager::AddEntity( entityCreateInfo );

                    MKT_CORE_LOGGER_INFO( "Added new sphere prefab" );
                }

                if ( ImGui::MenuItem( "Sponza" ) ) {
                    entityCreateInfo.Name = "Sponza Object";
                    entityCreateInfo.PrefabType = PrefabSceneObject::SPONZA_PREFAB_OBJECT;
                    SceneManager::AddEntity( entityCreateInfo );

                    MKT_CORE_LOGGER_INFO( "Added new sponza prefab" );
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
    }
}