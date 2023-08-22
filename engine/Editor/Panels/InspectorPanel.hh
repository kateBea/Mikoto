//
// Created by kate on 6/25/23.
//

#ifndef MIKOTO_INSPECTOR_PANEL_HH
#define MIKOTO_INSPECTOR_PANEL_HH

#include <memory>

#include <entt/entt.hpp>

#include <Utility/Common.hh>

#include <Editor/Panels/HierarchyPanel.hh>
#include <Editor/Panels/Panel.hh>
#include <Scene/Scene.hh>

namespace Mikoto {
    class InspectorPanel : public Panel {
    public:

        explicit InspectorPanel(const std::shared_ptr<HierarchyPanel> &hierarchy, const Path_T& iconPath = {});
        ~InspectorPanel() override = default;

        InspectorPanel(const InspectorPanel& other) = default;
        InspectorPanel(InspectorPanel&& other) = default;

        auto operator=(const InspectorPanel& other) -> InspectorPanel& = default;
        auto operator=(InspectorPanel&& other) -> InspectorPanel& = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) -> void override;
        auto MakeVisible(bool value) -> void override { m_Visible = value; }

        MKT_NODISCARD auto IsHovered() const -> bool override { return m_Hovered; }
        MKT_NODISCARD auto IsFocused() const -> bool override { return m_Focused; }
        MKT_NODISCARD auto IsVisible() const -> bool override { return m_Visible; }
    private:
        // TODO: DrawInspectorComponent<ComponentType>(std::function);

    private:
        std::shared_ptr<HierarchyPanel> m_Hierarchy{};
        bool m_Visible{};
        bool m_Hovered;
        bool m_Focused;

    };
}


#endif // MIKOTO_INSPECTOR_PANEL_HH
