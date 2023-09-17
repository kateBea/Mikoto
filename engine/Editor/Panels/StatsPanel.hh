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
    class StatsPanel : public Panel {
    public:
        explicit StatsPanel(const Path_T &iconPath = {});

        auto operator=(StatsPanel&& other) -> StatsPanel& = default;

        auto OnUpdate() -> void override;
        auto OnEvent(Event& event) ->  void override;

    private:
        /*************************************************************
        * HELPERS
        * ***********************************************************/
        auto DrawStatisticsTable() -> void;

    private:
        /*************************************************************
        * DATA MEMBERS
        * ***********************************************************/

    };
}


#endif // MIKOTO_STATS_PANEL_HH
