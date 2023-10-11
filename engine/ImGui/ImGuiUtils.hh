/**
 * Created by kate on 9/17/23.
 * */

#ifndef MIKOTO_IMGUI_UTILS_HH
#define MIKOTO_IMGUI_UTILS_HH

// C++ Standard Library
#include <any>

namespace Mikoto {
    /**
     * This class encapsulates backend implementation specific details. ImGui is a graphics API
     * agnostic GUI library and provides several implementations, each for a specific graphics backend.
     * This class serves as a general abstraction over the currently active backend in use in the application
     * that will also be used with ImGui
     * */
    class BackendImplementation {
    public:
        virtual auto Init(std::any windowHandle) -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        virtual ~BackendImplementation() = default;
    };
}

#endif // MIKOTO_IMGUI_UTILS_HH
