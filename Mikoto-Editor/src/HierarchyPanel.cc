/**
 * HierarchyPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <GUI/IconsMaterialDesign.h>

#include <Common/Common.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/AssetsSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>
#include <Panels/HierarchyPanel.hh>
#include <Scene/Scene/Scene.hh>

namespace Mikoto {
    static constexpr auto GetHierarchyName() -> std::string_view {
        return "Hierarchy";
    }

    auto HierarchyPanel::DrawPrefabMenu( const Entity* root ) const -> void {
        EntityCreateInfo entityCreateInfo{
            .Name{},
            .Root{ root },
            .ModelMesh{ nullptr }
        };

        entityCreateInfo.Root = root;

        if ( ImGui::BeginMenu( "3D Object" ) ) {
            AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };
            FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };


            Entity* newEntity{ nullptr };

            if ( ImGui::MenuItem( "Cube" ) ) {
                entityCreateInfo.Name = "Cube";
                entityCreateInfo.ModelMesh = assetsSystem.GetModel( PathBuilder()
                    .WithPath( fileSystem.GetAssetsRootPath().string() )
                    .WithPath( "Prefabs" )
                    .WithPath( "cube" )
                    .WithPath( "gltf" )
                    .WithPath( "scene.gltf" )
                    .Build().string());

                newEntity = m_TargetScene->CreateEntity( entityCreateInfo );
            }

            if ( ImGui::MenuItem( "Cone" ) ) {
                entityCreateInfo.Name = "Cone";
                entityCreateInfo.ModelMesh = assetsSystem.GetModel( PathBuilder()
                    .WithPath( fileSystem.GetAssetsRootPath().string() )
                    .WithPath( "Prefabs" )
                    .WithPath( "cone" )
                    .WithPath( "gltf" )
                    .WithPath( "scene.gltf" )
                    .Build().string());

                newEntity = m_TargetScene->CreateEntity( entityCreateInfo );
            }

            if ( ImGui::MenuItem( "Cylinder" ) ) {
                entityCreateInfo.Name = "Cylinder";
                entityCreateInfo.ModelMesh = assetsSystem.GetModel( PathBuilder()
                    .WithPath( fileSystem.GetAssetsRootPath().string() )
                    .WithPath( "Prefabs" )
                    .WithPath( "cylinder" )
                    .WithPath( "gltf" )
                    .WithPath( "scene.gltf" )
                    .Build().string());

                newEntity = m_TargetScene->CreateEntity( entityCreateInfo );
            }

            if ( ImGui::MenuItem( "Sphere" ) ) {
                entityCreateInfo.Name = "Sphere";
                entityCreateInfo.ModelMesh = assetsSystem.GetModel( PathBuilder()
                    .WithPath( fileSystem.GetAssetsRootPath().string() )
                    .WithPath( "Prefabs" )
                    .WithPath( "sphere" )
                    .WithPath( "gltf" )
                    .WithPath( "scene.gltf" )
                    .Build().string());

                newEntity = m_TargetScene->CreateEntity( entityCreateInfo );

            }

            if ( ImGui::MenuItem( "Sponza" ) ) {
                entityCreateInfo.Name = "Sponza";
                entityCreateInfo.ModelMesh = assetsSystem.GetModel( PathBuilder()
                    .WithPath( fileSystem.GetAssetsRootPath().string() )
                    .WithPath( "Prefabs" )
                    .WithPath( "sponza" )
                    .WithPath( "glTF" )
                    .WithPath( "Sponza.gltf" )
                    .Build().string());

                newEntity = m_TargetScene->CreateEntity( entityCreateInfo );
            }

            if (newEntity != nullptr) {
                MKT_APP_LOGGER_INFO( "Created new entity: {}", newEntity->GetComponent<TagComponent>().GetTag() );
            }

            ImGui::EndMenu();
        }
    }

    HierarchyPanel::HierarchyPanel(const HierarchyPanelCreateInfo& createInfo)
        :   Panel{ StringUtils::MakePanelName( ICON_MD_MERGE, GetHierarchyName() ) },
            m_TargetScene{ createInfo.TargetScene },
            m_GetActiveEntityCallback{ createInfo.GetActiveEntityCallback },
            m_SetActiveEntityCallback{ createInfo.SetActiveEntityCallback }
    {}

    auto HierarchyPanel::OnUpdate( MKT_UNUSED_VAR float ts ) -> void {
        if ( m_PanelIsVisible ) {
            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

            m_PanelIsHovered = ImGui::IsWindowHovered();
            m_PanelIsFocused = ImGui::IsWindowFocused();

            auto& hierarchy{ m_TargetScene->GetHierarchy() };
            for (auto& entityNode : hierarchy.GetNodes()) {
                DrawNodeTree( *entityNode );
            }

            if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() ) {
                // No entity es selected
                m_SetActiveEntityCallback(nullptr);
            }

            BlankSpacePopupMenu();

            ImGui::End();
        }
    }


    auto HierarchyPanel::DrawNodeTree( const GenTree<Entity*>::Node& node ) -> void {
        if (!node.data->IsValid()) {
            return;
        }

        Entity& current{ *node.data };
        Entity* currentSelection{ m_GetActiveEntityCallback() };


        const auto& tagCurrent{ current.GetComponent<TagComponent>() };


        const auto thisEntityIsSelected{ currentSelection != nullptr && tagCurrent.GetGUID() == currentSelection->GetComponent<TagComponent>().GetGUID() };
        static constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_AllowItemOverlap |
                                                        ImGuiTreeNodeFlags_Framed |
                                                        ImGuiTreeNodeFlags_SpanAvailWidth |
                                                        ImGuiTreeNodeFlags_FramePadding };

        const ImGuiTreeNodeFlags flags{ styleFlags | ( thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : 0 ) };
        const bool expanded{ ImGui::TreeNodeEx( reinterpret_cast<void*>( tagCurrent.GetGUID() ), flags, "%s", fmt::format( " {} {}", ICON_MD_WIDGETS, tagCurrent.GetTag() ).c_str() ) };

        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) {
            m_SetActiveEntityCallback(node.data);
        }

        OnEntityRightClickMenu( current );

        if ( expanded ) {
            ImGui::Indent();

            for (auto& child : node.children) {
                DrawNodeTree(*child);
            }

            ImGui::Unindent();
            ImGui::TreePop();
        }
    }


    auto HierarchyPanel::OnEntityRightClickMenu( Entity& target ) const -> void {
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
                    target.AddComponent<CameraComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Script", menuItemShortcut, menuItemSelected, !target.HasComponent<NativeScriptComponent>() ) ) {
                    target.AddComponent<NativeScriptComponent>();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if ( ImGui::MenuItem( "Remove object" ) ) {
                m_TargetScene->DestroyEntity( target.GetComponent<TagComponent>().GetGUID() );
            }

            if ( ImGui::MenuItem( "Create empty object" ) ) {
                EntityCreateInfo createInfo{};
                createInfo.Root = std::addressof(target);
                createInfo.Name = "Empty Object";
                createInfo.ModelMesh = nullptr;

                m_TargetScene->CreateEntity( createInfo );
            }

            DrawPrefabMenu(std::addressof(target));

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
    }


    auto HierarchyPanel::BlankSpacePopupMenu() const -> void {
        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        ImGui::PushStyleVar( ImGuiStyleVar_PopupBorderSize, 1.0f );

        if ( ImGui::BeginPopupContextWindow( "##HierarchyMenuOptions", popupWindowFlags ) ) {

            if ( ImGui::MenuItem( "Empty Object" ) ) {
                constexpr EntityCreateInfo createInfo{
                    .Name{ "Empty Object" },
                    .Root{ nullptr },
                    .ModelMesh{ nullptr },
                };

                m_TargetScene->CreateEntity( createInfo );
            }

            // We do not have the cursor on top of any entity
            // the new entity will have no root
            DrawPrefabMenu(nullptr );

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
    }
}