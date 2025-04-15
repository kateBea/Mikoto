//
// Created by zanet on 4/14/2025.
//

#ifndef RENDERPASS_HH
#define RENDERPASS_HH
#include "Renderer/FrameGraph.hh"


namespace Mikoto {

    class RenderPass {
    public:
        virtual ~RenderPass() = default;

        virtual auto PreRender(/* ... */) -> void = 0;
        virtual auto Execute(/* ... */) -> void = 0;
        virtual auto PostRender(/* ... */) -> void = 0;

    protected:

        bool m_IsCompute{ false };
    };

    class GBufferPass final : public RenderPass {

    public:

        struct GBufferData {
            FrameGraphResource Albedo{};
            FrameGraphResource Normal{};
            FrameGraphResource Position{};
        };

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;
    };

    class ShadingPass final : public RenderPass {
    public:

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;

    };

    class ShadowPass final : public RenderPass {
    public:

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;
    };

    class LightGridPass final : public RenderPass {
    public:

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;
    };

    class LightCullingPass final : public RenderPass {
    public:

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;
    };

    class WorldTextPass final : public RenderPass {
    public:

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;
    };

    class OverlayTextPass final : public RenderPass {
    public:

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;
    };

    class ObjectOutlinePass final : public RenderPass {
    public:

        auto PreRender( /* ... */ ) -> void override;
        auto Execute( /* ... */ ) -> void override;
        auto PostRender( /* ... */ ) -> void override;
    };


}



#endif //RENDERPASS_HH
