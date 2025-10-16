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
#include <Common/Common.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>
#include <Library/Utility/Types.hh>

#include <Renderer/Light.hh>
#include <Renderer/RenderUtility.hh>

namespace Mikoto {

    struct RendererDescription {
        std::string_view Name{};

        GpuDevice* Device{ nullptr };

        GraphicsAPI RendererAPI{ GraphicsAPI::VULKAN_API };
    };

    struct AttachmentInfo {
        TextureHandle Image{};
        Vec4F ClearColor{ 0.0f };   // Only used if cleared
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

    struct FrameContext {
        CommandListHandle Cmd{};
        UInt32 FrameIndex{ 0 };
        float DeltaTime{ 0.0f };
    };

    class RendererBackend {
    public:
        virtual ~RendererBackend() = default;

        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginRender(const RenderInfo& info ) -> void = 0;
        virtual auto EndRender() -> void = 0;

        virtual auto DrawScene(Scene* scene, std::span<Light> lights) -> void = 0;

        virtual auto BeginRender(const FrameContext& frame, const RenderInfo& info) -> void = 0;

        virtual auto OnResize( UInt32 width, UInt32 height ) -> void = 0;

        auto SetScene( Scene* scene ) -> void;
        auto SetCamera( const Camera* camera ) -> void;
        auto SetViewport( float x, float y, float width, float height ) -> void;

        template<typename... Args>
        auto SetClearColor( Args&&... args ) -> void {
            m_ClearColor = Vec4F{ std::forward<Args>(args)... };
        }

        static auto Create( const RendererDescription& createInfo ) -> Unique<RendererBackend>;

    protected:
        struct ViewportConstraints {
            float Width{ 0 };
            float Height{ 0 };
            float X{ 0 };
            float Y{ 0 };
        };

    protected:
        explicit RendererBackend( const RendererDescription& createInfo );

    protected:
        std::string m_Name{};
        GpuDevice* m_GraphicsDevice{ nullptr };

        ViewportConstraints m_ViewportConstraints{};

        Vec4F m_ClearColor{};

        const Camera* m_Camera{ nullptr };
        const Scene* m_Scene{ nullptr };
    };
}// namespace Mikoto

#endif// MIKOTO_RENDERER_API_HH