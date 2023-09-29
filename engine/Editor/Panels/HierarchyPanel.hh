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

        auto AddOnContextSelectCallback(const std::function<void(HierarchyPanel *)>& task) -> void;
        auto AddOnContextDeselectCallback(const std::function<void(HierarchyPanel *)>& task) -> void;

        MKT_NODISCARD auto GetContextSelection() const -> Entity  { return m_ContextSelection; }

        ~HierarchyPanel() override = default;

    private:
        friend class InspectorPanel;

    private:
        auto DrawEntityNode(Entity& target) -> void;
        auto EntityPopupMenu(Entity& target) -> void;
        auto BlankSpacePopupMenu() -> void;

        auto RunContextSelectionCallbacks() -> void;
        auto RunContextDeselectionCallbacks() -> void;

    private:
        std::vector<std::function<void(HierarchyPanel*)>> m_OnContextSelectCallbacks{};
        std::vector<std::function<void(HierarchyPanel*)>> m_OnContextDeselectCallbacks{};

        std::weak_ptr<Scene> m_Context{};
        Entity m_ContextSelection{};
    };
}

#endif // MIKOTO_HIERARCHY_PANEL_HH
