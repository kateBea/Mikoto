/**
 * @file RendererBackend.hh
 * @date 6/9/23
 * @author kate
 * */

#ifndef MIKOTO_RENDERER_API_HH
#define MIKOTO_RENDERER_API_HH

// C++ Standard Library
#include <any>
#include <memory>
#include <utility>

// Third-Party Libraries


// Project Headers
#include <Common/Service.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

    struct RendererDescription {
        std::string_view Name{};
        GpuDevice* Device{ nullptr };
    };

    class RendererBackend : public IService {
    public:
        ~RendererBackend() override = default;

        virtual auto EndRender() -> void = 0;
        virtual auto BeginRender(CommandListHandle cmd) -> void = 0;

        virtual auto DrawScene(Scene* scene) -> void = 0;

        virtual auto OnResize( UInt32 width, UInt32 height ) -> void = 0;

        virtual auto SetCamera( const Camera* camera ) -> void = 0;
        virtual auto SetViewport( float x, float y, float width, float height ) -> void = 0;

        virtual auto SetClearColor( float r, float g, float b, float a ) -> void = 0;

        MKT_NODISCARD virtual auto CreateMaterial( /* params */ ) -> MaterialHandle = 0;

        MKT_NODISCARD virtual auto GetFinalComposition() const -> TextureHandle = 0;

    protected:
        explicit RendererBackend( const RendererDescription& createInfo )
            : m_Name{ createInfo.Name }, m_GraphicsDevice{ createInfo.Device } {}

    protected:
        std::string m_Name{};
        GpuDevice* m_GraphicsDevice{ nullptr };
    };
}// namespace Mikoto

#endif// MIKOTO_RENDERER_API_HH