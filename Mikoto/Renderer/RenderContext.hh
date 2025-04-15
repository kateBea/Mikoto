//
// Created by zanet on 1/29/2025.
//

#ifndef RENDERCONTEXT_HH
#define RENDERCONTEXT_HH

#include <Common/Singleton.hh>
#include <Platform/Window.hh>
#include <Renderer/RendererBackend.hh>
#include <Renderer/GpuDevice.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    struct RenderContextCreateInfo {
        const Window* TargetWindow{ nullptr };
    };

    class RenderContext : public Singleton<RenderContext> {
    public:
        struct RenderContextData {
            const Window* TargetWindow{ nullptr };
        };

    public:
        ~RenderContext() override = default;


        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        auto GetContextData() -> RenderContextData& { return m_ContextData; }

        virtual auto SubmitFrame() -> void = 0;
        virtual auto PrepareFrame() -> void = 0;

        MKT_NODISCARD auto GetGraphicsDevice() const -> GpuDevice* { return m_GraphicsDevice.get(); }

        virtual auto EnableVSync() -> void = 0;
        virtual auto DisableVSync() -> void = 0;

        static auto Create(const RenderContextCreateInfo& config) -> Scope_T<RenderContext>;

    protected:
        explicit RenderContext() = default;

        explicit RenderContext(const RenderContextCreateInfo& createInfo)
            :   m_ContextData{ .TargetWindow{ createInfo.TargetWindow } }
        { }

    protected:

        RenderContextData m_ContextData{};
        Scope_T<GpuDevice> m_GraphicsDevice{};

    };
}// namespace Mikoto


#endif//RENDERCONTEXT_HH
