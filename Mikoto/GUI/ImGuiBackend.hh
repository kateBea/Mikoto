//
// Created by zanet on 2/16/2025.
//

#ifndef BACKENDIMPLEMENTATION_HH
#define BACKENDIMPLEMENTATION_HH

#include <Platform/Window.hh>
#include <Renderer/RendererBackend.hh>

namespace Mikoto {
    struct ImGuiBackendCreateInfo {
        const Window* Handle{};
        GraphicsAPI API{ GraphicsAPI::VULKAN_API };
        GpuDevice* Device{ nullptr };
    };

    /**
   * This class encapsulates backend implementation-specific details. ImGui is a graphics API
   * agnostic GUI library and provides several implementations, each for a specific graphics backend.
   * This class serves as a general abstraction over the currently active backend in use in the application
   * that will also be used with ImGui
   * */
    class ImGuiBackend {
    public:
        explicit ImGuiBackend( const ImGuiBackendCreateInfo& createInfo )
            : m_Window{ createInfo.Handle }, m_Api{ createInfo.API }
        {}

        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        virtual ~ImGuiBackend() = default;

        MKT_NODISCARD static auto Create(const ImGuiBackendCreateInfo& info) -> Scope_T<ImGuiBackend>;

    protected:
        const Window* m_Window{};
        GraphicsAPI m_Api{ GraphicsAPI::VULKAN_API };
    };
}// namespace Mikoto
#endif//BACKENDIMPLEMENTATION_HH
