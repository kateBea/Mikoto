/**
 * HierarchyPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_HIERARCHY_PANEL_HH
#define MIKOTO_HIERARCHY_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Panels/Panel.hh>

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

        auto BlankSpacePopupMenu() -> void;
        auto DrawModelLoadMenuItem() -> void;
        auto DrawNodeTree( UInt64 entity ) -> void;
        auto DrawTextMenuItems(Entity* entity = nullptr) const;
        auto OnEntityRightClickMenu( Entity* entity ) -> void;
        auto DrawPrefabMenuItems( Entity* root = nullptr ) -> void;
        auto DrawLightMenuItems( Entity* root = nullptr ) const -> void;

    private:
        EditorState* m_EditorState{};
    };
}

#endif// MIKOTO_HIERARCHY_PANEL_HH
