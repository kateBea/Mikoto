/**
 * Editor.hh
 * Created by kate on 6/22/23.
 * */

#ifndef MIKOTO_EDITOR_HH
#define MIKOTO_EDITOR_HH

// C++ Standard Library
#include <string_view>

// Third-Party Library
#include <imgui.h>

// Project Headers
#include <Utility/Common.hh>

namespace Mikoto::Editor {
    /*************************************************************
    * STRUCTURES
    * ***********************************************************/
    struct DockControlFlags {
        bool ApplicationCloseFlag{};

        bool HierarchyPanelVisible{ true };
        bool InspectorPanelVisible{ true };
        bool ScenePanelVisible{ true };
        bool SettingPanelVisible{ true };
        bool StatsPanelVisible{ true };
    };

    /*************************************************************
    * VARIABLES
    * ***********************************************************/
    inline DockControlFlags s_ControlFlags{};


    /*************************************************************
    * UTILITY FUNCTIONS
    * ***********************************************************/
    auto ThemeDarkModeDefault() -> void;
    auto ThemeDarkModeAlt() -> void;
    auto DrawAboutModalPopup() -> void;

    auto ShowDockingDisabledMessage() -> void;
    auto HelpMarker(std::string_view description) -> void;
    auto OnDockSpaceUpdate() -> void;

    MKT_NODISCARD inline auto GetControlFlags() -> const DockControlFlags& { return s_ControlFlags; }

}
#endif // MIKOTO_EDITOR_HH
