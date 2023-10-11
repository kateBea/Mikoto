/**
 * HierarchyPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_HIERARCHY_PANEL_HH
#define MIKOTO_HIERARCHY_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Editor/Panel.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    class HierarchyPanel : public Panel {
    public:
        explicit HierarchyPanel(const Path_T& iconPath = {});
        auto operator=(HierarchyPanel&& other) -> HierarchyPanel & = default;

        auto OnUpdate() -> void override;

        ~HierarchyPanel() override = default;

    private:
        friend class InspectorPanel;

    private:
        static auto DrawEntityNode(Entity& target) -> void;
        static auto OnEntityRightClickMenu(Entity& target) -> void;
        static auto BlankSpacePopupMenu() -> void;
    };
}

#endif // MIKOTO_HIERARCHY_PANEL_HH
