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

#include <EASTL/string.h>
#include <EASTL/memory.h>
#include <EASTL/string_view.h>

#include <imgui.h>

#include <ImGui/IconsMaterialDesign.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Memory/Allocator.hh>

#include <Core/LocalizationService.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/AssetsService.hh>

#include <Threading/TaskService.hh>

#include <Application/EditorApp.hh>

#include <Layers/EditorLayer.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsMaterialDesignIcons.h>

#include <Panels/HierarchyPanel.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;

    auto HierarchyPanel::DrawPrefabMenu( Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( ImGui::BeginMenu( "3D Object" ) ) {
            if ( ImGui::MenuItem( "Cube" ) ) {
                AddEntityWithModel( mEditorState->GetPrefab( PrefabModelType::eCube ), root );
            }
            if ( ImGui::MenuItem( "Cone" ) ) {
                AddEntityWithModel( mEditorState->GetPrefab( PrefabModelType::eCone ), root );
            }
            if ( ImGui::MenuItem( "Cylinder" ) ) {
                AddEntityWithModel( mEditorState->GetPrefab( PrefabModelType::eCylinder ), root );
            }
            if ( ImGui::MenuItem( "Sphere" ) ) {
                AddEntityWithModel( mEditorState->GetPrefab( PrefabModelType::eSphere ), root );
            }

            ImGui::EndMenu();
        }
    }

    auto HierarchyPanel::DrawLightMenuItems( Entity* root ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        EntityCreateInfo entityCreateInfo{
            .mRoot = root,
            .mEntityType = EntityType::eLight };

        ImGui::Spacing();
        ImGui::Separator();

        if ( ImGui::MenuItem( "Sky Light" ) ) {
            entityCreateInfo.mName = "Sky Light";
            entityCreateInfo.mLightType = LightType::eSkyLight;
        }

        if ( ImGui::MenuItem( "Directional light" ) ) {
            entityCreateInfo.mName = "Directional light";
            entityCreateInfo.mLightType = LightType::eDirectional;
        }

        if ( ImGui::MenuItem( "Point light" ) ) {
            entityCreateInfo.mName = "Point light";
            entityCreateInfo.mLightType = LightType::ePoint;
        }

        if ( ImGui::MenuItem( "Spot light" ) ) {
            entityCreateInfo.mName = "Spot light";
            entityCreateInfo.mLightType = LightType::eSpot;
        }

        if ( !entityCreateInfo.mName.empty() ) {
            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        }
    }

    auto HierarchyPanel::DrawNodeTree( const u64 entityID ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        Entity* entity{ mEditorState->mActiveScene->FindByID( entityID ) };
        if ( entity == nullptr ) {
            return;
        }

        TagComponent& tagComponent{ entity->GetComponent<TagComponent>() };
        if (!mSearchFilter.PassFilter( tagComponent.GetTag().c_str() )) {
            return;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        Entity* currentSelection{ mEditorState->mSelectedEntity };
        const RelationComponent& relationComponent{ entity->GetComponent<RelationComponent>() };

        const auto thisEntityIsSelected{ currentSelection != nullptr &&
                                         entityID == currentSelection->GetComponent<TagComponent>().GetGuid() };
        const ImGuiTreeNodeFlags styleFlags{
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanFullWidth |
            ImGuiTreeNodeFlags_FramePadding |
            ( relationComponent.IsLeaf() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None ) |
            ( thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None )
        };

        const eastl::string icon{ GetStringFromUnicode( 63185 ) };
        const eastl::string nodeID{ string::Format( "##DrawTreeNode_{}", tagComponent.GetGuid() ) };
        const eastl::string nodeLabel{  string::Format( " {} {}", icon.data(), tagComponent.GetTag() ) };

        const bool expanded{ ImGui::TreeNodeEx( nodeID.c_str(), styleFlags, "%s", nodeLabel.c_str() ) };
        SetCursorHandOnLastItemHovered();

        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) {
            mEditorState->mSelectedEntity = entity;
        }

        OnEntityRightClickMenu( entity );

        ImGui::TableNextColumn();
        ImGui::TextUnformatted( Entity::GetTypeString( entity->GetType() ).data() );

        ImGui::TableNextColumn();
        eastl::string visibilityIcon{ string::Format("  {}", tagComponent.IsActive() ? ICON_MD_VISIBILITY : ICON_MD_VISIBILITY_OFF ) };

        ImVec2 textPos{ ImGui::GetCursorScreenPos() };

        ImGui::TextUnformatted(visibilityIcon.c_str());
        SetCursorHandOnLastItemHovered();

        ImVec2 textSize{ ImGui::CalcTextSize(visibilityIcon.c_str()) };

        ImGui::SetCursorScreenPos(textPos);
        const eastl::string nodeVisibilityButtonID{ string::Format( "##DrawNodeTreeTextIconClick_{}", tagComponent.GetGuid() ) };
        if (ImGui::InvisibleButton(nodeVisibilityButtonID.c_str(), textSize)) {
            tagComponent.SetActive(!tagComponent.IsActive());
        }

        if ( expanded ) {
            for ( auto& childID: relationComponent.GetChildren() ) {
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

        imgui::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        imgui::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::BeginPopupContextItem( nullptr, popupWindowFlags ) ) {
            if ( ImGui::BeginMenu( "Add component" ) ) {
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

                if ( ImGui::MenuItem( "Rigid Body", menuItemShortcut, menuItemSelected, !IsPresent<RigidBodyComponent>( entity ) ) ) {
                    entity->AddComponent<RigidBodyComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if ( ImGui::MenuItem( "Audio source", menuItemShortcut, menuItemSelected, !IsPresent<AudioSourceComponent>( entity ) ) ) {
                    entity->AddComponent<AudioSourceComponent>("");
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
                mEditorState->mActiveScene->RemoveEntity( entity->GetComponent<TagComponent>().GetGuid() );
                mEditorState->mSelectedEntity = nullptr;
            }

            if ( ImGui::MenuItem( "Create empty object" ) ) {
                EntityCreateInfo createInfo{
                    .mRoot = entity,
                    .mName = "Empty object",
                    .mEntityType = EntityType::eEmpty,
                };

                mEditorState->mActiveScene->PushEntity( createInfo );
            }

            DrawPrefabMenu( entity );
            DrawModelLoadMenu( entity );
            DrawLightMenuItems( entity );
            DrawTextMenu( entity );

            ImGui::EndPopup();
        }
    }

    auto HierarchyPanel::DrawSearchBar() -> void {
        const float cursorPosX{ ImGui::GetCursorPosX() };
        mSearchFilter.Draw( "##HierarchyPanelFilter", ImGui::GetContentRegionAvail().x );
        if ( !mSearchFilter.IsActive() ) {
            ImGui::SameLine();
            ImGui::SetCursorPosX( cursorPosX + ImGui::GetFontSize() * 0.5f );

            ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 255, 255, 128 ) );

            const eastl::string searchText{ LocalizationService::Get()->Translate( "hierarchy_search" ) };
            ImGui::TextUnformatted( string::Format( "{} {}...", ICON_MD_SEARCH, searchText.c_str() ).c_str() );

            ImGui::PopStyleColor();
            ImGui::Spacing();
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
                .mRoot = root,
                .mName = "Text",
                .mEntityType = EntityType::eText,
                .mIsWorldText = false,
                .mTextSize = TextComponent::GetMinLetterSize(),
                .mTextSpacing = TextComponent::GetMinLetterSpacing(),
                .mInitialContents = "Example",
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        }

        if ( ImGui::MenuItem( "Text 3D" ) ) {
            const EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                .mName = "Text",
                .mEntityType = EntityType::eText,
                .mIsWorldText = true,
                .mTextSize = TextComponent::GetMinLetterSize(),
                .mTextSpacing = TextComponent::GetMinLetterSpacing(),
                .mInitialContents = "Example",
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        }
    }

    auto HierarchyPanel::AddEntityWithModel( eastl::string_view uri, Entity* root ) -> void {
        threading::TaskService::Get()->Submit( [this, root, path = Path{ uri }]() -> void {
            ModelLoadDescription description{
                .mFile = FileService::Get()->LoadFile( path ),
                .mExtractTextures = true,
            };
            const ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( description ) };

            const EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                .mName{ description.mFile->GetName() },
                .mModel = model,
                .mEntityType = EntityType::eMesh,
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        } );
    }

    auto HierarchyPanel::AddEntityWithModel( ModelHandle model, Entity* root ) -> void {
        threading::TaskService::Get()->Submit( [this, root, model]() -> void {
            const EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                .mName{ model->GetName() },
                .mModel = model,
                .mEntityType = EntityType::eMesh,
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        } );
    }

    auto HierarchyPanel::AddEntityWithModel( Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        threading::TaskService::Get()->Submit( [this, root]() -> void {
            const std::initializer_list<FileDialogPair> filters{
                { "Model files", "obj,gltf,fbx,glb" },
                { "OBJ files", "obj" },
                { "glTF files", "gltf" },
                { "FBX files", "fbx" },
                { "GLB files", "glb" } };
            const Path path{ filesystem::OpenFileDialog( filters ) };
            if (path.IsEmpty()) {
                return;
            }

            ModelLoadDescription description{
                .mFile = FileService::Get()->LoadFile( path ),
                .mExtractTextures = true,
            };
            const ModelHandle model{ AssetsService::Get()->LoadAsset<Model>( description ) };

            const EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                .mName{ description.mFile->GetName() },
                .mModel = model,
                .mEntityType = EntityType::eMesh,
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        });
    }

    auto HierarchyPanel::BlankSpacePopupMenu() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        if ( ImGui::BeginPopupContextWindow( "##HierarchyPanel::BlankSpacePopupMenu:HierarchyMenuOptions", popupWindowFlags ) ) {
            if ( ImGui::MenuItem( "Empty Object" ) ) {
                mEditorState->mActiveScene->PushEntity( "Empty Object" );
                RuntimeConsole::Get()->Debug( string::Format( "New entity queued {}", "Empty Object" ) );
            }

            DrawPrefabMenu();
            DrawModelLoadMenu();
            DrawLightMenuItems();
            DrawTextMenu();

            ImGui::EndPopup();
        }
    }

    HierarchyPanel::HierarchyPanel( const HierarchyPanelCreateInfo& createInfo )
        : Panel{ "Hierarchy",  },
          mEditorState{ createInfo.mState } {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_MERGE, mPanelName );
    }

    auto HierarchyPanel::OnRender( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ), ImGuiWindowFlags_NoCollapse );

        mPanelIsHovered = ImGui::IsWindowHovered();
        mPanelIsFocused = ImGui::IsWindowFocused();

        DrawSearchBar();

        if (ImGui::TreeNodeEx("Scripts", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding )) {
            imgui::SetCursorHandOnLastItemHovered();

            ImGui::TreePop();
        }
        // If node is not opened
        SetCursorHandOnLastItemHovered();

        const float lineHeight{ ImGui::GetTextLineHeight() };

        if (mEditorState->mActiveScene) {
            bool isEntitiesHierarchyOpen{ ImGui::TreeNodeEx("Entities", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen) };

            UnindentScoped und{};
            if (isEntitiesHierarchyOpen) {
                SetCursorHandOnLastItemHovered();

                constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_BordersInner };
                if (ImGui::BeginTable("HierarchyTable", 3, tableFlags)) {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, lineHeight * 3.0f);

                    eastl::string visibilityIcon{ string::Format("  {}", ICON_MD_VISIBILITY) };
                    ImGui::TableSetupColumn(visibilityIcon.c_str(), ImGuiTableColumnFlags_WidthFixed, lineHeight * 2.0f);

                    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 4.0f));

                    ImGui::TableHeadersRow();

                    ImGui::PopStyleVar();

                    auto& entityList{ mEditorState->mActiveScene->GetEntities() };
                    for ( auto& [entityID, entity]: entityList ) {
                        const RelationComponent& relation{ entity->GetComponent<RelationComponent>() };

                        // ONLY trigger root items. The function handles its own rows now!
                        if ( !relation.HasParent() ) {
                            DrawNodeTree( entityID );
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::TreePop();
            }

            // Make cursor hand only when entity list tree node is closed
            // otherwise cursor hand will leak to other items
            if (!isEntitiesHierarchyOpen) {
                imgui::SetCursorHandOnLastItemHovered();
            }

            if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() ) {
                mEditorState->mSelectedEntity = nullptr;
            }

            BlankSpacePopupMenu();
        }

        ImGui::End();
    }
}// namespace Mikoto