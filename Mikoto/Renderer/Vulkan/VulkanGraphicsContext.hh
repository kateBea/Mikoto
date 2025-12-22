//
// Created by kate on 11/25/25.
//

#ifndef MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
#define MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH

#include <utility>
#include <ankerl/unordered_dense.h>

#include <volk.h>

#include <Assets/Texture.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameGraph.hh>
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

        auto BeginFrame(FrameBlackboard* blackboard)-> void  override;
        auto EndFrame()-> void  override;

        auto BindPipeline( PipelineHandle pipeline, PassResources& resources ) -> void override;
        auto BindBuffer( BufferHandle texture ) -> void override;

        auto Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void override;
        auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance) -> void  override;

        auto SetViewport(const PassViewport& vp) -> void  override;
        auto SetScissor(const PassScissor& vp) -> void  override;

        auto RegisterImage( TextureHandle texture ) -> void override;
        auto RegisterImage( TextureHandle texture, SamplerHandle sampler ) -> void override;

        ~VulkanGraphicsContext() override = default;

    private:
        auto CreatePassDescriptors(IPipeline* pipeline, PassResources& resources) -> void;
        auto PushBuffer( IPipeline* pipeline, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType) -> void;
        auto PushImage( IPipeline * pipeline, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType) -> void;

    private:
        // Information I store for each pass
        struct FramePassInfo {
            // Set index -> Descriptor Set handle
            ankerl::unordered_dense::map<UInt32, VkDescriptorSet> DescriptorSets{};

            ankerl::unordered_dense::map<std::pair<UInt32, UInt32>, BufferHandle> BoundBuffers{};
            ankerl::unordered_dense::map<std::pair<UInt32, UInt32>, TextureHandle>  BoundTextures{};
        };

        // TODO: Before we start rendering passes
        // Descriptors are bound to a buffer so if we want to share descriptors they must be of same cmd buffer
        CommandListHandle m_CmdList{};

        FrameBlackboard* m_Blackboard{};
    private:
        ankerl::unordered_dense::map<IPipeline*, FramePassInfo> m_PassInfo{};
        ankerl::unordered_dense::map<std::pair<Texture*, Sampler*>, UInt32> m_CombinedSamplerIndices{};
    };
}


#endif//MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
