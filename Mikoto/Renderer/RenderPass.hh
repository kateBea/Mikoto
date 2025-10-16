//
// Created by zanet on 4/14/2025.
//

#ifndef RENDERPASS_HH
#define RENDERPASS_HH

#include <Renderer/GpuDevice.hh>
#include <Renderer/RendererBackend.hh>

#include <Renderer/Pipeline.hh>

namespace Mikoto {

    class IPass {
    public:
        virtual ~IPass() = default;

        virtual auto Execute() -> void {}
        virtual auto Init(GpuDevice* device) -> void = 0;
        virtual auto Shutdown() -> void = 0;
    };


    class IRenderPass : public IPass {
    public:
        ~IRenderPass() override = default;

        virtual auto Render(const FrameContext& frame, RendererBackend* backend) -> void = 0;

        virtual auto OnResize(UInt32 width, UInt32 height) -> void {}
    };

    // Final composition PBR shading pass
    class ShadingPass final : public IRenderPass {
    public:
        auto Init(GpuDevice* device) -> void override;

        auto Shutdown() -> void override;

        auto SetRenderData(Scene* scene, std::span<Light> lights) -> void;

        auto Render(const FrameContext& frame, RendererBackend* backend) -> void override;

        auto OnResize(UInt32 width, UInt32 height) -> void override;

    private:
        GpuDevice* m_Device{};

        Scene* m_Scene{};
        std::span<Light> m_Lights{};

        PipelineHandle m_Pipeline{};
        TextureHandle m_ColorTarget{};
        TextureHandle m_DepthTarget{};
    };

    // Dummy compute pipeline we will use for testing only for now
    class ComputeBasic final : public IPass {
    public:
        auto Init(GpuDevice* device) -> void override;

        auto Shutdown() -> void override;

        auto Execute() -> void override;

    private:
        GpuDevice* m_Device{};

        PipelineHandle m_Pipeline{};
    };
}

#endif//RENDERPASS_HH
