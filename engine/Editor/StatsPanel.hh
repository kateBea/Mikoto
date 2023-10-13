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
        explicit StatsPanel();
        auto operator=(StatsPanel&& other) -> StatsPanel& = default;

        auto OnUpdate(float timeStep) -> void override;

    private:
        auto DrawPerformance() -> void;
        auto DrawSystemInfo() -> void;
        auto DrawActiveSceneInfo() const -> void;
        auto DrawLightInfo() -> void;

    private:
        float m_FrameRate{};
        float m_FrameTime{};
        Int32_T m_ColumCount{ 2 };
    };
}


#endif // MIKOTO_STATS_PANEL_HH
