//
// Created by zanet on 1/26/2025.
//

#ifndef RENDERSYSTEM_HH
#define RENDERSYSTEM_HH

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Platform/Window.hh>
#include <Renderer/RenderContext.hh>

#include "Renderer/GpuDevice.hh"

namespace Mikoto {

    struct RenderServiceCreateInfo {
        Window* TargetWindow{ nullptr };
        GraphicsAPI RendererAPI{ GraphicsAPI::VULKAN_API };
    };

    class RenderService final : public IService, public Singleton<RenderService> {
    public:
        explicit RenderService(const RenderServiceCreateInfo& options);

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float ts) -> void override;

        ~RenderService() override = default;

        auto PrepareFrame() const -> void;
        auto EndFrame() -> void;

        MKT_NODISCARD auto GetContext() -> RenderContext* { return m_Context.get(); }
        MKT_NODISCARD auto GetContext() const -> const RenderContext* { return m_Context.get(); }

        MKT_NODISCARD auto GetGpuDevice() -> GpuDevice* { return m_Context->GetGpuDevice(); }
        MKT_NODISCARD auto GetGpuDevice() const -> const GpuDevice* { return m_Context->GetGpuDevice(); }

        MKT_NODISCARD auto IsGraphicsActive( GraphicsAPI api ) const -> bool;
        MKT_NODISCARD auto GetActiveGraphicsApi() const -> GraphicsAPI { return m_ActiveAPI; }

    private:
        auto Flush() -> void;

    private:
        RenderServiceCreateInfo m_Options{};
        Unique<RenderContext> m_Context{ nullptr };

        GraphicsAPI m_ActiveAPI{ GraphicsAPI::VULKAN_API };
    };
}
#endif //RENDERSYSTEM_HH
