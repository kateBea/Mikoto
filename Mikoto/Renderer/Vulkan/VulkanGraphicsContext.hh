//
// Created by kate on 11/25/25.
//

#ifndef MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
#define MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH

#include <ankerl/unordered_dense.h>
#include <volk.h>

#include <Assets/Texture.hh>
#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <utility>

#include "VulkanDescriptorManager.hh"

namespace Mikoto {

    class VulkanGraphicsContext final : public GraphicsContext {
    public:

        explicit  VulkanGraphicsContext(GpuDevice* device);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginRender(RenderInfo& beginInfo) -> void override;
        auto EndRender(RenderInfo& info) -> void override;

        auto BeginCompute(FramePass* pass) -> void override;
        auto EndCompute(FramePass* pass) -> void override;

        auto BeginFrame(FrameBlackboard* blackboard)-> void  override;
        auto EndFrame()-> void  override;

        auto BindPipeline( PipelineHandle pipeline, FramePass* Pass ) -> void override;

        auto Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ, FramePass* pass ) -> void override;
        auto Draw(UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance, FramePass* pass) -> void  override;

        auto BindIndexBuffer( BufferHandle indexBuffer, FramePass* pass )-> void  override;
        auto BindVertexBuffer( BufferHandle vertexBuffer, UInt32 binding, FramePass* pass ) -> void  override;
        auto DrawInstanced( Size indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance, FramePass* pass )-> void  override;

        auto SetViewport(const PassViewport& vp, FramePass* pass) -> void  override;
        auto SetScissor(const PassScissor& vp, FramePass* pass) -> void  override;

        auto BindTextureList(FramePass* pass) -> void override;
        auto BindFrameResources() -> void  override;

        auto PushImage(TextureHandle texture) -> Int32 override;

        auto BindPassResources(FramePass* pass) -> void override;

        auto GetPassSRG( FramePass* pass ) -> SRGPerPass* override;

        auto InsertResourceBarrier( FramePass * pass ) -> void override;

        ~VulkanGraphicsContext() override = default;

    private:
        MKT_NODISCARD auto HasDescriptorSets(FramePass* pipeline) -> bool;

        auto BindPassDescriptors(FramePass* pass) -> void;
        auto CreatePassDescriptors(FramePass* pass) -> void;

        auto PushBuffer( FramePass* pass, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType) -> void;
        auto PushImage( FramePass* pass, std::string_view textureName, std::string_view samplerName, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType) -> void;

        auto CreateBindlessTexturesSet() -> void;
        auto UpdateBindlessTexturesSet(Texture* texture, Sampler* sampler, Size setIndex ) const -> void;

        auto InitSharedShaderResourceGroups() -> void;

    private:
        static constexpr UInt32 TEXTURES_DESCRIPTOR_SET_INDEX{ 0 };
        static constexpr UInt32 PER_FRAME_DESCRIPTOR_SET_INDEX{ 1 };
        static constexpr UInt32 PER_PASS_DESCRIPTOR_SET_INDEX{ 2 };

    private:
        // Information I store for each pass
        struct FramePassInfo {
            // Set index -> Descriptor Set handle
            ankerl::unordered_dense::map<UInt32, VkDescriptorSet> DescriptorSets{};

            SRGPerPass PassResources{};

            PipelineHandle Pipeline{};

            // Whether we need to update the descriptor sets or not
            bool Dirty{ true };
        };

        ankerl::unordered_dense::map<FramePass*, CommandListHandle> m_CmdLists{};

        FrameBlackboard* m_Blackboard{};

#if defined( MKT_USE_VULKAN_BINDLESS )
        DescriptorSetLayoutHandle m_LayoutTextures{};
        VkDescriptorSet m_BindlessTexturesSet{};
        VkPipelineLayout m_TexturesPipelineLayout{};
        bool m_UpdateTextureDescriptor{ false };
#endif
    private:
        ankerl::unordered_dense::map<FramePass*, FramePassInfo> m_PassInfo{};
    };
}// namespace Mikoto


#endif//MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
