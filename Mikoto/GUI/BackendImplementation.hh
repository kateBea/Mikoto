//
// Created by zanet on 2/16/2025.
//

#ifndef BACKENDIMPLEMENTATION_HH
#define BACKENDIMPLEMENTATION_HH

#include <Models/Enums.hh>
#include <Platform/Window/Window.hh>

namespace Mikoto {
    struct ImGuiBackendCreateInfo {
        const Window* Handle{};
        GraphicsAPI API{ GraphicsAPI::VULKAN_API };
    };

    /**
   * This class encapsulates backend implementation specific details. ImGui is a graphics API
   * agnostic GUI library and provides several implementations, each for a specific graphics backend.
   * This class serves as a general abstraction over the currently active backend in use in the application
   * that will also be used with ImGui
   * */
    class BackendImplementation {
    public:
        explicit BackendImplementation( const ImGuiBackendCreateInfo& createInfo )
            : m_Window{ createInfo.Handle }, m_Api{ createInfo.API }

        {}

        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        virtual ~BackendImplementation() = default;

    protected:
        const Window* m_Window{};
        GraphicsAPI m_Api{ GraphicsAPI::VULKAN_API };
    };
}// namespace Mikoto
#endif//BACKENDIMPLEMENTATION_HH
