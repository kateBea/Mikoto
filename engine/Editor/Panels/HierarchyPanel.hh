//
// Created by kate on 6/25/23.
//

#ifndef KATE_ENGINE_HIERARCHY_PANEL_HH
#define KATE_ENGINE_HIERARCHY_PANEL_HH

#include <memory>

#include <entt/entt.hpp>

#include <Utility/Common.hh>
#include <Scene/Scene.hh>
#include <Editor/Panels/Panel.hh>
#include <Scene/Entity.hh>

namespace kaTe {
    class HierarchyPanel : public Panel {
    public:
        explicit HierarchyPanel(const std::shared_ptr<Scene>& scene, const Path_T &iconPath = {});
        ~HierarchyPanel() override = default;

        HierarchyPanel(const HierarchyPanel & other) = default;
        HierarchyPanel(HierarchyPanel && other) = default;

        auto operator=(const HierarchyPanel & other) -> HierarchyPanel & = default;
        auto operator=(HierarchyPanel && other) -> HierarchyPanel & = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) -> void override;
        auto SetScene(const std::shared_ptr<Scene>& scene) -> void;
        auto MakeVisible(bool value) -> void override { m_Visible = value; }

        KT_NODISCARD auto IsHovered() const -> bool override { return m_Hovered; }
        KT_NODISCARD auto IsFocused() const -> bool override { return m_Focused; }
        KT_NODISCARD auto IsVisible() const -> bool override { return m_Visible; }
    private:
        friend class InspectorPanel;

    private:
        // Helpers
        auto DrawEntityNode(Entity& target) -> void;
        auto EntityPopupMenu(Entity& target) -> void;
        auto BlankSpacePopupMenu() -> void;
    private:
        bool m_Visible{};
        bool m_Hovered;
        bool m_Focused;
        std::weak_ptr<Scene> m_Context{};

        // temporary, we may want to select more than one entity in our scene
        Entity m_ContextSelection{};
    };
}

#endif//KATE_ENGINE_HIERARCHYPANEL_HH
