/**
 * HierarchyPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <GUI/Icons/IconsMaterialDesign.h>

#include <Common/Common.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/AssetsSystem.hh>
#include <Core/System/FileSystem.hh>
#include <Core/System/RenderSystem.hh>
#include <GUI/ImGuiUtils.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>
#include <Panels/HierarchyPanel.hh>
#include <Scene/Scene/Scene.hh>
#include <Tools/ConsoleManager.hh>

namespace Mikoto {
    static constexpr auto GetHierarchyName() -> std::string_view {
        return "Hierarchy";
    }

    auto HierarchyPanel::DrawPrefabMenuItems( const Entity* root ) const -> void {
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
                                                                            .Build()
                                                                            .string() );

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
                                                                            .Build()
                                                                            .string() );

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
                                                                            .Build()
                                                                            .string() );

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
                                                                            .Build()
                                                                            .string() );

                newEntity = m_TargetScene->CreateEntity( entityCreateInfo );
            }

            if ( ImGui::MenuItem( "Sponza" ) ) {
                entityCreateInfo.Name = "Sponza";
                entityCreateInfo.ModelMesh = assetsSystem.GetModel( PathBuilder()
                                                                            .WithPath( fileSystem.GetAssetsRootPath().string() )
                                                                            .WithPath( "Prefabs" )
                                                                            .WithPath( "sponza" )
                                                                            .WithPath( "sponza.obj" )
                                                                            .Build()
                                                                            .string() );

                newEntity = m_TargetScene->CreateEntity( entityCreateInfo );
            }

            if ( newEntity != nullptr ) {
                MKT_APP_LOGGER_INFO( "Created new entity: {}", newEntity->GetComponent<TagComponent>().GetTag() );
            }

            ImGui::EndMenu();
        }
    }

    auto HierarchyPanel::DrawLightMenuItems( const Entity* root ) const -> void {
        EntityCreateInfo entityCreateInfo{
            .Name{ },
            .Root{ root },
            .ModelMesh{ nullptr }
        };

        entityCreateInfo.Root = root;

        ImGui::Spacing();
        ImGui::Separator();

        Entity* newEntity{ nullptr };

        if ( ImGui::MenuItem( "Sky Light" ) ) {
            entityCreateInfo.Name = "Sky Light";
            newEntity = m_TargetScene->CreateEntity( entityCreateInfo );

            if (newEntity != nullptr) {
                LightComponent& lightComponent{ newEntity->AddComponent<LightComponent>() };
                lightComponent.SetType( LightType::DIRECTIONAL_LIGHT_TYPE );
            }
        }

        if ( ImGui::MenuItem( "Directional light" ) ) {
            entityCreateInfo.Name = "Directional light";
            newEntity = m_TargetScene->CreateEntity( entityCreateInfo );

            if (newEntity != nullptr) {
                LightComponent& lightComponent{ newEntity->AddComponent<LightComponent>() };
                lightComponent.SetType( LightType::DIRECTIONAL_LIGHT_TYPE );
            }
        }

        if ( ImGui::MenuItem( "Point light" ) ) {
            entityCreateInfo.Name = "Point light";
            newEntity = m_TargetScene->CreateEntity( entityCreateInfo );

            if (newEntity != nullptr) {
                LightComponent& lightComponent{ newEntity->AddComponent<LightComponent>() };
                lightComponent.SetType( LightType::POINT_LIGHT_TYPE );
                lightComponent.GetData().PointLightDat.AttenuationParams.x = 50.0f;
                lightComponent.GetData().PointLightDat.AttenuationParams.y = 50.0f;
            }
        }

        if ( ImGui::MenuItem( "Spot light" ) ) {
            entityCreateInfo.Name = "Spot light";
            newEntity = m_TargetScene->CreateEntity( entityCreateInfo );

            if (newEntity != nullptr) {
                LightComponent& lightComponent{ newEntity->AddComponent<LightComponent>() };
                lightComponent.SetType( LightType::SPOT_LIGHT_TYPE );
            }
        }

        if ( newEntity != nullptr ) {
            MKT_APP_LOGGER_INFO( "Created new entity: {}", newEntity->GetComponent<TagComponent>().GetTag() );
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
        if (node.data != nullptr && !node.data->IsValid()) {
            return;
        }

        Entity& current{ *node.data };
        Entity* currentSelection{ m_GetActiveEntityCallback() };

        const auto& tagCurrent{ current.GetComponent<TagComponent>() };


        const auto thisEntityIsSelected{ currentSelection != nullptr && tagCurrent.GetGUID() == currentSelection->GetComponent<TagComponent>().GetGUID() };
        ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_AllowItemOverlap |
                                                        ImGuiTreeNodeFlags_Framed |
                                                        ImGuiTreeNodeFlags_SpanAvailWidth |
                                                        ImGuiTreeNodeFlags_FramePadding };

        styleFlags |= node.IsLeaf() ? ImGuiTreeNodeFlags_Leaf : 0;

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


    auto HierarchyPanel::OnEntityRightClickMenu( Entity& entity ) const -> void {
        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f  } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f  } };

        if ( ImGui::BeginPopupContextItem( nullptr, popupWindowFlags ) ) {
            if ( ImGui::BeginMenu( "Add component" ) ) {
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

                FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

                AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

                Font* interBlack{ assetsSystem.LoadFont( {
                    .Path{ PathBuilder()
                        .WithPath( fileSystem.GetFontsRootPath().string() )
                        .WithPath( "Inter" )
                        .WithPath( "static" )
                        .WithPath( "Inter-VariableFont.ttf" )
                        .Build() },
                    .Size{} } ) };

                textComponent.LoadFont( interBlack );

                textComponent.SetFontSize( 12 );
                textComponent.SetTextContent( "Example" );
                textComponent.SetLetterSpacing( 1 );

                ImGui::CloseCurrentPopup();
            }

                ImGui::EndPopup();
            }

            if ( ImGui::MenuItem( "Remove object" ) ) {
                m_TargetScene->RemoveEntity( entity.GetComponent<TagComponent>().GetGUID() );

                // Deselect the entity
                m_SetActiveEntityCallback(nullptr);

                ConsoleManager::PushMessage(ConsoleLogLevel::CONSOLE_DEBUG, fmt::format("Removed entity: {}", entity.GetComponent<TagComponent>().GetTag()));
            }

            if ( ImGui::MenuItem( "Create empty object" ) ) {
                EntityCreateInfo createInfo{};
                createInfo.Root = std::addressof( entity );
                createInfo.Name = "Empty Object";
                createInfo.ModelMesh = nullptr;

                Entity* result{ m_TargetScene->CreateEntity( createInfo ) };

                ConsoleManager::PushMessage(ConsoleLogLevel::CONSOLE_INFO, fmt::format("Added entity: {}. Id => {}",
                    result->GetComponent<TagComponent>().GetTag(), StringUtils::ToHex(result->GetComponent<TagComponent>().GetGUID())));
            }

            DrawPrefabMenuItems( std::addressof( entity ) );

            ImGui::EndPopup();
        }
    }

    auto HierarchyPanel::DrawModelLoadMenuItem() const -> void {
        if ( ImGui::MenuItem( "Load model" ) ) {
            FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };
            RenderSystem& renderSystem{ Engine::GetSystem<RenderSystem>() };
            AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

            const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Model files", "obj,gltf,fbx" },
                { "OBJ files", "obj" },
                { "glTF files", "gltf" },
                { "FBX files", "fbx" }
            };

            const Path_T path{ fileSystem.OpenDialog( filters )};

            if ( !path.empty() ) {
                const ModelLoadInfo modelLoadInfo{
                    .Path = path,
                    .InvertedY = renderSystem.GetDefaultApi() == GraphicsAPI::VULKAN_API,
                    .WantTextures = true,
                };

                const Model* model{ assetsSystem.LoadModel( modelLoadInfo ) };

                const EntityCreateInfo entityCreateInfo{
                    .Name = path.stem().string(),
                    .Root = nullptr,
                    .ModelMesh = model,
                };

                Entity* result{ m_TargetScene->CreateEntity( entityCreateInfo ) };

                ConsoleManager::PushMessage(ConsoleLogLevel::CONSOLE_INFO, fmt::format("Added entity: {}. Id => {}",
                    result->GetComponent<TagComponent>().GetTag(), StringUtils::ToHex(result->GetComponent<TagComponent>().GetGUID())));
            }
        }
    }

    auto HierarchyPanel::BlankSpacePopupMenu() const -> void {
        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        ImGuiUtils::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiUtils::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f  } };
        ImGuiUtils::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f  } };

        if ( ImGui::BeginPopupContextWindow( "##HierarchyPanel::BlankSpacePopupMenu:HierarchyMenuOptions", popupWindowFlags ) ) {

            if ( ImGui::MenuItem( "Empty Object" ) ) {
                EntityCreateInfo createInfo{
                    .Name{ "Empty Object" },
                    .Root{ nullptr },
                    .ModelMesh{ nullptr },
                };

                Entity* result{ m_TargetScene->CreateEntity( createInfo ) };

                ConsoleManager::PushMessage(ConsoleLogLevel::CONSOLE_INFO, fmt::format("Added entity: {}. Id => {}",
                    result->GetComponent<TagComponent>().GetTag(), StringUtils::ToHex(result->GetComponent<TagComponent>().GetGUID())));
            }

            // We do not have the cursor on top of any entity
            // the new entity will have no root
            DrawPrefabMenuItems(nullptr );

            DrawModelLoadMenuItem();

            DrawLightMenuItems( nullptr );

            ImGui::EndPopup();
        }
    }
}
