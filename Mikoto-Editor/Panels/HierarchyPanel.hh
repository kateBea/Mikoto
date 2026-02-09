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

#ifndef MIKOTO_HIERARCHY_PANEL_HH
#define MIKOTO_HIERARCHY_PANEL_HH

#include <memory>

#include <Panels/Panel.hh>
#include <Scene/Entity.hh>

namespace Mikoto {
    struct EditorState;

    struct HierarchyPanelCreateInfo {
        EditorState* State{};
    };

    class HierarchyPanel final : public Panel {
    public:
        explicit HierarchyPanel(const HierarchyPanelCreateInfo& createInfo);

        auto OnUpdate( float ts ) -> void override;

        ~HierarchyPanel() override = default;

    private:
        auto AddEntityWithModel(Entity* root = nullptr) -> void;
        auto AddEntityWithModel(std::string_view uri, Entity* root = nullptr) -> void;

        auto DrawNodeTree( UInt64 entity ) -> void;

        auto BlankSpacePopupMenu() -> void;
        auto OnEntityRightClickMenu( Entity* entity ) -> void;
        auto DrawTextMenuItems(Entity* entity = nullptr) -> void;
        auto DrawPrefabMenuItems( Entity* root = nullptr ) -> void;
        auto DrawModelLoadMenuItem( Entity* root = nullptr ) -> void;
        auto DrawLightMenuItems( Entity* root = nullptr ) const -> void;

    private:
        EditorState* m_EditorState{};
    };
}

#endif// MIKOTO_HIERARCHY_PANEL_HH
