//
// Created by zanet on 1/29/2025.
//

#ifndef RENDERCONTEXT_HH
#define RENDERCONTEXT_HH

#include <Common/Singleton.hh>
#include <Core/Engine.hh>
#include <Platform/Window/Window.hh>
#include <Renderer/Core/RendererBackend.hh>

namespace Mikoto {
    struct RenderContextCreateInfo {
        const Window* TargetWindow{ nullptr };
        GraphicsAPI GraphicsAPI{ GraphicsAPI::VULKAN_API };
    };

    class RenderContext {
    public:
        struct RenderContextData {
            const Window* TargetWindow{ nullptr };

            GraphicsAPI GraphicsAPI{ GraphicsAPI::VULKAN_API };
            Scope_T<RendererBackend> DefaultRenderer{ nullptr };
        };

    public:

        virtual ~RenderContext() = default;


        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        auto GetContextData() -> RenderContextData& { return m_ContextData; }

        virtual auto SubmitFrame() -> void = 0;
        virtual auto PrepareFrame() -> void = 0;

        virtual auto EnableVSync() -> void = 0;
        virtual auto DisableVSync() -> void = 0;

        static auto Create(const RenderContextCreateInfo& config) -> Scope_T<RenderContext>;


    protected:
        explicit RenderContext() = default;

        explicit RenderContext(const RenderContextCreateInfo& createInfo)
            :   m_ContextData{ .TargetWindow{ createInfo.TargetWindow }, .GraphicsAPI{ createInfo.GraphicsAPI } }
        { }

    protected:
        RenderContextData m_ContextData{};

    };
}// namespace Mikoto


#endif//RENDERCONTEXT_HH
