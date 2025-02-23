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
#include <Scene/Scene/Entity.hh>
#include <Library/Data/GenTree.hh>

namespace Mikoto {
    struct HierarchyPanelCreateInfo {
        Scene* TargetScene{ nullptr };
        std::function<Entity*()> GetActiveEntityCallback{};
        std::function<void(Entity*)> SetActiveEntityCallback{};
    };

    class HierarchyPanel final : public Panel {
    public:
        explicit HierarchyPanel(const HierarchyPanelCreateInfo& createInfo);

        auto OnUpdate( float ts ) -> void override;

        ~HierarchyPanel() override = default;

    private:
        auto DrawNodeTree( const GenTree<Entity*>::Node& node ) -> void;
        auto OnEntityRightClickMenu( Entity& target ) const -> void;
        auto DrawModelLoadMenuItem() const -> void;
        auto BlankSpacePopupMenu() const -> void;
        auto DrawPrefabMenuItems( const Entity* root ) const -> void;

    private:
        Scene* m_TargetScene{ nullptr };

        std::function<Entity*()> m_GetActiveEntityCallback{};
        std::function<void(Entity*)> m_SetActiveEntityCallback{};
    };
}

#endif// MIKOTO_HIERARCHY_PANEL_HH
