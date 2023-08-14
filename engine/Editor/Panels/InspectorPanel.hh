//
// Created by kate on 6/25/23.
//

#ifndef KATE_ENGINE_INSPECTORPANEL_HH
#define KATE_ENGINE_INSPECTORPANEL_HH

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

        KT_NODISCARD auto IsHovered() const -> bool override { return m_Hovered; }
        KT_NODISCARD auto IsFocused() const -> bool override { return m_Focused; }
        KT_NODISCARD auto IsVisible() const -> bool override { return m_Visible; }
    private:
        // TODO: DrawInspectorComponent<ComponentType>(std::function);

    private:
        std::shared_ptr<HierarchyPanel> m_Hierarchy{};
        bool m_Visible{};
        bool m_Hovered;
        bool m_Focused;

    };
}


#endif//KATE_ENGINE_INSPECTORPANEL_HH
