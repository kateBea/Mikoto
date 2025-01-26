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
#include <Scene/Entity/Entity.hh>
#include <STL/Data/GenTree.hh>

namespace Mikoto {

    class HierarchyPanel final : public Panel {
    public:
        explicit HierarchyPanel();
        auto operator=( HierarchyPanel&& other ) -> HierarchyPanel& = default;

        auto OnUpdate( float ts ) -> void override;

        ~HierarchyPanel() override = default;

    private:
        static auto DrawNodeTree( const std::unique_ptr<GenTree<Entity>::Node>& node) -> void;
        static auto OnEntityRightClickMenu( Entity& target ) -> void;
        static auto BlankSpacePopupMenu() -> void;
    };
}

#endif// MIKOTO_HIERARCHY_PANEL_HH
