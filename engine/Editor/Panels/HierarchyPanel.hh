/**
 * HierarchyPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_HIERARCHY_PANEL_HH
#define MIKOTO_HIERARCHY_PANEL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Utility/Common.hh>
#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Editor/Panels/Panel.hh>

namespace Mikoto {
    class HierarchyPanel : public Panel<HierarchyPanel> {
    public:
        explicit HierarchyPanel(const std::shared_ptr<Scene>& scene, const Path_T &iconPath = {});
        ~HierarchyPanel() = default;

        HierarchyPanel(const HierarchyPanel & other) = default;
        HierarchyPanel(HierarchyPanel && other) = default;

        auto operator=(const HierarchyPanel & other) -> HierarchyPanel & = default;
        auto operator=(HierarchyPanel && other) -> HierarchyPanel & = default;

        auto OnUpdate() -> void;
        auto OnEvent(Event& event) -> void;
        auto SetScene(const std::shared_ptr<Scene>& scene) -> void;
        auto MakeVisible(bool value) -> void { m_PanelIsVisible = value; }

    private:
        friend class InspectorPanel;

    private:
        // Helpers
        auto DrawEntityNode(Entity& target) -> void;
        auto EntityPopupMenu(Entity& target) -> void;
        auto BlankSpacePopupMenu() -> void;
    private:
        std::weak_ptr<Scene> m_Context{};
        Entity m_ContextSelection{};
    };
}

#endif // MIKOTO_HIERARCHY_PANEL_HH
