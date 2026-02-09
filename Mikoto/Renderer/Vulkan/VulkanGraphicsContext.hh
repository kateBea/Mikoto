//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
#define MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH

#include <utility>

#include <volk.h>
#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>

#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>

namespace Mikoto {

    class VulkanGraphicsContext final : public GraphicsContext {
    public:
        explicit  VulkanGraphicsContext(GpuDevice* device);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame()-> void override;
        auto EndFrame()-> void  override;

        auto GetSampler(std::string_view name) -> SamplerHandle override;
        auto GetTexture(std::string_view name) -> TextureHandle override;
        auto GetPipeline(std::string_view name) -> PipelineHandle override;
        auto GetBuffer(std::string_view name) -> BufferHandle override;

        auto CreateTexture(std::string_view name, const TextureDescription &description) -> TextureHandle override;
        auto CreateTexture(std::string_view name, const TextureCubeCreateDescription& description) -> TextureHandle override;

        auto CreateBuffer(std::string_view name, BufferDescription description) -> BufferHandle override;

        auto CreateSampler( SamplerDescription& description ) -> SamplerHandle  override;
        auto CreateSampler( std::string_view name, const SamplerDescription& description ) -> void override;

        auto UpdateResourceBindings( std::string_view passName, SRGPerPass& passData ) -> void override;
        auto PrepareResourceBindings( std::string_view passName, PipelineDescription& desc ) -> void override;
        auto BindShaderResources( std::string_view passName, CommandListHandle cmdList ) -> void override;

        auto PushGlobalTexture( TextureHandle texture ) -> Int32  override;
        auto BindGlobalTextures(std::string_view passName, CommandListHandle cmdList) -> void override;

        auto PushBuffer(BufferHandle handle, std::string_view passName, UInt32 bindingSlot) -> void  override;
        auto PushTexture(TextureHandle handle, SamplerHandle sampler, std::string_view passName, UInt32 bindingSlot) -> void  override;
        auto PushConstants( std::string_view name, const SRGConstants& constants, CommandListHandle cmd ) -> void override;

        auto InsertResourceBarrier(BufferHandle buffer, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd) -> bool  override;
        auto InsertResourceBarrier(TextureHandle texture, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd) -> bool  override;

        // auto BindShaderResources(FramePassNode* pass, CommandListHandle cmdList ) -> void override;
        // auto CommitShaderResources(FramePassNode* pass ) -> void override;
        // auto CreatePipelineResources(FramePassNode* pass, PipelineHandle pipeline) -> void override;

        ~VulkanGraphicsContext() override = default;

    private:
        auto UpdatePassDescriptors(std::string_view passName, SRGPerPass& passData) -> void;
        auto CreatePassDescriptors(std::string_view passName, PipelineDescription& desc) -> void;
        auto CreatePipeline( PipelineDescription& description ) -> PipelineHandle;

        auto PushBuffer( BufferHandle handle, UInt32 groupBinding, VkDescriptorSet set) -> void;
        auto PushImage( TextureHandle textureHandle, SamplerHandle samplerHandle, UInt32 groupBinding, VkDescriptorSet set) -> void;

        auto CreateBindlessTexturesSet() -> void;
        auto UpdateBindlessTexturesSet(Texture* texture, Sampler* sampler, Size setIndex ) const -> void;

    private:
        static constexpr UInt32 TEXTURES_DESCRIPTOR_SET_INDEX{ 0 };
        static constexpr UInt32 PER_FRAME_DESCRIPTOR_SET_INDEX{ 1 };
        static constexpr UInt32 PER_PASS_DESCRIPTOR_SET_INDEX{ 2 };

    private:
        // Information I store for each pass
        struct FramePassInfo {
            // Set index -> Descriptor Set handle. Allocate as many as max frames in flight
            ankerl::unordered_dense::map<UInt32, VkDescriptorSet> DescriptorSets{};
            PipelineHandle Pipeline{};

            // Buffers this pass is using
            ankerl::unordered_dense::set<Buffer*> Buffers{};
            ankerl::unordered_dense::set<std::pair<Texture*, Sampler*>> CombinedImageSampler{};

            ankerl::unordered_dense::map<UInt32, std::pair<Texture*, Sampler*>> CombinedImageSamplerBinding{};
        };

        UInt32 m_CurrentFrameIndex{};
        UInt32 m_MaxFramesInFlight{};

#if defined( MKT_USE_VULKAN_BINDLESS )
        DescriptorSetLayoutHandle m_LayoutTextures{};
        VkDescriptorSet m_BindlessTexturesSet{};
        VkPipelineLayout m_TexturesPipelineLayout{};
        VkPipelineLayout m_TexturesPipelineLayoutPushConstants{};
        bool m_UpdateTextureDescriptor{ false };
#endif
    private:
        ankerl::unordered_dense::map<std::string, FramePassInfo> m_PassInfo{};

        // Resources by names
        ankerl::unordered_dense::map<std::string, TextureHandle> m_TexturesByNames{};
        ankerl::unordered_dense::map<std::string, PipelineHandle> m_PipelinesByNames{};
        ankerl::unordered_dense::map<std::string, BufferHandle> m_BuffersByNames{};
        ankerl::unordered_dense::map<std::string, SamplerHandle> m_SamplersByNames{};

        std::vector<SamplerHandle> m_Samplers{};
    };
}// namespace Mikoto


#endif//MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
