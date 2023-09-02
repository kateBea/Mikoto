/**
 * GUIManager.cc
 * Created by kate on 9/2/23.
 * */
#ifndef MIKOTO_GUI_MANAGER_HH
#define MIKOTO_GUI_MANAGER_HH

// C++ Standard Library

// Third-Party Libraries

// Project Headers

namespace Mikoto::GUIManager {
    auto Init() -> void;
    auto Shutdown() -> void;

    auto BeginFrame() -> void;
    auto EndFrame() -> void;
}


#endif // MIKOTO_GUI_MANAGER_HH
