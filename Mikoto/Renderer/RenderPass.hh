//
// Created by zanet on 4/14/2025.
//

#ifndef RENDERPASS_HH
#define RENDERPASS_HH
#include "Renderer/FrameGraph.hh"


namespace Mikoto {
    struct DirectionalLight;
}
namespace Mikoto {
    struct PointLight;
}
namespace Mikoto {
    struct SpotLight;
}
namespace Mikoto {

    class RenderPass : ReferenceCounted {
    public:
        ~RenderPass() override = default;

        virtual auto Init(GpuDevice* device) -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto PreRender(GpuDevice* device, FrameBlackboard* blackboard) -> void {}
        virtual auto Execute(GpuDevice* device, FrameBlackboard* blackboard) -> void {}
        virtual auto PostRender(GpuDevice* device, FrameBlackboard* blackboard) -> void {}
        virtual auto SetRenderSize(GpuDevice* device,UInt32_T width, UInt32_T height) -> void {}

        MKT_NODISCARD auto GetRenderWidth() const -> UInt32_T { return m_RenderWidth; }
        MKT_NODISCARD auto GetRenderHeight() const -> UInt32_T { return m_RenderHeight; }

        MKT_NODISCARD auto IsEnabled() const -> bool { return m_IsEnabled; }
        MKT_NODISCARD auto IsCompute() const -> bool { return m_IsCompute; }

        auto SetEnabled( const bool enabled) -> void { m_IsEnabled = enabled; }
    protected:
        RenderPass( const UInt32_T viewportWidth, const UInt32_T viewportHeight)
            : m_RenderWidth{ viewportWidth }, m_RenderHeight{ viewportHeight } {}

    protected:
        UInt32_T m_RenderWidth{};
        UInt32_T m_RenderHeight{};

        bool m_IsCompute{ false };

        bool m_IsEnabled{ true };
    };

    // Generate the gbuffer attachments
    class GBufferPass final : public RenderPass {
    public:

        explicit GBufferPass(UInt32_T viewportWidth, UInt32_T viewportHeight);

        struct GBufferData {
            TextureHandle Albedo{};
            TextureHandle Normal{};
            TextureHandle Position{};
        };

        auto Init(GpuDevice* device) -> void override;
        auto Shutdown() -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    private:
        GBufferData m_GBuffer{};

        UInt32_T m_ViewportWidth{};
        UInt32_T m_ViewportHeight{};

        FramebufferHandle m_Framebuffer{};

        GraphicsPipelineHandle m_Pipeline{};

        ankerl::unordered_dense::map<ShaderStage, std::string> m_ShaderPaths{};
    };

#if false
    // compute pass to generate light clusters
    class LightGridPass final : public RenderPass {
    public:

        auto PreRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto PostRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    private:

        BufferHandle m_LightGridBuffer{};

        // We will take this as input
        std::span<const SpotLight*> m_SpotLights{};
        std::span<const PointLight*> m_PointLights{};
        std::span<const DirectionalLight*> m_DirectionalLights{};
    };

    // Compute pass to cull lights per tile
    class LightCullingPass final : public RenderPass {
    public:

        auto PreRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto PostRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    public:
        BufferHandle m_LightGridBuffer{};
        BufferHandle m_LightCullingBuffer{};

        // We will take this as input
        std::span<const SpotLight*> m_SpotLights{};
        std::span<const PointLight*> m_PointLights{};
        std::span<const DirectionalLight*> m_DirectionalLights{};
    };

    // Apply lighting and most likely final step to generate the final image
    class ShadingPass final : public RenderPass {
    public:

        auto PreRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto PostRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    private:
        std::span<Entity*> m_Entities{};
    };

    // Generate shadows for the active lights in the scene
    class ShadowPass final : public RenderPass {
    public:

        auto PreRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto PostRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    private:
        const Camera* m_TargetCamera{};
        std::span<Entity*> m_Entities{};
    };

    // Draw text in the world
    class WorldTextPass final : public RenderPass {
    public:

        auto PreRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto PostRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    private:
        std::span<Entity*> m_TextEntities{};
    };

    // Drawn after the last mesh render pass which is shading pass for now
    class OverlayTextPass final : public RenderPass {
    public:

        auto PreRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto PostRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    private:
        // Text is drawn on top of final image because is overlay
        BufferHandle m_FinalImage{};

        std::span<Entity*> m_TextEntities{};
    };

    // Draw outline effects on a given mesh
    class ObjectOutlinePass final : public RenderPass {
    public:

        auto PreRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto Execute(GpuDevice* device, FrameBlackboard* blackboard ) -> void override;
        auto PostRender( GpuDevice* device, FrameBlackboard* blackboard ) -> void override;

    private:
        // drawn on top of 3d text because 3d text is part of the world
        BufferHandle m_FinalImage{};

        std::span<Entity*> m_TargetMeshes{};
    };
#endif

}

#endif //RENDERPASS_HH
