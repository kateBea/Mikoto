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
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <Library/Utility/Types.hh>

#include <Renderer/GpuDevice.hh>

namespace Mikoto {

    struct RendererDescription {
        std::string_view Name{};
        GpuDevice* Device{ nullptr };
    };

    struct AttachmentInfo {
        TextureHandle Image{};
        Vec4F ClearColor{ 0.4f, 0.33f, 0.55f, 1.0f };
        bool Clear{ true };
        bool Store{ true };
    };

    struct DepthAttachmentInfo {
        TextureHandle Image{};
        float ClearDepth{ 1.0f };
        bool Clear{ true };
        bool Store{ false };
    };

    struct RenderInfo {
        DepthAttachmentInfo DepthAttachment{};
        std::vector<AttachmentInfo> ColorAttachments{};
    };

    class RendererBackend : public IService {
    public:
        ~RendererBackend() override = default;

        virtual auto EndRender() -> void = 0;
        virtual auto BeginRender(const RenderInfo& info, CommandListHandle cmd) -> void = 0;

        virtual auto SetPipeline(PipelineHandle pipeline) -> void = 0;

        virtual auto DrawScene(Scene* scene) -> void = 0;

        virtual auto OnResize( UInt32 width, UInt32 height ) -> void = 0;

        virtual auto SetCamera( const Camera* camera ) -> void = 0;
        virtual auto SetViewport( float x, float y, float width, float height ) -> void = 0;

    protected:
        explicit RendererBackend( const RendererDescription& createInfo )
            : m_Name{ createInfo.Name }, m_GraphicsDevice{ createInfo.Device } {}

    protected:
        std::string m_Name{};
        GpuDevice* m_GraphicsDevice{ nullptr };
    };
}// namespace Mikoto

#endif// MIKOTO_RENDERER_API_HH