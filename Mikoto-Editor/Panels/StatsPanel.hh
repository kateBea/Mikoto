/**
 * StatsPanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_STATS_PANEL_HH
#define MIKOTO_STATS_PANEL_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Common/Common.hh>
#include <Panels/Panel.hh>

namespace Mikoto {
    class StatsPanel final : public Panel {
    public:
        explicit StatsPanel();

        auto OnUpdate(float timeStep) -> void override;

    private:
        auto UpdateStatsInfo(float timeStep) -> void;
        auto DrawPerformance() -> void;
        auto DrawSystemInfo() -> void;
        auto DrawActiveSceneInfo() const -> void;
        auto DrawLightInfo() -> void;

    private:
        float m_FrameRate{};
        float m_FrameTime{};
        Int32_T m_ColumCount{ 2 };

        // Interval in seconds
        float m_IntervalUpdate{ 1 };
        float m_LastTimeUpdate{};

        SystemInfo m_SysInfo{};
    };
}


#endif // MIKOTO_STATS_PANEL_HH
