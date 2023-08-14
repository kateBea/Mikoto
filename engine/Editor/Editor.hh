//
// Created by kate on 6/22/23.
//

#ifndef KATE_ENGINE_EDITOR_HH
#define KATE_ENGINE_EDITOR_HH

#include <string_view>

#include <imgui.h>

#include <Utility/Common.hh>

namespace Mikoto::Editor {

    struct DockControlFlags {
        bool ApplicationCloseFlag{};

        bool HierarchyPanelVisible{ true };
        bool InspectorPanelVisible{ true };
        bool ScenePanelVisible{ true };
        bool SettingPanelVisible{ true };
        bool StatsPanelVisible{ true };
    };

    inline DockControlFlags s_ControlFlags{};

    auto ThemeDarkModeDefault() -> void;
    auto ThemeDarkModeAlt() -> void;

    auto ShowDockingDisabledMessage() -> void;
    auto HelpMarker(std::string_view description) -> void;
    auto OnDockSpaceUpdate() -> void;

    KT_NODISCARD inline auto GetControlFlags() -> const DockControlFlags& { return s_ControlFlags; }


}
#endif//KATE_ENGINE_EDITOR_HH
