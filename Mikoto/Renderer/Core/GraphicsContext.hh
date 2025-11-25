//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_RENDERCONTEXT_HH
#define MIKOTO_RENDERCONTEXT_HH

#include <Assets//Texture.hh>
#include <Assets/Model.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Core/Pipeline.hh>
#include <string_view>

#include "RenderService.hh"

namespace Mikoto {

    enum class RenderTargetType {
        COLOR,
        DEPTH,
    };

    struct PipelineDescription {
        PipelineType Type{};

        // With std::variant
        ComputePipelineDescription ComputeDesc{};
        GraphicsPipelineDescription GraphicsDesc{};

        std::vector<std::string> Shaders{};

        auto AddShader(std::string_view path) -> void;
    };

    class GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

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

        // Create Resources
        virtual auto CreateNamedBuffer(std::string_view name, BufferDescription description) -> void = 0;
        virtual auto CreateNamedPipeline(std::string_view name, PipelineDescription description) -> void = 0;
        virtual auto CreateNamedRenderTarget(std::string_view name, TextureDescription description, RenderTargetType) -> void = 0;


        virtual auto BindBuffer(BufferHandle texture) -> void = 0;
        virtual auto BindTexture(TextureHandle texture) -> void = 0;
        virtual auto BindSampler(SamplerHandle sampler) -> void = 0;

        virtual auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void;

        MKT_NODISCARD static auto Create(GraphicsAPI api ) -> Unique<GraphicsContext>;
    };

    class PassCommandList {
    public:

        explicit PassCommandList(GraphicsContext* context);

        auto BeginRender() -> void;
        auto EndRender() -> void;

        auto BeginCompute() -> void;
        auto EndCompute() -> void;

        auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void ;
        auto SetScissor(Int32 x, Int32 y, Int32 width, Int32 height) -> void;

        auto BindPipeline(std::string_view pipelineName) -> void;

        // TODO
        auto BindVertexBuffer(BufferHandle vertices) -> void;
        auto BindIndexBuffer(BufferHandle indices) -> void;
        auto DrawIndexed() -> void;

        auto Dispatch(UInt32 invX, UInt32 invY, UInt32 invZ) -> void;

        // Or maybe by name?
        auto BindBuffer(BufferHandle texture) -> void;
        auto BindTexture(TextureHandle texture) -> void;

        // A sampler tells how we sample from a texture when we bind a sampler we
        // need to speficy the texture we will be sampling from with this sampler
        auto BindSampler(SamplerHandle sampler, std::string_view textureName) -> void;
        auto BindSampler(SamplerHandle sampler, TextureHandle texture) -> void;


    private:
        GraphicsContext* m_Context{};
        // Draw state


        // Descriptor state


        // Pass state

    };
}

#endif//MIKOTO_RENDERCONTEXT_HH
