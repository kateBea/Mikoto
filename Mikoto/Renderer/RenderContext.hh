//
// Created by zanet on 1/29/2025.
//

#ifndef RENDERCONTEXT_HH
#define RENDERCONTEXT_HH

#include <Common/Singleton.hh>
#include <Platform/Window.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    struct RenderContextCreateInfo {
        const Window* TargetWindow{ nullptr };
    };

    class RenderContext {
    public:
        virtual ~RenderContext() = default;

        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto SubmitFrame() -> void = 0;
        virtual auto PrepareFrame() -> void = 0;

        virtual auto EnableVSync() -> void = 0;
        virtual auto DisableVSync() -> void = 0;

        static auto Create(const RenderContextCreateInfo& config) -> Unique<RenderContext>;

    protected:

        explicit RenderContext(const RenderContextCreateInfo& createInfo)
            :   m_TargetWindow{ createInfo.TargetWindow }
        { }

    protected:

        const Window* m_TargetWindow{ nullptr };
        //Unique<GpuDevice> m_GraphicsDevice{};
    };
}// namespace Mikoto


#endif//RENDERCONTEXT_HH
