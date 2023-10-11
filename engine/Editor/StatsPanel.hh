/**
 * StatsPanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_STATS_PANEL_HH
#define MIKOTO_STATS_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Editor/Panel.hh>
#include <Utility/Common.hh>

namespace Mikoto {
    class StatsPanel : public Panel {
    public:
        explicit StatsPanel(const Path_T &iconPath = {});
        auto operator=(StatsPanel&& other) -> StatsPanel& = default;

        auto OnUpdate() -> void override;

    private:
        auto DrawStatisticsTable() const -> void;

    private:
        Int32_T m_ColumCount{ 2 };
    };
}


#endif // MIKOTO_STATS_PANEL_HH
