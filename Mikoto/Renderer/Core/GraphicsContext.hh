//
// Created by kate on 11/24/25.
//

#ifndef MIKOTO_RENDERCONTEXT_HH
#define MIKOTO_RENDERCONTEXT_HH

#include <string_view>

#include <Assets/Model.hh>
#include <Assets//Texture.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/Light.hh>

#include <Material/Material.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    // By convention renderers will expose to the shaders 3 sets
    // layout(set = 0, binding = 0) uniform FrameUBO { ... };
    // layout(set = 0, binding = 1) uniform LightUniformBuffer { ... };
    // layout(set = 1, binding = 0) uniform sampler2D g_BindlessTextures[];
    // layout(set = 2, binding = 0) readonly buffer InstanceData { ... };
    // Any shader may access these if declared properly
    class GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

        virtual auto BeginRender() -> void = 0;
        virtual auto EndRender() -> void = 0;

        // Render state
        virtual auto SetRenderTarget(TextureHandle texture) -> void = 0;
        virtual auto SetRenderTarget(TextureHandle color, TextureHandle depth) -> void = 0;

        virtual auto SetScissor(Int32 x, Int32 y) -> void = 0;
        virtual auto SetViewport(Int32 x, Int32 y, Int32 width, Int32 height) -> void = 0;

        virtual auto ClearColor(TextureHandle colorTarget, const Vec4F& color) -> void = 0;
        virtual auto ClearColor(TextureHandle colorTarget, float r, float g, float b, float a) -> void = 0;
        virtual auto ClearDepth(TextureHandle depthTarget, float depth) -> void = 0;

        virtual auto BindPipeline(PipelineHandle pipeline) -> void = 0;

        virtual auto BindBuffer(BufferHandle texture) -> void = 0;
        virtual auto BindTexture(TextureHandle texture) -> void = 0;
        virtual auto BindSampler(SamplerHandle sampler) -> void = 0;

        virtual auto RegisterLight(LightObject* light ) -> void = 0;

        virtual auto Dispatch() -> void = 0;

        virtual auto Draw(MeshNode* node, MaterialHandle material, const Mat4F& transform) -> void;
        virtual auto Draw(std::string_view text, MaterialHandle material, const Mat4F& transform) -> void;
    };
}

#endif//MIKOTO_RENDERCONTEXT_HH
