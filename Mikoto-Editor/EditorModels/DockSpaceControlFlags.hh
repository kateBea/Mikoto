//
// Created by kate on 1/6/25.
//

#ifndef DOCKSPACECONTROLFLAGS_HH
#define DOCKSPACECONTROLFLAGS_HH

namespace Mikoto {
    struct DockControlFlags {
        bool ApplicationCloseFlag{};

        bool HierarchyPanelVisible{ true };
        bool InspectorPanelVisible{ true };
        bool ScenePanelVisible{ true };
        bool SettingPanelVisible{ true };
        bool StatsPanelVisible{ true };
        bool ContentBrowser{ true };
        bool ConsolePanel{ true };
        bool RendererPanel{ true };
    };
}
#endif //DOCKSPACECONTROLFLAGS_HH
