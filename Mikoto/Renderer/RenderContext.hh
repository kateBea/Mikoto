//
// Created by zanet on 1/29/2025.
//

#ifndef RENDERCONTEXT_HH
#define RENDERCONTEXT_HH

#include <Common/Singleton.hh>
#include <Platform/Window.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/GpuDevice.hh>

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
}// namespace Mikoto


#endif//RENDERCONTEXT_HH
