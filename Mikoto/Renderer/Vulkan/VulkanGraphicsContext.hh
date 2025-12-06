//
// Created by kate on 11/25/25.
//

#ifndef MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
#define MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH

#include <utility>
#include <ankerl/unordered_dense.h>

#include <volk.h>

#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>

namespace Mikoto {

    class VulkanGraphicsContext final : public GraphicsContext {
    public:

        explicit  VulkanGraphicsContext(GpuDevice* device);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginRender(GfxRenderInfo& beginInfo) -> void override;
        auto EndRender(GfxRenderInfo& info) -> void override;

        auto BeginCompute() -> void override;
        auto EndCompute() -> void override;

        auto SetCommandList(CommandListHandle cmd)-> void  override;

        auto BindPipeline( PipelineHandle pipeline ) -> void override;
        auto BindBuffer( BufferHandle texture ) -> void override;

        auto Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void override;
        auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance) -> void  override;

        auto SetViewport(const PassViewport& vp) -> void  override;
        auto SetScissor(const PassScissor& vp) -> void  override;

        auto RegisterImage( TextureHandle texture ) -> void override;
        auto RegisterImage( TextureHandle texture, SamplerHandle sampler ) -> void override;

        ~VulkanGraphicsContext() override = default;
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

        // TODO: Bfore we start rendering passes
        // Descriptors are bound to a buffer so if we want to share descriptors they must be of same cmd buffer
        CommandListHandle m_CmdList{};
    private:
        ankerl::unordered_dense::map<std::pair<Texture*, Sampler*>, UInt32> m_CombinedSamplerIndices{};
        ankerl::unordered_dense::map<FramePass*, FramePassInfo> m_PassInfo{};
    };
}


#endif//MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
