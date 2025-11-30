//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_RENDERCONTEXT_HH
#define MIKOTO_RENDERCONTEXT_HH

#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>

namespace Mikoto {
    class PassCommandList;

    struct PassViewport {
        UInt32 X{}, Y{}, Width{}, Height{};
    };

    struct PassScissor {
        UInt32 X{}, Y{}, Width{}, Height{};
    };

    class GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginRender() -> void = 0;
        virtual auto EndRender() -> void = 0;

        virtual auto BeginCompute() -> void = 0;
        virtual auto EndCompute() -> void = 0;

        // Render state
        virtual auto SetRenderTarget(TextureHandle texture) -> void = 0;
        virtual auto SetRenderTarget(TextureHandle color, TextureHandle depth) -> void = 0;

        virtual auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void = 0;
        virtual auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void = 0;

        virtual auto ClearColor(std::string_view resourceName, TextureHandle colorTarget, const Vec4F& color) -> void = 0;
        virtual auto ClearColor(std::string_view resourceName,TextureHandle colorTarget, float r, float g, float b, float a) -> void = 0;
        virtual auto ClearDepth(std::string_view resourceName,TextureHandle depthTarget, float depth) -> void = 0;

        virtual auto BindPipeline(PipelineHandle pipeline) -> void = 0;

        virtual auto CreateCommandList() -> PassCommandList* = 0;

        virtual auto SubmitCommandList(PassCommandList* cmd) -> void = 0;

        virtual auto BindBuffer(BufferHandle texture) -> void = 0;

        virtual auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void = 0;




        // Current confirmed API ============================================
        MKT_NODISCARD auto GetDevice() const -> GpuDevice* { return  m_Device; }

        virtual auto RegisterImage(TextureHandle texture) -> void = 0;
        virtual auto RegisterImage(TextureHandle texture, SamplerHandle sampler) -> void = 0;

        MKT_NODISCARD static auto Create(GpuDevice* device) -> Unique<GraphicsContext>;

    protected:
        explicit GraphicsContext(GpuDevice* device)
            : m_Device{ device } {}

    protected:
        GpuDevice* m_Device{ nullptr };
    };

    class PassCommandList {
    public:

        auto Begin() -> void;
        auto End() -> void;

        explicit PassCommandList(GraphicsContext* context);

        auto BeginRender() -> void;
        auto EndRender() -> void;

        auto SetColorRenderTarget(TextureHandle color) -> void;
        auto SetColorRenderTarget(std::string_view color) -> void;

        auto SetDepthRenderTarget(TextureHandle color) -> void;
        auto SetDepthRenderTarget(std::string_view color) -> void;

        auto BeginCompute() -> void;
        auto EndCompute() -> void;

        auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void ;
        auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void;

        auto BindPipeline(std::string_view pipelineName) -> void;

        // TODO
        auto BindVertexBuffer(BufferHandle vertices) -> void;
        auto BindIndexBuffer(BufferHandle indices) -> void;
        auto SubmitDraw() -> void;
        auto BindBuffer(BufferHandle buffer, UInt32 set, UInt32 index) -> void;
        auto BindBuffer(std::string_view buffer, UInt32 set, UInt32 index) -> void;



        // Current confirmed API ============================================
        // These two will use the default sampler
        auto BindTexture(TextureHandle texture ) const -> void;
        auto BindTexture(std::string_view texture) const -> void;

        auto BindTexture(TextureHandle texture, SamplerHandle sampler ) const -> void;

        auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void;

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
        GraphicsContext* m_Context{};

        // Draw state
        std::vector<SamplerHandle> m_BoundSamplers{};
        std::vector<TextureHandle> m_BoundTextures{};

        std::vector<BufferHandle> m_BoundBuffers{};

        TextureHandle m_DepthRenderTarget{};
        std::vector<TextureHandle> m_ColorRenderTargets{};

        ankerl::unordered_dense::map<std::pair<Buffer*, Buffer*>, DrawInstanceMetadata>  MeshData{};

        // Descriptor state


        // Pass state
        PassViewport m_Viewport{};
        PassViewport m_Scissor{};

    };
}

#endif//MIKOTO_RENDERCONTEXT_HH
