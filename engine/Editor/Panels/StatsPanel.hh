//
// Created by kate on 6/27/23.
//

#ifndef KATE_ENGINE_STATS_PANEL_HH
#define KATE_ENGINE_STATS_PANEL_HH

#include <Utility/Common.hh>

#include <Editor/Panels/PanelData.hh>
#include <Editor/Panels/Panel.hh>

namespace kaTe {
    class StatsPanel : public Panel {
    public:
        explicit StatsPanel() = default;
        explicit StatsPanel(const std::shared_ptr<StatsPanelData> &data, const Path_T &iconPath = {});

        StatsPanel(const StatsPanel& other) = default;
        StatsPanel(StatsPanel&& other) = default;

        auto operator=(const StatsPanel& other) -> StatsPanel& = default;
        auto operator=(StatsPanel&& other) -> StatsPanel& = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) ->  void override;
        auto MakeVisible(bool value) ->  void override { m_Visible = value; }

        KT_NODISCARD auto IsHovered() const -> bool override { return m_Hovered; }
        KT_NODISCARD auto IsFocused() const -> bool override { return m_Focused; }
        KT_NODISCARD auto IsVisible() const -> bool override { return m_Visible; }
    private:
        bool m_Visible{};
        bool m_Hovered{};
        bool m_Focused{};

        std::shared_ptr<StatsPanelData> m_Data{};
    };
}


#endif//KATE_ENGINE_STATS_PANEL_HH
