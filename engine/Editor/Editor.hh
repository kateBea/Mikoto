/**
 * Editor.hh
 * Created by kate on 6/22/23.
 * */

#ifndef MIKOTO_EDITOR_HH
#define MIKOTO_EDITOR_HH

// C++ Standard Library
#include <functional>
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

    struct DockSpaceCallbacks {
        // This function is called when we click on the Save scene of the File Menu
        std::function<void()> OnSceneSaveCallback{};

        // This function is called when we click on the Load scene of the File Menu
        std::function<void()> OnSceneLoadCallback{};
    };

    /*************************************************************
     * VARIABLES
     *
     * Please access these trough their getters
     * ***********************************************************/
    inline DockControlFlags s_ControlFlags{};
    inline DockSpaceCallbacks s_DockSpaceCallbacks{};


    /*************************************************************
    * UTILITY FUNCTIONS
    * ***********************************************************/
    auto ThemeDarkModeDefault() -> void;
    auto ThemeDarkModeAlt() -> void;
    auto DrawAboutModalPopup() -> void;

    auto ShowDockingDisabledMessage() -> void;
    auto HelpMarker(std::string_view description) -> void;
    auto OnDockSpaceUpdate() -> void;

    MKT_NODISCARD inline auto GetControlFlags() -> DockControlFlags& { return s_ControlFlags; }
    MKT_NODISCARD inline auto GetDockSpaceCallbacks() -> DockSpaceCallbacks& { return s_DockSpaceCallbacks; }

}
#endif // MIKOTO_EDITOR_HH
