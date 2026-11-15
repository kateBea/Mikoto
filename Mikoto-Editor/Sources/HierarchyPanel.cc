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

#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>

#include <EASTL/sort.h>
#include <EASTL/array.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/algorithm.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/LocalizationService.hh>

#include <Memory/Allocator.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Assets/AssetRegistryService.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Scripting/ScriptingService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsFontAwesome6.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

#include <Application/EditorApp.hh>

#include <Panels/ComponentMenu.hh>
#include <Panels/HierarchyPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;

    static constexpr core::cstr kEntityDragPayload{ "MIKOTO_ENTITY" };

    static auto GetHierarchyTypeLabel( scene::EntityType type ) -> const eastl::string& {
        constexpr eastl::array<core::cstr, core::as<core::usize>( scene::EntityType::eCount )> labels{
            "hierarchy_type_empty",
            "hierarchy_type_sound",
            "content_browser_material",
            "editor_layer_light",
            "hierarchy_text",
            "component_menu_mesh",
            "hierarchy_camera",
            "hierarchy_canvas",
            "hierarchy_group",
            "component_menu_script",
            "hierarchy_type_water",
            "hierarchy_terrain"
        };

        const auto index{ core::as<core::usize>( type ) };
        return MKT_LOC( index < labels.size() ? labels[index] : "stats_unknown" );
    }

    auto HierarchyPanel::DrawPrefabMenu( scene::Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "hierarchy_3d_object", "3D Object" ).c_str() ) ) {
            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_cube", "Cube" ).c_str() ) ) {
                AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eCube ), root );
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_cone", "Cone" ).c_str() ) ) {
                AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eCone ), root );
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_cylinder", "Cylinder" ).c_str() ) ) {
                AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eCylinder ), root );
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_sphere", "Sphere" ).c_str() ) ) {
                AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eSphere ), root );
            }

            ImGui::EndMenu();
        }
    }

    auto HierarchyPanel::DrawLightMenuItems( scene::Entity* root ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        scene::EntityCreateInfo entityCreateInfo{
            .mRoot = root,
            .mEntityType = scene::EntityType::eLight
        };

        ImGui::Separator();

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_sky_light", "Sky Light" ).c_str() ) ) {
            entityCreateInfo.mName = MKT_LOC( "hierarchy_sky_light" ).c_str();
            entityCreateInfo.mLightType = renderer::LightType::eSkyLight;
        }

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_directional_light", "Directional light" ).c_str() ) ) {
            entityCreateInfo.mName = MKT_LOC( "hierarchy_directional_light" ).c_str();
            entityCreateInfo.mLightType = renderer::LightType::eDirectional;
        }

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_point_light", "Point light" ).c_str() ) ) {
            entityCreateInfo.mName = MKT_LOC( "hierarchy_point_light" ).c_str();
            entityCreateInfo.mLightType = renderer::LightType::ePoint;
        }

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_spot_light", "Spot light" ).c_str() ) ) {
            entityCreateInfo.mName = MKT_LOC( "hierarchy_spot_light" ).c_str();
            entityCreateInfo.mLightType = renderer::LightType::eSpot;
        }

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_area_light", "Area light" ).c_str() ) ) {
            entityCreateInfo.mName = MKT_LOC( "hierarchy_area_light" ).c_str();
            entityCreateInfo.mLightType = renderer::LightType::eAreaLight;
        }

        if ( !entityCreateInfo.mName.empty() ) {
            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        }
    }

    auto HierarchyPanel::ObserveScene() -> void {
        auto* scene{ mEditorState->mActiveScene };
        const auto signal{ scene ? scene->GetHierarchySignal() : nullptr };
        // Comparing the signal also detects replacement at the same scene address.
        if ( mHierarchySignal != signal ) {
            // Entity payloads contain IDs scoped to the scene they came from.
            if ( const auto* payload{ ImGui::GetDragDropPayload() }; payload && payload->IsDataType( kEntityDragPayload ) ) {
                ImGui::ClearDragDrop();
            }

            mReparentEntity = 0;
            mReparentTarget = 0;

            mHierarchyConnection.release();
            mHierarchySignal = signal;
            mExpandedEntities.clear();
            mContextEntityID = 0;
            mRenameEntityID = 0;
            mFocusRename = false;
            mRowsDirty = mSearchDirty = true;

            if ( mHierarchySignal ) {
                mHierarchyConnection = entt::sink{ *mHierarchySignal }.connect<&HierarchyPanel::OnHierarchyChanged>( this );
            }
        }
    }

    auto HierarchyPanel::OnHierarchyChanged( bool structureChanged ) -> void {
        mSearchDirty = true;
        mRowsDirty |= structureChanged || HasActiveFilter() || mSortOrder != SortOrder::eIgnore;
    }

    auto HierarchyPanel::BeginRename( scene::Entity& entity ) -> void {
        const auto& tag{ entity.GetComponent<scene::TagComponent>() };
        mRenameEntityID = tag.GetGuid();
        mRenameBuffer.resize( eastl::max<core::usize>( 256, tag.GetTag().size() + 1 ) );
        eastl::copy( tag.GetTag().begin(), tag.GetTag().end(), mRenameBuffer.begin() );
        mRenameBuffer[tag.GetTag().size()] = '\0';
        mFocusRename = true;
        mEditorState->SelectEntity( &entity );

        // A selection made in the viewport may live beneath collapsed hierarchy nodes.
        auto parent{ mEditorState->mActiveScene->GetHierarchyParent( mRenameEntityID ) };

        while ( parent ) {
            mExpandedEntities.insert( parent );
            parent = mEditorState->mActiveScene->GetHierarchyParent( parent );
        }

        mRowsDirty = true;
    }

    auto HierarchyPanel::DrawRenameInput( scene::TagComponent& tag, const ImVec2& position ) -> void {
        ImGui::SetCursorScreenPos( position );
        ImGui::SetNextItemWidth( eastl::max( ImGui::GetContentRegionAvail().x, 1.0f ) );

        if ( mFocusRename ) {
            ImGui::SetKeyboardFocusHere();
        }

        const auto resize{ []( ImGuiInputTextCallbackData* data ) -> int {
            auto& buffer{ *static_cast<eastl::vector<char>*>( data->UserData ) };
            buffer.resize( core::as<core::usize>( data->BufSize ) );
            data->Buf = buffer.data();
            return 0;
        } };
        constexpr ImGuiInputTextFlags flags{ ImGuiInputTextFlags_AutoSelectAll |
                                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize };
        const bool submitted{ ImGui::InputText( "##RenameEntity", mRenameBuffer.data(), mRenameBuffer.size(), flags, resize, &mRenameBuffer ) };

        if ( mFocusRename ) {
            ImGui::SetScrollHereY( 0.5f );
            mFocusRename = !ImGui::IsItemActive();
        }

        // Keep the draft local until submission or focus loss, so Escape never changes the entity.
        if ( ImGui::IsKeyPressed( ImGuiKey_Escape, false ) ) {
            mRenameEntityID = 0;
            mFocusRename = false;
        }
        else if ( submitted || ImGui::IsItemDeactivated() ) {
            if ( mRenameBuffer[0] != '\0' ) {
                tag.SetTag( mRenameBuffer.data() );
            }

            mRenameEntityID = 0;
            mFocusRename = false;
        }
    }

    auto HierarchyPanel::BuildVisibleRows() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mRowsDirty ) {
            return;
        }

        mRowsDirty = false;

        auto* scene{ mEditorState->mActiveScene };

        mVisibleRows.clear();
        mPendingRows.clear();
        mVisitedEntities.clear();
        mDisplayChildren.clear();

        if ( !scene ) {
            return;
        }

        const bool filtering{ HasActiveFilter() };
        const bool filteringByType{ mTypeFilter != scene::EntityType::eCount };
        const auto sortRows{ [&]( auto first, auto last ) {
            if ( mSortOrder == SortOrder::eIgnore || last - first < 2 ) {
                return;
            }

            const bool ascending{ mSortOrder == SortOrder::eNameAscending };
            eastl::stable_sort( first, last, [&]( const VisibleRow& left, const VisibleRow& right ) {
                const auto& leftName{ scene->FindByID( left.mEntityID )->GetComponent<scene::TagComponent>().GetTag() };
                const auto& rightName{ scene->FindByID( right.mEntityID )->GetComponent<scene::TagComponent>().GetTag() };
                const auto order{ leftName.comparei( rightName ) };
                return ascending ? order < 0 : order > 0;
            } );
        } };

        // Rebuild the display order only when the hierarchy view changes.
        for ( const auto& entry: scene->GetEntities() ) {
            const auto& id{ entry.first };

            if ( const auto parent{ scene->GetHierarchyParent( id ) } ) {
                mDisplayChildren[parent].push_back( id );
            }
        }

        for ( auto* root: scene->GetRootEntities() ) {
            const auto id{ root->GetComponent<scene::TagComponent>().GetGuid() };

            if ( !scene->GetHierarchyParent( id ) ) {
                mPendingRows.push_back( { id, 0 } );
            }
        }
        if ( !filteringByType ) {
            sortRows( mPendingRows.begin(), mPendingRows.end() );
        }

        // Reverse each sibling batch for the traversal, which pops from the back.
        eastl::reverse( mPendingRows.begin(), mPendingRows.end() );

        while ( !mPendingRows.empty() ) {
            const auto row{ mPendingRows.back() };
            mPendingRows.pop_back();
            auto* entity{ scene->FindByID( row.mEntityID ) };

            if ( !entity || !mVisitedEntities.insert( row.mEntityID ).second ) {
                continue;
            }

            if ( !filtering || mSearchResults.contains( row.mEntityID ) ) {
                mVisibleRows.push_back( row );
            }
            else if ( !filteringByType ) {
                continue;
            }

            if ( filtering || mExpandedEntities.contains( row.mEntityID ) ) {
                const auto firstChild{ mPendingRows.size() };
                const core::u32 depth{ filteringByType ? 0 : row.mDepth + 1 };

                if ( const auto children{ mDisplayChildren.find( row.mEntityID ) }; children != mDisplayChildren.end() ) {
                    for ( const auto child: children->second ) {
                        mPendingRows.push_back( { child, depth } );
                    }
                }

                if ( !filteringByType ) {
                    sortRows( mPendingRows.begin() + firstChild, mPendingRows.end() );
                }

                eastl::reverse( mPendingRows.begin() + firstChild, mPendingRows.end() );
            }
        }

        if ( filteringByType ) {
            sortRows( mVisibleRows.begin(), mVisibleRows.end() );
        }

        // Keep expansion storage bounded when a large subtree is deleted.
        for ( auto it{ mExpandedEntities.begin() }; it != mExpandedEntities.end(); ) {
            if ( !scene->FindByID( *it ) ) {
                it = mExpandedEntities.erase( it );
            }
            else {
                ++it;
            }
        }
    }

    auto HierarchyPanel::AcceptEntityDrop( core::u64 targetID ) -> void {
        const auto* payload{ ImGui::AcceptDragDropPayload( kEntityDragPayload, ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) };

        if ( payload && payload->IsDelivery() && payload->Data && payload->DataSize == sizeof( core::u64 ) ) {
            mReparentEntity = *static_cast<const core::u64*>( payload->Data );
            mReparentTarget = targetID;
        }
    }

    auto HierarchyPanel::DrawNodeTree( const VisibleRow& row, float rowHeight ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const core::u64 entityID{ row.mEntityID };

        scene::Entity* entity{ mEditorState->mActiveScene->FindByID( entityID ) };

        if ( entity == nullptr ) {
            return;
        }

        scene::TagComponent& tagComponent{ entity->GetComponent<scene::TagComponent>() };

        ImGui::PushID( static_cast<core::i32>( entityID >> 32 ) );
        ImGui::PushID( static_cast<core::i32>( entityID ) );
        ImGui::TableNextRow( ImGuiTableRowFlags_None, rowHeight );
        const bool rowSubmitted{ ImGui::TableNextColumn() };

        scene::Entity* currentSelection{ mEditorState->mSelectedEntity };
        const bool isGroup{ entity->HasComponent<scene::GroupComponent>() };

        const auto thisEntityIsSelected{ eastl::find( mEditorState->mSelectedEntityIDs.begin(),
            mEditorState->mSelectedEntityIDs.end(), entityID ) != mEditorState->mSelectedEntityIDs.end() };
        const bool renaming{ mRenameEntityID == entityID };
        const ImVec4 normalText{ ImGui::GetStyleColorVec4( ImGuiCol_Text ) };

        if ( thisEntityIsSelected ) {
            constexpr ImVec4 selection{ 232.0f / 255.0f, 191.0f / 255.0f, 124.0f / 255.0f, 1.0f };

            // SpanAllColumns paints one gold selection across the complete entity row.
            ImGui::PushStyleColor( ImGuiCol_Header, selection );
            ImGui::PushStyleColor( ImGuiCol_HeaderHovered, selection );
            ImGui::PushStyleColor( ImGuiCol_HeaderActive, selection );
            ImGui::PushStyleColor( ImGuiCol_Text, ImVec4{ 47.0f / 255.0f, 41.0f / 255.0f, 32.0f / 255.0f, 1.0f } );
        }

        const bool filteringByType{ mTypeFilter != scene::EntityType::eCount };
        const ImGuiTreeNodeFlags styleFlags{
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAllColumns |
            ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_FramePadding |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ( filteringByType || !mDisplayChildren.contains( entityID ) ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None ) |
            ( thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None )
        };

        // Preserve the chosen code point for each entity type and encode once.
        static const eastl::string soundIcon{ imgui::GetStringFromUnicode( 57421 ) };
        static const eastl::string materialIcon{ imgui::GetStringFromUnicode( 59248 ) };
        static const eastl::string lightIcon{ imgui::GetStringFromUnicode( 58414 ) };
        static const eastl::string textIcon{ imgui::GetStringFromUnicode( 59618 ) };
        static const eastl::string meshIcon{ imgui::GetStringFromUnicode( 59289 ) };
        static const eastl::string cameraIcon{ imgui::GetStringFromUnicode( 58386 ) };
        static const eastl::string emptyIcon{ imgui::GetStringFromUnicode( 57933 ) };
        static const eastl::string canvasIcon{ imgui::GetStringFromUnicode( 58295 ) };
        static const eastl::string terrainIcon{ imgui::GetStringFromUnicode( 58359 ) };

        core::cstr icon{ emptyIcon.c_str() };

        switch ( entity->GetType() ) {
            case scene::EntityType::eSound:
                icon = soundIcon.c_str();
                break;
            case scene::EntityType::eMaterial:
                icon = materialIcon.c_str();
                break;
            case scene::EntityType::eLight:
                icon = lightIcon.c_str();
                break;
            case scene::EntityType::eText:
                icon = textIcon.c_str();
                break;
            case scene::EntityType::eCanvas:
                icon = canvasIcon.c_str();
                break;
            case scene::EntityType::eMesh:
                icon = meshIcon.c_str();
                break;
            case scene::EntityType::eCamera:
                icon = cameraIcon.c_str();
                break;
            case scene::EntityType::eScript:
                icon = ICON_FA_CODE;
                break;
            case scene::EntityType::eWater:
                icon = ICON_FA_WATER;
                break;
            case scene::EntityType::eTerrain:
                icon = terrainIcon.c_str();
                break;
            case scene::EntityType::eEmpty:
            default:
                break;
        }

        const auto rowPosition{ ImGui::GetCursorScreenPos() };
        const auto& style{ ImGui::GetStyle() };
        const float frameHeight{ ImGui::GetFrameHeight() };
        const core::f32 indentation{ core::as<core::f32>( row.mDepth + 1 ) * style.IndentSpacing };
        ImGui::Indent( indentation );
        ImGui::SetNextItemOpen( HasActiveFilter() || mExpandedEntities.contains( entityID ), ImGuiCond_Always );
        const ImVec2 labelStart{ ImGui::GetCursorScreenPos().x + ImGui::GetTreeNodeToLabelSpacing(),
            ImGui::GetCursorScreenPos().y + ImGui::GetStyle().FramePadding.y };
        const bool open{ isGroup ? ImGui::TreeNodeEx( "##Entity", styleFlags, " " )
                                 : ImGui::TreeNodeEx( "##Entity", styleFlags, "%s %s", icon, renaming ? "" : tagComponent.GetTag().c_str() ) };
        const bool expanded{ open && !filteringByType };

        if ( isGroup ) {
            const auto folder{ expanded ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER };
            const ImVec4 color{ thisEntityIsSelected ? ImGui::GetStyleColorVec4( ImGuiCol_Text ) :
                expanded ? ImVec4{ 1.0f, 0.72f, 0.20f, 1.0f } : ImVec4{ 0.85f, 0.60f, 0.15f, 1.0f } };
            auto* draw{ ImGui::GetWindowDrawList() };
            draw->AddText( labelStart, ImGui::GetColorU32( color ), folder );

            if ( !renaming ) {
                draw->AddText( { labelStart.x + ImGui::CalcTextSize( folder ).x + 6.0f, labelStart.y },
                    ImGui::GetColorU32( ImGuiCol_Text ), tagComponent.GetTag().c_str() );
            }
        }

        // A skipped table cell leaves the previous widget as ImGui's last item.
        if ( rowSubmitted ) {
            imgui::SetCursorHandOnLastItemHovered();
        }

        if ( rowSubmitted && !renaming && ImGui::BeginDragDropSource() ) {
            ImGui::SetDragDropPayload( kEntityDragPayload, &entityID, sizeof( entityID ) );

            // Restore the text color while the drag tooltip is still open.
            {
                const imgui::ImGuiScopedColor textColor{ ImGuiCol_Text, normalText };
                ImGui::TextUnformatted( tagComponent.GetTag().c_str() );
            }

            ImGui::EndDragDropSource();
        }

        if ( rowSubmitted && !renaming && ImGui::BeginDragDropTarget() ) {
            AcceptEntityDrop( entityID );
            ImGui::EndDragDropTarget();
        }

        if ( rowSubmitted && !renaming && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
            // Match viewport selection: a double-click selects the immediate parent.
            auto* selected{ entity };
            const auto parentID{ mEditorState->mActiveScene->GetHierarchyParent( entityID ) };

            if ( parentID && !isGroup ) {
                if ( auto* parent{ mEditorState->mActiveScene->FindByID( parentID ) } ) {
                    selected = parent;
                }
            }

            mEditorState->SelectEntity( selected );
        }
        else if ( rowSubmitted && !renaming && ImGui::IsItemClicked( ImGuiMouseButton_Left ) && !ImGui::IsItemToggledOpen() ) {
            mEditorState->SelectEntity( entity );
        }

        if ( rowSubmitted && !HasActiveFilter() && ImGui::IsItemToggledOpen() ) {
            mRowsDirty = true;

            if ( expanded ) {
                mExpandedEntities.insert( entityID );
            }
            else {
                mExpandedEntities.erase( entityID );
            }
        }

        if ( rowSubmitted && !renaming && ImGui::IsItemHovered() && ImGui::IsMouseReleased( ImGuiMouseButton_Right ) ) {
            mContextEntityID = entityID;
            mOpenEntityPopup = true;
        }

        if ( rowSubmitted && renaming ) {
            const imgui::ImGuiScopedColor textColor{ ImGuiCol_Text, normalText };
            const auto nameIcon{ isGroup ? ( expanded ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER ) : icon };
            const float gap{ isGroup ? 6.0f : ImGui::CalcTextSize( " " ).x };
            DrawRenameInput( tagComponent, { labelStart.x + ImGui::CalcTextSize( nameIcon ).x + gap, rowPosition.y } );
        }

        ImGui::Unindent( indentation );

        ImGui::TableNextColumn();
        ImGui::TextUnformatted( GetHierarchyTypeLabel( entity->GetType() ).c_str() );

        ImGui::TableNextColumn();
        const ImVec2 visibilitySize{ eastl::max( ImGui::GetContentRegionAvail().x, 1.0f ), ImGui::GetFrameHeight() };
        ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0, 0, 0, 0 } );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4{ 0, 0, 0, 0 } );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4{ 0, 0, 0, 0 } );

        if ( imgui::widget::IconButton( "Visibility", tagComponent.IsActive() ? ICON_MD_VISIBILITY : ICON_MD_VISIBILITY_OFF, visibilitySize ) ) {
            tagComponent.SetActive( !tagComponent.IsActive() );
        }

        ImGui::PopStyleColor( 3 );
        ImGui::PopStyleVar();
        imgui::SetCursorHandOnLastItemHovered();

        if ( thisEntityIsSelected ) {
            ImGui::PopStyleColor( 4 );
        }

        // Finish every cell before measuring the row, so adjacent guides share their endpoints.
        ImGui::TableSetColumnIndex( 0 );
        const auto* table{ ImGui::GetCurrentTable() };
        auto* draw{ ImGui::GetWindowDrawList() };
        const float guideX{ rowPosition.x + style.FramePadding.x + ImGui::GetFontSize() * 0.5f };
        constexpr ImU32 guideColor{ IM_COL32( 82, 82, 82, 255 ) };
        constexpr float guideWidth{ 1.0f };

        // Pixel-aligned filled strips avoid the wider fringe of anti-aliased strokes.
        for ( core::u32 depth{}; depth <= row.mDepth; ++depth ) {
            const float x{ ImFloor( guideX + core::as<float>( depth ) * style.IndentSpacing ) };
            draw->AddRectFilled( { x, ImFloor( table->RowPosY1 ) },
                { x + guideWidth, ImFloor( table->RowPosY2 ) }, guideColor );
        }

        const float branchY{ ImFloor( rowPosition.y + frameHeight * 0.5f ) };
        draw->AddRectFilled(
            { ImFloor( guideX + core::as<float>( row.mDepth ) * style.IndentSpacing ), branchY },
            { ImFloor( rowPosition.x + indentation + style.FramePadding.x ), branchY + guideWidth }, guideColor );

        ImGui::PopID();
        ImGui::PopID();
    }

    auto HierarchyPanel::OnEntityRightClickMenu( scene::Entity* entity ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( entity == nullptr ) {
            return;
        }

        imgui::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        imgui::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::BeginPopup( "##HierarchyEntityMenu" ) ) {
            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_rename", "Rename" ).c_str(), "F2" ) ) {
                BeginRename( *entity );
            }

            if ( !entity->HasComponent<scene::GroupComponent>() && ImGui::BeginMenu( imgui::widget::LocalizedLabel( "hierarchy_add_component", "Add component" ).c_str() ) ) {
                DrawAddComponentMenu( entity );
                ImGui::EndMenu();
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_remove_object", "Remove object" ).c_str() ) ) {
                mEditorState->mActiveScene->RemoveEntity( entity->GetComponent<scene::TagComponent>().GetGuid() );
                mEditorState->SelectEntity( nullptr );
                ImGui::EndPopup();
                return;
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_create_empty_object", "Create empty object" ).c_str() ) ) {
                scene::EntityCreateInfo createInfo{
                    .mRoot = entity,
                    .mName = MKT_LOC( "hierarchy_empty_name" ).c_str(),
                    .mEntityType = scene::EntityType::eEmpty,
                };

                mEditorState->mActiveScene->PushEntity( createInfo );
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_create_group" ).c_str() ) ) {
                CreateGroup( entity );
            }

            const auto entityID{ entity->GetComponent<scene::TagComponent>().GetGuid() };

            if ( mEditorState->mActiveScene->GetEntityGroup( entityID ) &&
                 ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_remove_from_group" ).c_str() ) ) {
                ( void )mEditorState->mActiveScene->SetEntityGroup( entityID, 0 );
            }

            DrawPrefabMenu( entity );
            DrawCanvasMenu( entity );
            DrawModelLoadMenu( entity );
            DrawLightMenuItems( entity );
            DrawAudioMenu( entity );
            DrawVideoMenu( entity );
            DrawTerrainMenu( entity );
            DrawTextMenu( entity );

            ImGui::EndPopup();
        }
    }

    auto HierarchyPanel::HasActiveFilter() const -> bool {
        return mSearchFilter.IsActive() || mTypeFilter != scene::EntityType::eCount;
    }

    auto HierarchyPanel::BuildSearchResults() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mSearchDirty ) {
            return;
        }

        mSearchDirty = false;
        mSearchResults.clear();

        if ( !HasActiveFilter() || !mEditorState->mActiveScene ) {
            return;
        }

        const auto scene{ mEditorState->mActiveScene };
        const bool filteringByType{ mTypeFilter != scene::EntityType::eCount };

        for ( const auto& entry: scene->GetEntities() ) {
            const auto& id{ entry.first };
            const auto& candidate{ entry.second };

            if ( ( filteringByType && candidate->GetType() != mTypeFilter ) ||
                 !mSearchFilter.PassFilter( candidate->GetComponent<scene::TagComponent>().GetTag().c_str() ) ) {
                continue;
            }

            // A type filter lists exact matches. Text search alone keeps their parent paths.
            if ( filteringByType ) {
                mSearchResults.insert( id );
                continue;
            }

            // Include the path to each match. Stop on an already visited ancestor,
            // which also protects search from malformed cyclic parent relations.
            auto entity{ candidate.get() };

            while ( entity ) {
                if ( !mSearchResults.insert( entity->GetComponent<scene::TagComponent>().GetGuid() ).second ) {
                    break;
                }

                const auto parent{ scene->GetHierarchyParent( entity->GetComponent<scene::TagComponent>().GetGuid() ) };
                entity = parent ? scene->FindByID( parent ) : nullptr;
            }
        }
    }

    auto HierarchyPanel::DrawSearchBar() -> void {
        MKT_BEGIN_PROFILER();

        ImGui::BeginDisabled( !mEditorState->mActiveScene );

        if ( ImGui::Button( ICON_MD_ADD " " ICON_MD_EXPAND_MORE "##HierarchyAdd" ) ) {
            ImGui::OpenPopup( "##HierarchyAddMenu" );
        }

        ImGui::EndDisabled();

        if ( ImGui::BeginPopup( "##HierarchyAddMenu" ) ) {
            if ( mEditorState->mActiveScene ) {
                DrawCreateMenu();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        const auto height{ ImGui::GetFrameHeight() };
        const ImVec2 buttonSize{ height, height };

        if ( imgui::widget::ToggleIconButton( "HierarchyFilter", ICON_MD_FILTER_LIST, mTypeFilter != scene::EntityType::eCount, buttonSize ) ) {
            ImGui::OpenPopup( "##HierarchyFilterMenu" );
        }

        imgui::widget::TooltipText( MKT_LOC( "hierarchy_filter_entities" ) );

        if ( ImGui::BeginPopup( "##HierarchyFilterMenu" ) ) {
            auto filter{ mTypeFilter };

            if ( ImGui::MenuItem( MKT_LOC( "inspector_none" ).c_str(), nullptr, filter == scene::EntityType::eCount ) ) {
                filter = scene::EntityType::eCount;
            }

            if ( ImGui::BeginMenu( MKT_LOC( "hierarchy_filter_by_type" ).c_str() ) ) {
                for ( core::usize index{}; index < core::as<core::usize>( scene::EntityType::eCount ); ++index ) {
                    const auto type{ core::as<scene::EntityType>( index ) };

                    if ( ImGui::MenuItem( GetHierarchyTypeLabel( type ).c_str(), nullptr, filter == type ) ) {
                        filter = type;
                    }
                }

                ImGui::EndMenu();
            }

            if ( filter != mTypeFilter ) {
                mTypeFilter = filter;
                mSearchDirty = mRowsDirty = true;
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if ( imgui::widget::ToggleIconButton( "HierarchySort", ICON_MD_SORT_BY_ALPHA, mSortOrder != SortOrder::eIgnore, buttonSize ) ) {
            ImGui::OpenPopup( "##HierarchySortMenu" );
        }

        imgui::widget::TooltipText( MKT_LOC( "hierarchy_sort_entities" ) );

        if ( ImGui::BeginPopup( "##HierarchySortMenu" ) ) {
            const auto sortOption{ [&]( SortOrder order, core::cstr label ) {
                if ( ImGui::MenuItem( MKT_LOC( label ).c_str(), nullptr, mSortOrder == order ) && mSortOrder != order ) {
                    mSortOrder = order;
                    mRowsDirty = true;
                }
            } };

            sortOption( SortOrder::eIgnore, "hierarchy_sort_ignore" );
            sortOption( SortOrder::eNameAscending, "hierarchy_sort_name_ascending" );
            sortOption( SortOrder::eNameDescending, "hierarchy_sort_name_descending" );
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if ( imgui::widget::SearchFilter( "##HierarchyPanelFilter", mSearchFilter,
                 MKT_LOC( "hierarchy_search_filter" ) ) ) {
            mSearchDirty = mRowsDirty = true;
        }
    }

    auto HierarchyPanel::DrawModelLoadMenu( scene::Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_load_model", "Load model" ).c_str() ) ) {
            AddEntityWithModel( root );
        }
    }

    auto HierarchyPanel::DrawCanvasMenu( scene::Entity* root ) -> void {
        ImGui::Spacing();
        ImGui::Separator();

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_canvas", "Canvas" ).c_str() ) ) {
            const scene::EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                .mName = MKT_LOC( "hierarchy_canvas" ).c_str(),
                .mEntityType = scene::EntityType::eCanvas
            };
            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        }
    }

    auto HierarchyPanel::DrawTextMenu( scene::Entity* root ) -> void {
        MKT_BEGIN_PROFILER();

        ImGui::Spacing();
        ImGui::Separator();

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_text", "Text" ).c_str() ) ) {
            const scene::EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                .mName = MKT_LOC( "hierarchy_text" ).c_str(),
                .mEntityType = scene::EntityType::eText,
                .mIsWorldText = false,
                .mTextSize = scene::TextComponent::GetMinLetterSize(),
                .mTextSpacing = scene::TextComponent::GetMinLetterSpacing(),
                .mInitialContents = "Example",
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        }

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_text_3d", "Text 3D" ).c_str() ) ) {
            const scene::EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                .mName = MKT_LOC( "hierarchy_text" ).c_str(),
                .mEntityType = scene::EntityType::eText,
                .mIsWorldText = true,
                .mTextSize = scene::TextComponent::GetMinLetterSize(),
                .mTextSpacing = scene::TextComponent::GetMinLetterSpacing(),
                .mInitialContents = "Example",
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        }
    }

    auto HierarchyPanel::AddEntityWithModel( eastl::string_view uri, scene::Entity* root ) -> void {
        MKT_BEGIN_PROFILER();
        threading::TaskService::Get()->ExecuteWorkerTask( [this, root, path = filesystem::Path{ uri }]() -> void {
            asset::ModelLoadDescription description{
                .mFile = filesystem::FileService::Get()->LoadFile( path ),
                .mExtractTextures = true,
            };
            const asset::ModelHandle model{ asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( description ) };

            if ( model.IsEmpty() ) {
                return;
            }

            const scene::EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                    .mName{ description.mFile->GetName() },
                .mModel = model,
                .mEntityType = scene::EntityType::eMesh,
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        } );
    }

    auto HierarchyPanel::AddEntityWithModel( const asset::AssetRef<asset::Model>& model, scene::Entity* root ) -> void {
        MKT_BEGIN_PROFILER();
        mEditorState->mActiveScene->PushEntity( {
            .mRoot = root,
            .mName = eastl::string{ model.GetPath().GetStem() },
            .mModelAsset = model,
            .mEntityType = scene::EntityType::eMesh,
        } );
    }

    auto HierarchyPanel::AddEntityWithModel( scene::Entity* root ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        threading::TaskService::Get()->ExecuteWorkerTask( [this, root]() -> void {
            const std::initializer_list<filesystem::FileDialogPair> filters{
                { MKT_LOC( "hierarchy_model_files" ).c_str(), "obj,gltf,fbx,glb" },
                { MKT_LOC( "hierarchy_obj_files" ).c_str(), "obj" },
                { MKT_LOC( "hierarchy_gltf_files" ).c_str(), "gltf" },
                { MKT_LOC( "hierarchy_fbx_files" ).c_str(), "fbx" },
                { MKT_LOC( "hierarchy_glb_files" ).c_str(), "glb" }
            };

            const filesystem::Path path{ filesystem::OpenFileDialog( filters ) };

            if ( path.IsEmpty() ) {
                return;
            }

            asset::ModelLoadDescription description{
                .mFile = filesystem::FileService::Get()->LoadFile( path ),
                .mExtractTextures = true,
            };

            asset::ModelHandle model{ asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( description ) };

            if ( model.IsEmpty() ) {
                return;
            }

            const scene::EntityCreateInfo entityCreateInfo{
                .mRoot = root,
                    .mName{ description.mFile->GetName() },
                .mModel = model,
                .mEntityType = scene::EntityType::eMesh,
            };

            mEditorState->mActiveScene->PushEntity( entityCreateInfo );
        } );
    }

    auto HierarchyPanel::CreateGroup( scene::Entity* container ) -> void {
        auto* group{ mEditorState->mActiveScene->CreateGroup( MKT_LOC( "hierarchy_group" ), container ) };

        if ( group ) {
            mEditorState->SelectEntity( group );

            if ( container ) {
                mExpandedEntities.insert( container->GetComponent<scene::TagComponent>().GetGuid() );
            }

            mRowsDirty = true;
        }
    }

    auto HierarchyPanel::DrawAudioMenu( scene::Entity* parent ) -> void {
        if ( !ImGui::BeginMenu( imgui::widget::LocalizedLabel( "hierarchy_audio", "Audio" ).c_str() ) ) {
            return;
        }

        const auto create{ [&]<typename Component>( const char* label ) {
            if ( !ImGui::MenuItem( imgui::widget::LocalizedLabel( label ).c_str() ) ) {
                return;
            }

            if ( auto* entity{ mEditorState->mActiveScene->CreateEntity( scene::EntityCreateInfo{
                .mRoot = parent,
                .mName = MKT_LOC( label ).c_str(),
                .mEntityType = scene::EntityType::eSound } ) } ) {
                entity->AddComponent<Component>();
                mEditorState->SelectEntity( entity );

                if ( parent ) {
                    mExpandedEntities.insert( parent->GetComponent<scene::TagComponent>().GetGuid() );
                }

                mRowsDirty = true;
            }
        } };
        create.operator()<scene::AudioSourceComponent>( "hierarchy_audio_source" );
        create.operator()<scene::AudioListenerComponent>( "hierarchy_audio_listener" );
        ImGui::EndMenu();
    }

    auto HierarchyPanel::DrawTerrainMenu( scene::Entity* parent ) -> void {
        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_terrain", "Terrain" ).c_str() ) ) {
            if ( auto* terrain{ mEditorState->mActiveScene->CreateEntity( scene::EntityCreateInfo{
                .mRoot = parent,
                .mName = MKT_LOC( "hierarchy_terrain" ).c_str(),
                .mEntityType = scene::EntityType::eTerrain } ) } ) {
                mEditorState->SelectEntity( terrain );
                mRowsDirty = true;
            }
        }
    }

    auto HierarchyPanel::DrawVideoMenu( scene::Entity* parent ) -> void {
        if ( !ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_video", "Video" ).c_str() ) ) {
            return;
        }

        auto* entity{ mEditorState->mActiveScene->CreateEntity( scene::EntityCreateInfo{
            .mRoot = parent,
            .mName = "Video",
            .mEntityType = scene::EntityType::eEmpty } ) };

        if ( entity ) {
            entity->AddComponent<scene::VideoComponent>();
            mEditorState->SelectEntity( entity );

            if ( parent ) {
                mExpandedEntities.insert( parent->GetComponent<scene::TagComponent>().GetGuid() );
            }

            mRowsDirty = true;
        }
    }

    auto HierarchyPanel::BlankSpacePopupMenu() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        imgui::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        imgui::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        constexpr ImGuiPopupFlags popupWindowFlags{
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_MouseButtonRight
        };

        if ( ImGui::BeginPopupContextWindow( "##HierarchyPanel::BlankSpacePopupMenu:HierarchyMenuOptions", popupWindowFlags ) ) {
            DrawCreateMenu();
            ImGui::EndPopup();
        }
    }

    auto HierarchyPanel::DrawCreateMenu() -> void {
        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_create_group" ).c_str() ) ) {
            CreateGroup();
        }

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_empty_object", "Create empty" ).c_str() ) ) {
            mEditorState->mActiveScene->PushEntity( MKT_LOC( "hierarchy_empty_name" ).c_str() );
        }

        DrawAudioMenu();
        DrawVideoMenu();
        DrawTerrainMenu();

        if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "hierarchy_simulation", "Simulation" ).c_str() ) ) {
            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_simulation_particles", "Particles" ).c_str() ) ) {
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_simulation_water", "Water" ).c_str() ) ) {
                mEditorState->mActiveScene->PushEntity( scene::EntityCreateInfo{
                    .mName = MKT_LOC( "hierarchy_simulation_water" ).c_str(),
                    .mEntityType = scene::EntityType::eWater } );
            }

            ImGui::EndMenu();
        }

        if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_camera", "Camera" ).c_str() ) ) {
            if ( auto* camera{ mEditorState->mActiveScene->CreateEntity( scene::EntityCreateInfo{
                .mName = MKT_LOC( "hierarchy_camera" ).c_str(),
                .mEntityType = scene::EntityType::eCamera } ) } ) {
                camera->AddComponent<scene::CameraComponent>();
                mEditorState->SelectEntity( camera );
                mRowsDirty = true;
            }
        }

        DrawPrefabMenu();
        DrawCanvasMenu();
        DrawModelLoadMenu();
        DrawLightMenuItems();
        DrawTextMenu();
    }

    HierarchyPanel::HierarchyPanel( const HierarchyPanelCreateInfo& createInfo )
        : Panel{ "Hierarchy", "Panel", createInfo.mGroup, createInfo.mIsToggleable },
          mEditorState{ createInfo.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_MERGE, mPanelName );
    }

    auto HierarchyPanel::RefreshProjectScripts() -> void {
        const auto& directory{ scripting::ScriptingService::Get()->GetScriptBasePath() };

        if ( mScriptDirectory != directory ) {
            mScriptDirectory = directory;
            mScriptRequests.clear();
            mProjectScripts.clear();
            mScriptEditorOpen = false;
            mScriptFile.Reset();
            mPendingScriptOpen = {};
            mNextScriptRefresh = 0;
        }

        if ( ImGui::GetTime() < mNextScriptRefresh ) {
            return;
        }

        mNextScriptRefresh = ImGui::GetTime() + 1.0;
        mProjectScripts.clear();
        mScriptListError.clear();

        try {
            const auto root{ directory.GetPathTyped<std::filesystem::path>() };

            if ( !std::filesystem::exists( root ) ) {
                return;
            }

            for ( auto it{ std::filesystem::recursive_directory_iterator{ root } }; it != std::filesystem::recursive_directory_iterator{}; ++it ) {
                if ( it->is_directory() ) {
                    const auto name{ it->path().filename() };

                    if ( name == "bin" || name == "obj" || name == ".cache" || name == ".git" ) {
                        it.disable_recursion_pending();
                    }

                    continue;
                }

                if ( !it->is_regular_file() ) {
                    continue;
                }

                const filesystem::Path path{ it->path() };
                const auto extension{ path.GetNormalizedExtension() };

                if ( extension == "lua" || extension == "cs" || extension == "cpp" ) {
                    mProjectScripts.push_back( path.GetAbsolute() );
                }
            }

            eastl::sort( mProjectScripts.begin(), mProjectScripts.end() );
            // Drop cached requests for deleted or renamed sources.
            for ( auto it{ mScriptRequests.begin() }; it != mScriptRequests.end(); ) {
                if ( !eastl::binary_search( mProjectScripts.begin(), mProjectScripts.end(), it->first ) ) {
                    it = mScriptRequests.erase( it );
                }
                else {
                    ++it;
                }
            }
        } catch ( const std::exception& error ) {
            mScriptListError = error.what();
        }
    }

    auto HierarchyPanel::DrawScripts() -> void {
        imgui::UnindentScoped unindent{};

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_create_script", "Create script" ).c_str() ) ) {
            ImGui::OpenPopup( "CreateProjectScript" );
        }

        if ( ImGui::BeginPopup( "CreateProjectScript" ) ) {
            constexpr scripting::ScriptLanguage languages[]{ scripting::ScriptLanguage::eNative, scripting::ScriptLanguage::eLua, scripting::ScriptLanguage::eCSharp };
            const char* labels[]{ "Native (C++)", "Lua", "C# (.NET)" };

            for ( int index{}; index < 3; ++index ) {
                if ( ImGui::MenuItem( labels[index] ) ) {
                    try {
                        const auto path{ scripting::ScriptingService::Get()->CreateScript( languages[index] ) };

                        if ( !path.IsEmpty() ) {
                            mNextScriptRefresh = 0;
                            RefreshProjectScripts();
                            OpenScriptEditor( path );
                        }
                    } catch ( const std::exception& error ) {
                        core::RuntimeConsole::Get()->Error( error.what() );
                    }
                }
            }

            ImGui::EndPopup();
        }

        RefreshProjectScripts();
        const auto& paths{ mProjectScripts };

        if ( !mScriptListError.empty() ) {
            ImGui::TextWrapped( "%s", mScriptListError.c_str() );
        }

        bool shown{};
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV };

        if ( !ImGui::BeginTable( "HierarchyScriptsTable", 3, tableFlags ) ) {
            return;
        }

        ImGui::TableSetupColumn( MKT_LOC( "hierarchy_name" ).c_str(), ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide );
        ImGui::TableSetupColumn( MKT_LOC( "hierarchy_language" ).c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight() * 4.0f );
        ImGui::TableSetupColumn( MKT_LOC( "hierarchy_status" ).c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight() * 5.0f );
        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2{ 6.0f, 4.0f } );
        ImGui::TableHeadersRow();
        ImGui::PopStyleVar();

        for ( const auto& value: paths ) {
            if ( !mSearchFilter.PassFilter( value.c_str() ) ) {
                continue;
            }

            shown = true;
            const filesystem::Path path{ value };
            const bool sourceFile{ path.GetNormalizedExtension() == "lua" || path.GetNormalizedExtension() == "cs" || path.GetNormalizedExtension() == "cpp" };

            if ( sourceFile && mScriptRequests[value].GetState() == asset::AssetState::eUnloaded ) {
                mScriptRequests[value] = asset::AssetRegistryService::Get()->Request<asset::ScriptSource>( asset::ScriptLoadDescription{ .mPath = path } );
            }

            const auto state{ sourceFile ? mScriptRequests[value].GetState() : asset::AssetState::eFailed };
            const eastl::string name{ path.GetFilename() };
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );
            ImGui::PushID( value.c_str() );
            const bool selected{ mScriptFile && mScriptFile->GetPath().GetAbsolute() == path.GetAbsolute() };

            if ( ImGui::Selectable( name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick ) &&
                 ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                OpenScriptEditor( path );
            }

            if ( state == asset::AssetState::eReady && ImGui::BeginDragDropSource() ) {
                ImGui::SetDragDropPayload( "MIKOTO_SCRIPT_SOURCE", value.c_str(), value.size() + 1 );
                ImGui::TextUnformatted( name.c_str() );
                ImGui::EndDragDropSource();
            }

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( "%s", value.c_str() );
            }

            ImGui::TableSetColumnIndex( 1 );
            ImGui::TextUnformatted( path.GetNormalizedExtension() == "lua" ? "Lua" : path.GetNormalizedExtension() == "cpp" ? "C++"
                                                                                                                            : "C#" );
            ImGui::TableSetColumnIndex( 2 );

            if ( sourceFile && state == asset::AssetState::eFailed ) {
                if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "hierarchy_retry", "Retry" ).c_str() ) ) {
                    asset::AssetRegistryService::Get()->ReloadScriptSource( path );
                    mScriptRequests[value] = asset::AssetRegistryService::Get()->Request<asset::ScriptSource>( asset::ScriptLoadDescription{ .mPath = path } );
                }

                if ( ImGui::IsItemHovered() ) {
                    mikoto::imgui::widget::ItemTooltip( "%s", mScriptRequests[value].GetError().c_str() );
                }
            }
            else if ( sourceFile && state != asset::AssetState::eReady ) {
                ImGui::TextDisabled( "%s", state == asset::AssetState::eQueued ? MKT_LOC( "hierarchy_queued" ).c_str() : MKT_LOC( "hierarchy_loading" ).c_str() );
            }
            else {
                ImGui::TextDisabled( "%s", sourceFile ? MKT_LOC( "hierarchy_ready" ).c_str() : MKT_LOC( "hierarchy_assembly" ).c_str() );
            }

            ImGui::PopID();
        }

        ImGui::EndTable();

        if ( !shown ) {
            ImGui::TextDisabled( "%s", paths.empty() ? MKT_LOC( "hierarchy_no_scripts_in_this_project" ).c_str() : MKT_LOC( "hierarchy_no_matching_scripts" ).c_str() );
        }
    }

    auto HierarchyPanel::OpenScriptEditor( const filesystem::Path& path ) -> void {
        mPendingScriptOpen = {};
        mScriptEditorOpen = true;
        mFocusScriptEditor = true;

        if ( mScriptFile && mScriptFile->GetPath().GetAbsolute() == path.GetAbsolute() ) {
            return;
        }

        mScriptFile.Reset();
        mScriptDiskText.clear();
        mScriptEditor.SetText( {} );
        mScriptEditor.SetReadOnly( true );
        const auto extension{ path.GetNormalizedExtension() };

        if ( extension != "lua" && extension != "cs" && extension != "cpp" ) {
            mScriptEditorError = MKT_LOC( "hierarchy_this_script_is_a_compiled_assembly_open_its_cs_source" ).c_str();
            return;
        }

        try {
            const auto key{ path.GetAbsolute() };
            auto& request{ mScriptRequests[key] };

            if ( request.GetState() == asset::AssetState::eUnloaded ) {
                request = asset::AssetRegistryService::Get()->Request<asset::ScriptSource>( asset::ScriptLoadDescription{ .mPath = path } );
            }

            const auto source{ request.GetIfReady() };

            if ( !source ) {
                if ( request.GetState() == asset::AssetState::eFailed ) {
                    mPendingScriptOpen = {};
                    mScriptEditorError = request.GetError();
                }
                else {
                    mPendingScriptOpen = path;
                    mScriptEditorError = MKT_LOC( "hierarchy_loading_script_source" ).c_str();
                }

                return;
            }

            const auto file{ filesystem::FileService::Get()->LoadFile( path ) };

            if ( !file ) {
                mScriptEditorError = MKT_LOC( "hierarchy_cannot_read_the_selected_script" ).c_str();
                return;
            }

            mScriptFile = file;
            mPendingScriptOpen = {};
            mScriptDiskText = file->GetContentsString();
            mScriptEditor.SetLanguage( extension == "lua" ? TextLanguage::eLua : extension == "cpp" ? TextLanguage::eCpp
                                                                                                    : TextLanguage::eCSharp );
            mScriptEditor.SetText( mScriptDiskText );
            mScriptEditor.SetReadOnly( true );
            mScriptEditor.SetShowWhitespaces( false );
            mScriptEditorError.clear();
        } catch ( const std::exception& error ) {
            mScriptEditorError = error.what();
        }
    }

    auto HierarchyPanel::DrawScriptEditor() -> void {
        if ( !mScriptEditorOpen ) {
            return;
        }

        ImGui::SetNextWindowSize( ImVec2{ 850, 600 }, ImGuiCond_FirstUseEver );

        if ( mFocusScriptEditor ) {
            ImGui::SetNextWindowFocus();
            mFocusScriptEditor = false;
        }

        if ( mScriptFile && mPendingScriptOpen.IsEmpty() ) {
            // FileService refreshes its snapshot when the watcher sees an external save.
            const auto file{ filesystem::FileService::Get()->LoadFile( mScriptFile->GetPath() ) };

            if ( file ) {
                const auto contents{ file->GetContentsString() };

                if ( contents != mScriptDiskText ) {
                    const auto cursor{ mScriptEditor.GetCursorPosition() };
                    mScriptEditor.SetText( contents );
                    mScriptEditor.SetSelection( cursor, cursor );
                    mScriptDiskText = contents;
                }

                mScriptEditorError.clear();
            }
            else {
                mScriptEditorError = MKT_LOC( "hierarchy_cannot_read_the_selected_script" ).c_str();
            }
        }

        if ( imgui::BeginWindow( imgui::widget::LocalizedLabel( "hierarchy_script_editor_hierarchyscripteditor", "Script editor###HierarchyScriptEditor" ).c_str(), &mScriptEditorOpen,
                 ImGuiWindowFlags_MenuBar ) ) {
            imgui::ImGuiScopedTextFont codeFont{ imgui::ImGuiService::Get()->PushFont( "./Resources/Fonts/Google_Sans_Code/static/GoogleSansCode-Light.ttf" ) };

            if ( ImGui::BeginMenuBar() ) {
                if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "hierarchy_edit", "Edit" ).c_str(), !!mScriptFile ) ) {
                    if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_copy", "Copy" ).c_str(), "Ctrl+C", false, mScriptEditor.HasSelection() ) ) {
                        mScriptEditor.Copy();
                    }

                    if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_select_all", "Select all" ).c_str(), "Ctrl+A" ) ) {
                        mScriptEditor.SelectAll();
                    }

                    ImGui::EndMenu();
                }

                if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "hierarchy_view", "View" ).c_str() ) ) {
                    if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_dark_palette", "Dark Palette" ).c_str() ) ) {
                        mScriptEditor.SetPalette( TextEditorPalette::eDark );
                    }

                    if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_light_palette", "Light Palette" ).c_str() ) ) {
                        mScriptEditor.SetPalette( TextEditorPalette::eLight );
                    }

                    if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "hierarchy_retro_blue_palette", "Retro Blue Palette" ).c_str() ) ) {
                        mScriptEditor.SetPalette( TextEditorPalette::eRetroBlue );
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }

            if ( !mScriptEditorError.empty() ) {
                ImGui::TextWrapped( "%s", mScriptEditorError.c_str() );
            }

            if ( mScriptFile ) {
                ImGui::TextWrapped( MKT_LOC( "hierarchy_source" ).c_str(), mScriptFile->GetPath().GetC_Str(), "" );
                const auto cursor{ mScriptEditor.GetCursorPosition() };

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text( MKT_LOC( "hierarchy_lines" ).c_str(), cursor.mLine + 1, cursor.mColumn + 1,
                    mScriptEditor.GetTotalLines(), mScriptEditor.IsOverwrite() ? MKT_LOC( "hierarchy_ovr" ).c_str() : MKT_LOC( "hierarchy_ins" ).c_str(), " ",
                    TextEditor::GetLanguageName( mScriptEditor.GetLanguage() ) );
                mScriptEditor.Draw( "##HierarchyScriptSource", ImGui::GetContentRegionAvail() );
            }
        }

        ImGui::End();
    }

    auto HierarchyPanel::DrawSceneHierarchy() -> void {
        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX };

        if ( !ImGui::BeginTable( "HierarchyTable", 3, tableFlags ) ) {
            return;
        }

        const float scale{ ImGui::GetFontSize() / 17.0f };
        const float rounding{ 2.0f * scale };
        const float lineHeight{ ImGui::GetTextLineHeight() };
        const float frameExtension{ ImTrunc( ImGui::GetCurrentWindow()->WindowPadding.x * 0.5f ) };

        // Match ImGui's framed tree bounds so the column heading, scene and Scripts share their edges.
        const auto drawFrame{ [frameExtension]( ImU32 color, float radius ) {
            ImGui::TableSetColumnIndex( 0 );
            const auto* table{ ImGui::GetCurrentTable() };
            ImGui::TablePushBackgroundChannel();
            ImGui::GetWindowDrawList()->AddRectFilled(
                { table->WorkRect.Min.x - frameExtension, table->RowPosY1 },
                { table->WorkRect.Max.x + frameExtension, table->RowPosY2 }, color, radius );
            ImGui::TablePopBackgroundChannel();
        } };

        ImGui::TableSetupColumn( MKT_LOC( "hierarchy_name" ).c_str(), ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide );
        ImGui::TableSetupColumn( MKT_LOC( "hierarchy_type" ).c_str(), ImGuiTableColumnFlags_WidthFixed, lineHeight * 3.0f );
        ImGui::TableSetupColumn( ICON_MD_VISIBILITY, ImGuiTableColumnFlags_WidthFixed, lineHeight * 2.0f );
        {
            const imgui::ImGuiScopedStyleVar headerPadding{ ImGuiStyleVar_CellPadding, ImVec2{ 6.0f * scale, 7.0f * scale } };
            ImGui::TableNextRow( ImGuiTableRowFlags_Headers, lineHeight + 14.0f * scale );
            ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, 0 );
            drawFrame( ImGui::GetColorU32( ImGuiCol_TableHeaderBg ), rounding );

            for ( int column{}; column < 3; ++column ) {
                ImGui::TableSetColumnIndex( column );

                if ( column == 2 ) {
                    const float offset{ ( ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize( ICON_MD_VISIBILITY ).x ) * 0.5f };
                    ImGui::SetCursorPosX( ImGui::GetCursorPosX() + eastl::max( offset, 0.0f ) );
                }

                ImGui::TableHeader( ImGui::TableGetColumnName( column ) );

                // Keep hover inside the outer corners instead of letting a square cell fill leak out.
                ImGui::TableSetBgColor( ImGuiTableBgTarget_CellBg, 0 );

                if ( ImGui::IsItemHovered() || ImGui::IsItemActive() ) {
                    const auto* table{ ImGui::GetCurrentTable() };
                    auto bounds{ ImGui::TableGetCellBgRect( table, column ) };

                    if ( column == 0 ) {
                        bounds.Min.x -= frameExtension;
                    }
                    else if ( column == 2 ) {
                        bounds.Max.x += frameExtension;
                    }

                    const auto corners{ column == 0 ? ImDrawFlags_RoundCornersLeft : column == 2 ? ImDrawFlags_RoundCornersRight
                                                                                                 : ImDrawFlags_RoundCornersNone };
                    ImGui::TablePushBackgroundChannel();
                    ImGui::GetWindowDrawList()->AddRectFilled( bounds.Min, bounds.Max,
                        ImGui::GetColorU32( ImGui::IsItemActive() ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered ), rounding, corners );
                    ImGui::TablePopBackgroundChannel();
                }
            }

            ImGui::TablePushBackgroundChannel();
            const auto* table{ ImGui::GetCurrentTable() };

            for ( int column{ 1 }; column < 3; ++column ) {
                ImGui::GetWindowDrawList()->AddLine( { table->Columns[column].MinX, table->RowPosY1 },
                    { table->Columns[column].MinX, table->RowPosY2 }, ImGui::GetColorU32( ImGuiCol_TableBorderLight ) );
            }

            ImGui::TablePopBackgroundChannel();
        }

        {
            const imgui::ImGuiScopedStyleVar gapPadding{ ImGuiStyleVar_CellPadding, ImVec2{ 0, 0 } };
            ImGui::TableNextRow( ImGuiTableRowFlags_None, 3.0f * scale );
            drawFrame( ImGui::GetColorU32( ImGuiCol_WindowBg ), 0.0f );
        }

        ImGui::TableNextRow();
        const bool rootSubmitted{ ImGui::TableSetColumnIndex( 0 ) };

        if ( HasActiveFilter() || mRenameEntityID ) {
            ImGui::SetNextItemOpen( true, ImGuiCond_Always );
        }

        // Saved scenes follow their file identity, including files created from the starter template.
        const auto name{ mEditorState->mActiveScenePath.IsEmpty() ? mEditorState->mActiveScene->GetName() : mEditorState->mActiveScenePath.GetStem() };
        constexpr ImGuiTreeNodeFlags sceneFlags{
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAllColumns |
            ImGuiTreeNodeFlags_LabelSpanAllColumns | ImGuiTreeNodeFlags_NoTreePushOnOpen
        };

        ImGui::PushStyleVar( ImGuiStyleVar_FrameRounding, rounding );
        ImGui::PushStyleColor( ImGuiCol_Header, ImGui::GetStyleColorVec4( ImGuiCol_MenuBarBg ) );

        static const eastl::string icon{ imgui::GetStringFromUnicode( 59248 ) };
        const bool expanded{ ImGui::TreeNodeEx( "##ActiveScene", sceneFlags,
            "%s %.*s", icon.c_str(), core::as<int>( name.size() ), name.data() ) };
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if ( rootSubmitted ) {
            imgui::SetCursorHandOnLastItemHovered();

            if ( ImGui::BeginDragDropTarget() ) {
                AcceptEntityDrop( 0 );
                ImGui::EndDragDropTarget();
            }
        }

        if ( expanded ) {
            const imgui::ImGuiScopedStyleVar cellPadding{
                ImGuiStyleVar_CellPadding, ImVec2{ ImGui::GetStyle().CellPadding.x, 1.0f * scale }
            };
            BuildVisibleRows();
            // Reserve the same pixel-aligned height in the table and clipper. The eye button
            // inherits the tree's text baseline, which adds to its otherwise unpadded height.
            const auto& style{ ImGui::GetStyle() };
            const float rowHeight{ ImCeil( ImGui::GetFrameHeight() + style.FramePadding.y + style.CellPadding.y * 2.0f ) };
            ImGuiListClipper clipper{};
            clipper.Begin( core::as<int>( mVisibleRows.size() ), rowHeight );

            if ( mRenameEntityID ) {
                // Keep the edit submitted even when its row scrolls outside the visible range.
                for ( core::usize index{}; index < mVisibleRows.size(); ++index ) {
                    if ( mVisibleRows[index].mEntityID == mRenameEntityID ) {
                        clipper.IncludeItemByIndex( core::as<int>( index ) );
                        break;
                    }
                }
            }
            while ( clipper.Step() ) {
                for ( int index{ clipper.DisplayStart }; index < clipper.DisplayEnd; ++index ) {
                    DrawNodeTree( mVisibleRows[index], rowHeight );
                }
            }
        }
        else {
            mRenameEntityID = 0;
            mFocusRename = false;
        }

        ImGui::EndTable();
    }

    auto HierarchyPanel::OnRender( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ObserveScene();

        // The source window remains usable when Hierarchy is hidden or collapsed.
        if ( mScriptEditorOpen && !mPendingScriptOpen.IsEmpty() ) {
            const auto pending{ mPendingScriptOpen };
            OpenScriptEditor( pending );
        }

        DrawScriptEditor();

        if ( !mPanelIsVisible ) {
            mRenameEntityID = 0;
            mFocusRename = false;
            return;
        }

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            mRenameEntityID = 0;
            mFocusRename = false;
            ImGui::End();
            return;
        }

        mPanelIsHovered = ImGui::IsWindowHovered();
        mPanelIsFocused = ImGui::IsWindowFocused();

        const float scale{ ImGui::GetFontSize() / 17.0f };
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 6.0f * scale, ImGui::GetStyle().FramePadding.y } );
        ImGui::PushStyleVar( ImGuiStyleVar_CellPadding, ImVec2{ 6.0f * scale, 3.0f * scale } );

        DrawSearchBar();
        BuildSearchResults();

        if ( mRenameEntityID && ( !mEditorState->mActiveScene || !mEditorState->mActiveScene->FindByID( mRenameEntityID ) ||
             ( HasActiveFilter() && !mSearchResults.contains( mRenameEntityID ) ) ) ) {
            mRenameEntityID = 0;
            mFocusRename = false;
        }

        if ( mPanelIsFocused && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
             ImGui::IsKeyPressed( ImGuiKey_F2, false ) && mEditorState->mSelectedEntity ) {
            const auto id{ mEditorState->mSelectedEntity->GetComponent<scene::TagComponent>().GetGuid() };

            if ( !HasActiveFilter() || mSearchResults.contains( id ) ) {
                BeginRename( *mEditorState->mSelectedEntity );
            }
        }

        if ( ImGui::TreeNodeEx( imgui::widget::LocalizedLabel( "hierarchy_scripts", "Scripts" ).c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding ) ) {
            imgui::SetCursorHandOnLastItemHovered();
            DrawScripts();
            ImGui::TreePop();
        }
        // If node is not opened
        imgui::SetCursorHandOnLastItemHovered();

        if ( mEditorState->mActiveScene ) {
            DrawSceneHierarchy();
        }

        if ( mEditorState->mActiveScene ) {
            if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() ) {
                mEditorState->SelectEntity( nullptr );
            }

            // Keep the drop ID stable as rows expand or the panel resizes.
            const ImVec2 dropSize{ ImGui::GetContentRegionAvail() };
            const ImVec2 dropStart{ ImGui::GetCursorScreenPos() };
            const ImRect dropArea{ dropStart, { dropStart.x + dropSize.x, dropStart.y + dropSize.y } };

            if ( dropSize.x > 0.0f && dropSize.y > 0.0f &&
                 ImGui::BeginDragDropTargetCustom( dropArea, ImGui::GetID( "##HierarchyRootDrop" ) ) ) {
                AcceptEntityDrop( 0 );
                ImGui::EndDragDropTarget();
            }

            if ( mReparentEntity ) {
                auto* scene{ mEditorState->mActiveScene };

                if ( scene->MoveEntityInHierarchy( mReparentEntity, mReparentTarget ) ) {
                    if ( mReparentTarget ) {
                        mExpandedEntities.insert( mReparentTarget );
                    }
                }
                else {
                    core::RuntimeConsole::Get()->Warning( MKT_LOC( "hierarchy_move_rejected" ) );
                }

                mReparentEntity = 0;
                mReparentTarget = 0;
            }

            BlankSpacePopupMenu();

            // Submit the popup outside clipped rows so it remains usable if
            // its owning row scrolls out of view while the menu is open.
            if ( mOpenEntityPopup ) {
                ImGui::OpenPopup( "##HierarchyEntityMenu" );
                mOpenEntityPopup = false;
            }

            OnEntityRightClickMenu( mEditorState->mActiveScene->FindByID( mContextEntityID ) );
        }

        ImGui::PopStyleVar( 2 );
        ImGui::End();
    }
}// namespace mikoto::editor
