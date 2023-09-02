/**
 * StatsPanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_STATS_PANEL_HH
#define MIKOTO_STATS_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Common.hh>
#include <Editor/Panels/PanelData.hh>
#include <Editor/Panels/Panel.hh>

namespace Mikoto {
    class StatsPanel : public Panel<StatsPanel> {
    public:
        explicit StatsPanel() = default;
        explicit StatsPanel(const std::shared_ptr<StatsPanelData> &data, const Path_T &iconPath = {});

        auto operator=(StatsPanel&& other) -> StatsPanel& = default;

        auto OnUpdate() -> void;
        auto OnEvent(Event& event) ->  void;
        auto MakeVisible(bool value) ->  void { m_PanelIsVisible = value; }

    private:
        auto DrawStatisticsTable() -> void;

    private:
        std::shared_ptr<StatsPanelData> m_Data{};
    };
}


#endif // MIKOTO_STATS_PANEL_HH
