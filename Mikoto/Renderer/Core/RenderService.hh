//
// Created by zanet on 1/26/2025.
//

#ifndef RENDERSYSTEM_HH
#define RENDERSYSTEM_HH

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Material/ShaderLibrary.hh>
#include <Platform/Window.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {

    struct RenderContextCreateInfo {
        GraphicsAPI Api{ GraphicsAPI::UNKNOWN };
        const Window* TargetWindow{ nullptr };
    };

    class RenderContext {
    public:
        virtual ~RenderContext() = default;

        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto SubmitFrame() -> void = 0;
        virtual auto PrepareFrame() -> void = 0;

        MKT_NODISCARD auto GetGpuDevice() -> GpuDevice* { return m_Device.get(); }
        MKT_NODISCARD auto GetGpuDevice() const -> const GpuDevice* { return m_Device.get(); }

        virtual auto EnableVSync() -> void = 0;
        virtual auto DisableVSync() -> void = 0;

        static auto Create(const RenderContextCreateInfo& config) -> Unique<RenderContext>;

    protected:

        explicit RenderContext(const RenderContextCreateInfo& createInfo)
            :   m_TargetWindow{ createInfo.TargetWindow }
        { }

        Unique<GpuDevice> m_Device{ nullptr };

        const Window* m_TargetWindow{ nullptr };
    };

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

        MKT_NODISCARD auto GetBackend() -> RendererBackend* { return m_RenderBackend.get(); }
        MKT_NODISCARD auto GetBackend() const -> const RendererBackend* { return m_RenderBackend.get(); }

        MKT_NODISCARD auto GetGpuDevice() -> GpuDevice* { return m_Context->GetGpuDevice(); }
        MKT_NODISCARD auto GetGpuDevice() const -> const GpuDevice* { return m_Context->GetGpuDevice(); }

        MKT_NODISCARD auto IsGraphicsActive( GraphicsAPI api ) const -> bool;
        MKT_NODISCARD auto GetActiveGraphicsApi() const -> GraphicsAPI { return m_ActiveAPI; }

    private:
        auto InitContext() -> void;
        auto InitRendererBackend() -> void;
        auto InitShaderLibrary() -> void;

    private:
        RenderServiceCreateInfo m_Options{};

        Unique<RenderContext> m_Context{};
        Unique<RendererBackend> m_RenderBackend{};
        Unique<ShaderLibrary> m_ShaderLibrary{};

        GraphicsAPI m_ActiveAPI{ GraphicsAPI::VULKAN_API };
    };
}
#endif //RENDERSYSTEM_HH
