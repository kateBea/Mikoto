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

#ifndef MIKOTO_HIERARCHY_PANEL_HH
#define MIKOTO_HIERARCHY_PANEL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Panels/Panel.hh>
#include <Scene/Entity.hh>

namespace mikoto::editor {

    struct EditorState;

    struct HierarchyPanelCreateInfo {
        EditorState* mState{};
    };

    class HierarchyPanel final : public Panel {
    public:
        explicit HierarchyPanel( const HierarchyPanelCreateInfo& createInfo );

        auto OnUpdate( float ts ) -> void override;

        ~HierarchyPanel() override = default;

    private:
        auto BlankSpacePopupMenu() -> void;
        auto DrawNodeTree( core::u64 entity ) -> void;

        auto OnEntityRightClickMenu( scene::Entity* entity ) -> void;

        auto DrawSearchBar() -> void;
        auto DrawTextMenu( scene::Entity* entity = nullptr ) -> void;
        auto DrawPrefabMenu( scene::Entity* root = nullptr ) -> void;
        auto DrawModelLoadMenu( scene::Entity* root = nullptr ) -> void;
        auto DrawLightMenuItems( scene::Entity* root = nullptr ) const -> void;

        auto AddEntityWithModel( scene::Entity* root = nullptr ) -> void;
        auto AddEntityWithModel( eastl::string_view uri, scene::Entity* root = nullptr ) -> void;
        auto AddEntityWithModel( asset::ModelHandle model, scene::Entity* root = nullptr ) -> void;

    private:
        EditorState* mEditorState{};

        ImGuiTextFilter mSearchFilter{};

        bool mIsEntityCreateQueued{};
        scene::EntityCreateInfo mEntityCreateInfo{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_HIERARCHY_PANEL_HH
