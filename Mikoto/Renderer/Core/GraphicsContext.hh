//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_RENDERCONTEXT_HH
#define MIKOTO_RENDERCONTEXT_HH

#include <vector>
#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/FrameBlackboard.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace Mikoto {
    class FramePass;
    class PassCommandList;

    struct PassViewport {
        float X{}, Y{}, Width{}, Height{};
    };

    struct PassScissor {
        float X{}, Y{}, Width{}, Height{};
    };

    struct GfxRenderInfo {
        Vec4F ClearColor{};
        TextureHandle DepthRenderTarget{};
        std::vector<TextureHandle> ColorRenderTargets{};
    };

    struct PassResources {
        FrameBlackboard* Blackboard{ nullptr };
        ankerl::unordered_dense::map<UInt32, std::vector<ShaderResourceInfo>> Bindings{};
    };

    class GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginRender(GfxRenderInfo& info) -> void = 0;
        virtual auto EndRender(GfxRenderInfo& info) -> void = 0;

        virtual auto BeginCompute() -> void = 0;
        virtual auto EndCompute() -> void = 0;

        virtual auto BindBuffer(BufferHandle texture) -> void = 0;

        // Current confirmed API ============================================
        virtual auto BeginFrame(FrameBlackboard* blackboard)-> void = 0;
        virtual auto EndFrame()-> void = 0;

        virtual auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void = 0;

        virtual auto RegisterImage(TextureHandle texture) -> void = 0;
        virtual auto RegisterImage(TextureHandle texture, SamplerHandle sampler) -> void = 0;

        virtual auto SetViewport(const PassViewport& vp) -> void = 0;
        virtual auto SetScissor(const PassScissor& vp) -> void = 0;

        virtual auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance) -> void = 0;

        virtual auto BindPipeline(PipelineHandle pipeline, PassResources& resources) -> void = 0;

        MKT_NODISCARD static auto Create(GpuDevice* device) -> Unique<GraphicsContext>;

    protected:
        explicit GraphicsContext(GpuDevice* device)
            : m_Device{ device } {}

    protected:
        GpuDevice* m_Device{ nullptr };
    };

    class PassCommandList {
    public:
        explicit PassCommandList(GraphicsContext* context, FrameBlackboard* blackboard);

        auto BeginRender() -> void;
        auto EndRender() -> void;

        auto SetColorRenderTarget(std::string_view color) -> void;

        auto SetDepthRenderTarget(std::string_view depth) -> void;

        auto BeginCompute() -> void;
        auto EndCompute() -> void;

        auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void ;
        auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void;

        auto BindPipeline(std::string_view pipelineName) -> void;

        // TODO
        auto BindVertexBuffer(BufferHandle vertices) -> void;
        auto BindIndexBuffer(BufferHandle indices) -> void;
        auto SubmitDraw() -> void;

        auto BindStorageBuffer(BufferHandle buffer, UInt32 set, UInt32 index) -> void;
        auto BindStorageBuffer(std::string_view buffer, UInt32 set, UInt32 index) -> void;

        auto BindUniformBuffer(BufferHandle buffer, UInt32 set, UInt32 index) -> void;
        auto BindUniformBuffer(std::string_view buffer, UInt32 set, UInt32 index) -> void;

        // Current confirmed API ============================================
        auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance) -> void;

        // These two will use the default sampler
        auto BindTexture(TextureHandle texture ) const -> void;
        auto BindTexture(std::string_view texture) const -> void;

        auto BindTexture(TextureHandle texture, SamplerHandle sampler ) const -> void;

        auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void;

        auto SetClearColor(const Vec4F& color) -> void;

    private:
        struct DrawInstanceMetadata {
            BufferHandle VertexBuffer{};
            BufferHandle IndexBuffer{};

            // Is the instance ID of the first instance to draw.
            UInt32 FirstInstance{};

            // Is the number of instances to draw.
            UInt32 InstanceCount{};

            // Number of vertices
            UInt32 VertexCount{};

            // Is the number of vertices to draw.
            UInt32 IndexCount{};
        };

    private:
        FrameBlackboard* m_Blackboard{};
        GraphicsContext* m_Context{};

        std::vector<BufferHandle> m_BoundBuffers{};

        ankerl::unordered_dense::map<std::pair<Buffer*, Buffer*>, DrawInstanceMetadata>  MeshData{};

        // Descriptor state


        // Pass state
        PassScissor m_Scissor{};
        PassViewport m_Viewport{};

        // Pass shader resources
        // TODO: Group index -> (ShaderResourceInfo)
        PassResources m_ShaderResources{};

        GfxRenderInfo m_RenderInfo{};

    };
}

#endif//MIKOTO_RENDERCONTEXT_HH
