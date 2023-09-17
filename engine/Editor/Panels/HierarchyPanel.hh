/**
 * HierarchyPanel.hh
 * Created by kate on 6/25/23.
 * */

#ifndef MIKOTO_HIERARCHY_PANEL_HH
#define MIKOTO_HIERARCHY_PANEL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries


// Project Headers
#include <Utility/Common.hh>
#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Editor/Panels/Panel.hh>

namespace Mikoto {
    class HierarchyPanel : public Panel {
    public:
        explicit HierarchyPanel(const std::shared_ptr<Scene>& scene, const Path_T& iconPath = {});

        auto operator=(HierarchyPanel&& other) -> HierarchyPanel & = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) -> void override;
        auto SetScene(const std::shared_ptr<Scene>& scene) -> void;

        ~HierarchyPanel() = default;

    private:
        friend class InspectorPanel;

    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        auto DrawEntityNode(Entity& target) -> void;
        auto EntityPopupMenu(Entity& target) -> void;
        auto BlankSpacePopupMenu() -> void;

    private:
        /*************************************************************
        * DATA MEMBERS
        * ***********************************************************/
        std::weak_ptr<Scene> m_Context{};
        Entity m_ContextSelection{};
    };
}

#endif // MIKOTO_HIERARCHY_PANEL_HH
