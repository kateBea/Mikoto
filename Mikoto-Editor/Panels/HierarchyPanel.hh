/**
 * HierarchyPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_HIERARCHY_PANEL_HH
#define MIKOTO_HIERARCHY_PANEL_HH

// C++ Standard Library
#include <memory>
#include <unordered_set>

// Project Headers
#include "Common/Common.hh"
#include "Panel.hh"
#include "Scene/Entity.hh"
#include "Scene/Scene.hh"

namespace Mikoto {

    class HierarchyPanel final : public Panel {
    public:
        explicit HierarchyPanel();
        auto operator=( HierarchyPanel&& other ) -> HierarchyPanel& = default;

        auto OnUpdate( float ts ) -> void override;

        ~HierarchyPanel() override = default;

    private:
        static auto DrawNodeTree(GenTree<Entity>& hierarchy) -> void;
        static auto OnEntityRightClickMenu( Entity& target ) -> void;
        static auto BlankSpacePopupMenu() -> void;
    };
}

#endif// MIKOTO_HIERARCHY_PANEL_HH
