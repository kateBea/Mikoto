//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_RENDERCONTEXT_HH
#define MIKOTO_RENDERCONTEXT_HH

#include <string_view>
#include <vector>
#include <initializer_list>

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameBlackboard.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>

#include "Renderer/Core/SRGBase.hh"

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
        FramePass* Pass{ nullptr };
        FrameBlackboard* Blackboard{ nullptr };
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

        virtual auto BeginFrame(FrameBlackboard* blackboard)-> void = 0;
        virtual auto EndFrame()-> void = 0;

        virtual auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void = 0;

        virtual auto SetViewport(const PassViewport& vp) -> void = 0;
        virtual auto SetScissor(const PassScissor& vp) -> void = 0;

        virtual auto PushImage(TextureHandle texture) -> Int32 = 0;

        virtual auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance) -> void = 0;

        virtual auto BindPipeline(PipelineHandle pipeline, FramePass* Pass) -> void = 0;
        virtual auto BindTextureList() -> void = 0;
        virtual auto BindFrameResources() -> void = 0;
        virtual auto BindPassResources(FramePass* pass ) -> void = 0;

        virtual auto GetPassSRG( FramePass* pass ) -> SRGPerPass* = 0;

        virtual auto BindIndexBuffer( BufferHandle indexBuffer )-> void = 0;
        virtual auto BindVertexBuffer( BufferHandle vertexBuffer, UInt32 binding ) -> void = 0;
        virtual auto DrawInstanced( Size indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance )-> void = 0;

        MKT_NODISCARD static auto Create(GpuDevice* device) -> Unique<GraphicsContext>;

    protected:
        explicit GraphicsContext(GpuDevice* device)
            : m_Device{ device } {}

    protected:
        GpuDevice* m_Device{ nullptr };

        ankerl::unordered_dense::map<SRGType, Unique<SRGBase>> m_SRG{};
    };

    struct DrawIndexedState {
        BufferHandle IndexBuffer{};

        // Specifies the buffer and its binding
        std::vector<std::pair<BufferHandle, UInt32>> VertexBuffers{};

        UInt32 IndicesCount{};
        UInt32 InstancesCount{};

        UInt32 FirstIndex{};
        UInt32 VertexOffset{};
        UInt32 FirstInstance{};
    };

    class PassCommandList {
    public:
        explicit PassCommandList(GraphicsContext* context, FrameBlackboard* blackboard);

        auto BeginRender(FramePass* pass) -> void;
        auto EndRender() -> void;

        auto BeginCompute(FramePass* pass) -> void;
        auto EndCompute() const -> void;

        auto SetColorRenderTarget(std::string_view color) -> void;
        auto SetDepthRenderTarget(std::string_view depth) -> void;

        auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void ;
        auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void;

        auto BindPipeline(std::string_view pipelineName ) const -> void;

        auto DrawIndexed(const DrawIndexedState& info) const -> void;

        auto SetBufferBindSlot(SRGType type, std::string_view buffer, UInt32 index ) const -> void;
        auto SetTextureBindSlot(SRGType type, std::string_view buffer, UInt32 index) -> void;

        auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) const -> void;

        auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void;

        auto SetClearColor(const Vec4F& color) -> void;

        auto FillBuffer(std::string_view bufferName, const void* ptrSrc, Size size ) const -> void;
        auto PushTexture(TextureHandle texture ) const -> Int32;

        auto BindResourceGroup(SRGType srgType ) const -> void;

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

        // Pass state
        PassScissor m_Scissor{};
        PassViewport m_Viewport{};


        FramePass* m_ActivePass{ nullptr };

        GfxRenderInfo m_RenderInfo{};

    };
}

#endif//MIKOTO_RENDERCONTEXT_HH
