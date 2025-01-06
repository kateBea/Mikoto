//
// Created by kate on 1/6/25.
//

#ifndef DOCKSPACECALLBACKS_HH
#define DOCKSPACECALLBACKS_HH
#include <functional>

namespace Mikoto {
    struct DockSpaceCallbacks {
        // This function is called when we click on the Save scene of the File Menu
        std::function<void()> OnSceneSaveCallback{};

        // This function is called when we click on the Load scene of the File Menu
        std::function<void()> OnSceneLoadCallback{};

        // This function is called when we click on the New scene of the File Menu
        std::function<void()> OnSceneNewCallback{};
    };
}
#endif //DOCKSPACECALLBACKS_HH
