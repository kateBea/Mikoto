//
// Created by zanet on 4/14/2025.
//

#ifndef RENDERPASS_HH
#define RENDERPASS_HH

#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>

#include "GpuDevice.hh"

#include <Scene/Scene.hh>

#include "Light.hh"

namespace Mikoto {

    class RendererBackend;

    class RenderPass : ReferenceCounted {
    public:
        ~RenderPass() override = default;

        virtual auto Init(GpuDevice* device) -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto Execute(RendererBackend* backend) -> void = 0;

        MKT_NODISCARD auto IsEnabled() const -> bool { return m_IsEnabled; }
        MKT_NODISCARD auto IsCompute() const -> bool { return m_IsCompute; }

        auto SetEnabled( const bool enabled) -> void { m_IsEnabled = enabled; }
    protected:
        RenderPass() = default;

    protected:

        bool m_IsCompute{ false };

        bool m_IsEnabled{ true };

        std::vector<RefAny> m_InputResources{};
        std::vector<RefAny> m_OutputResources{};
    };

    // Generate the gbuffer attachments
    class GBufferPass final : public RenderPass {
    public:

        struct GBufferPassDescription {
            UInt32_T ViewportWidth{ 0 };
            UInt32_T ViewportHeight{ 0 };

            std::vector<std::string> m_ShaderPaths{};
        };

        explicit GBufferPass(const GBufferPassDescription& description);

        struct GBufferData {
            TextureHandle Albedo{};
            TextureHandle Normal{};
            TextureHandle Position{};
        };

        auto Init(GpuDevice* device) -> void  override;
        auto Shutdown() -> void  override;

        auto Execute(RendererBackend* backend) -> void  override;

    private:
        Scene* m_Scene{ nullptr };

        GBufferData m_GBuffer{};

        FramebufferHandle m_Framebuffer{};
        GraphicsPipelineHandle m_Pipeline{};

        Int32_T m_ViewportWidth{ 0 };
        Int32_T m_ViewportHeight{ 0 };
    };

#if false
    // Pass to cull lights in the scene
    // Generates the light grid and culling buffer
    class LightCullingPass final : public RenderPass {
    public:

        auto Init(GpuDevice* device) -> void  override;
        auto Shutdown() -> void  override;

        auto Execute(RendererBackend* backend) -> void  override;

    public:
        BufferHandle m_LightClusters{};
        BufferHandle m_LightCulling{};

        std::vector<SpotLight> m_SpotLights{};
        std::vector<PointLight> m_PointLights{};
        std::vector<DirectionalLight> m_DirectionalLights{};
    };

    // Final PBR composition. Apply lighting
    class GeometryPass final : public RenderPass {
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
