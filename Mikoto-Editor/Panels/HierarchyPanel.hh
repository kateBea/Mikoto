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

#ifndef MIKOTO_PANELS_HIERARCHY_PANEL_HH
#define MIKOTO_PANELS_HIERARCHY_PANEL_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <entt/signal/sigh.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>

#include <Assets/AssetLoadRegistry.hh>
#include <Assets/AssetScriptSource.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>

#include <ImGui/TextEditor.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    struct HierarchyPanelCreateInfo {
        EditorState* mState{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class HierarchyPanel final : public Panel {
    public:
        explicit HierarchyPanel( const HierarchyPanelCreateInfo& createInfo );

        auto OnRender( float ts ) -> void override;

        ~HierarchyPanel() override = default;

    private:
        enum class SortOrder {
            eIgnore,
            eNameAscending,
            eNameDescending
        };

        struct VisibleRow {
            core::u64 mEntityID{};
            core::u32 mDepth{};
        };

        auto BlankSpacePopupMenu() -> void;
        /** @brief Draws the entity creation actions shared by the toolbar and context menu. */
        auto DrawCreateMenu() -> void;
        auto CreateGroup( scene::Entity* container = nullptr ) -> void;
        auto BuildVisibleRows() -> void;
        auto ObserveScene() -> void;
        auto OnHierarchyChanged( bool structureChanged ) -> void;
        auto BeginRename( scene::Entity& entity ) -> void;
        auto DrawRenameInput( scene::TagComponent& tag, const ImVec2& position ) -> void;
        auto DrawNodeTree( const VisibleRow& row, float rowHeight ) -> void;
        auto AcceptEntityDrop( core::u64 targetID ) -> void;
        /** @brief Draws column headings, the active scene root and its visible entity rows. */
        auto DrawSceneHierarchy() -> void;

        auto OnEntityRightClickMenu( scene::Entity* entity ) -> void;

        MKT_NODISCARD auto HasActiveFilter() const -> bool;
        auto BuildSearchResults() -> void;
        auto DrawSearchBar() -> void;
        auto DrawScripts() -> void;
        auto RefreshProjectScripts() -> void;
        auto DrawScriptEditor() -> void;
        auto OpenScriptEditor( const filesystem::Path& path ) -> void;
        auto DrawTextMenu( scene::Entity* entity = nullptr ) -> void;
        auto DrawAudioMenu( scene::Entity* entity = nullptr ) -> void;
        auto DrawVideoMenu( scene::Entity* entity = nullptr ) -> void;
        auto DrawTerrainMenu( scene::Entity* parent = nullptr ) -> void;
        auto DrawPrefabMenu( scene::Entity* root = nullptr ) -> void;
        auto DrawModelLoadMenu( scene::Entity* root = nullptr ) -> void;
        auto DrawCanvasMenu( scene::Entity* root = nullptr ) -> void;
        auto DrawLightMenuItems( scene::Entity* root = nullptr ) const -> void;

        auto AddEntityWithModel( scene::Entity* root = nullptr ) -> void;
        auto AddEntityWithModel( eastl::string_view uri, scene::Entity* root = nullptr ) -> void;
        auto AddEntityWithModel( const asset::AssetRef<asset::Model>& model, scene::Entity* root = nullptr ) -> void;

    private:
        EditorState* mEditorState{};

        imgui::TextEditor mScriptEditor{};
        filesystem::FileHandle mScriptFile{};
        eastl::string mScriptDiskText{};
        eastl::string mScriptEditorError{};
        bool mScriptEditorOpen{};
        bool mFocusScriptEditor{};
        eastl::unordered_map<eastl::string, asset::AssetHandle<asset::ScriptSource>> mScriptRequests{};
        filesystem::Path mPendingScriptOpen{};

        filesystem::Path mScriptDirectory{};
        eastl::vector<eastl::string> mProjectScripts{};
        eastl::string mScriptListError{};
        double mNextScriptRefresh{};

        ImGuiTextFilter mSearchFilter{};
        scene::EntityType mTypeFilter{ scene::EntityType::eCount };
        SortOrder mSortOrder{ SortOrder::eIgnore };

        ankerl::unordered_dense::set<core::u64> mSearchResults{};

        ankerl::unordered_dense::set<core::u64> mExpandedEntities{};
        ankerl::unordered_dense::set<core::u64> mVisitedEntities{};
        eastl::vector<VisibleRow> mVisibleRows{};
        eastl::vector<VisibleRow> mPendingRows{};
        ankerl::unordered_dense::map<core::u64, eastl::vector<core::u64>> mDisplayChildren{};
        // Connection is destroyed before the retained signal, including when
        // the scene itself has already been destroyed by a workspace switch.
        eastl::shared_ptr<entt::sigh<void( bool )>> mHierarchySignal{};
        entt::scoped_connection mHierarchyConnection{};
        bool mRowsDirty{ true };
        bool mSearchDirty{ true };

        core::u64 mContextEntityID{};
        core::u64 mReparentEntity{};
        core::u64 mReparentTarget{};
        bool mOpenEntityPopup{};

        core::u64 mRenameEntityID{};
        eastl::vector<char> mRenameBuffer{};
        bool mFocusRename{};

        bool mIsEntityCreateQueued{};
        scene::EntityCreateInfo mEntityCreateInfo{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_HIERARCHY_PANEL_HH
