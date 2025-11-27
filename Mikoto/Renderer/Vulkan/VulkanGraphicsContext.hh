//
// Created by kate on 11/25/25.
//

#ifndef MIKOTO_VULKANGRAPHICSCONTEXT_HH
#define MIKOTO_VULKANGRAPHICSCONTEXT_HH

#include <utility>
#include <ankerl/unordered_dense.h>

#include <volk.h>

#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {

    class VulkanGraphicsContext final : public GraphicsContext {
    public:

        explicit  VulkanGraphicsContext();

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto BeginRender() -> void override;
        auto EndRender() -> void override;
        auto BeginCompute() -> void override;
        auto EndCompute() -> void override;
        auto SetRenderTarget( TextureHandle texture ) -> void override;
        auto SetRenderTarget( TextureHandle color, TextureHandle depth ) -> void override;
        auto SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void override;
        auto SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void override;
        auto ClearColor( std::string_view resourceName, TextureHandle colorTarget, const Vec4F &color ) -> void override;
        auto ClearColor( std::string_view resourceName, TextureHandle colorTarget, float r, float g, float b, float a ) -> void override;
        auto ClearDepth( std::string_view resourceName, TextureHandle depthTarget, float depth ) -> void override;
        auto BindPipeline( PipelineHandle pipeline ) -> void override;
        auto BindBuffer( BufferHandle texture ) -> void override;
        auto BindTexture( TextureHandle texture ) -> void override;
        auto BindSampler( SamplerHandle sampler ) -> void override;
        auto Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void override;

        ~VulkanGraphicsContext() override = default;
        auto CreateCommandList() -> PassCommandList * override;
        auto SubmitCommandList( PassCommandList *cmd ) -> void override;

    private:
        // Information I store for each pass
        struct FramePassInfo {
            std::string Name{};

            PipelineHandle Pipeline{};

            TextureHandle DepthRenderTarget{};
            std::vector<TextureHandle> ColorRenderTargets{};

            // Set index -> Descriptor Set handle
            ankerl::unordered_dense::map<UInt32, VkDescriptorSet> DescriptorSets{};

            ankerl::unordered_dense::map<std::pair<UInt32, UInt32>, BufferHandle> BoundBuffers{};
            ankerl::unordered_dense::map<std::pair<UInt32, UInt32>, TextureHandle>  BoundTextures{};

            // Meshes are submitted as index
            // buffer and its vertices

            // We will issue as many draws as we need for each pair
            ankerl::unordered_dense::map<std::pair<Buffer*, Buffer*>, UInt32>  MeshData{};
        };

    private:
        ankerl::unordered_dense::map<FramePass*, FramePassInfo> m_PassInfo{};
    };
}


#endif//MIKOTO_VULKANGRAPHICSCONTEXT_HH
