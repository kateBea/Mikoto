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

#include <map>
#include <array>
#include <utility>

#include <volk.h>
#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>

#include <Renderer/Core/Buffer.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GraphicsContext.hh>

#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>

namespace Mikoto {

    // For resources that can be update every frame I will handle them like so:
    // A resource can be marked and either stream, dynamic, or stream
    // if a resource is static it means it will never be modified like vertex buffers, static textures, index buffers
    // if a resource is dynamic it means it can be updated every frame, like dynamic uniform buffers, or streaming textures
    // in this case what i will do is to have a cpu staging buffer i write to and then copy to the gpu buffer every frame via a copy command, this way I properly handle synchronization and avoid hazards while the actual resource the gpu reads from is device local
    // if the resource is marked as streaming it means that it can be updated every frame but I will not use a staging buffer, instead I will write directly to the mapped memory of the gpu buffer, this is useful for resources that are updated every frame but are small
    // In the vulkan buffer when a resource is marked as dynamic I will need to vcreate it with VK_BUFFER_USAGE_TRANSFER_DST_BIT to be able to copy to it,
    struct BarrierManager {
        static constexpr UInt32 MAX_BARRIERS{ 10 };

        UInt32 ImageBarrierCount{};
        UInt32 BufferBarrierCount{};

        std::array<VkImageMemoryBarrier2, MAX_BARRIERS> ImageBarriers{};
        std::array<VkBufferMemoryBarrier2, MAX_BARRIERS> BufferBarriers{};

        auto InsertBufferBarrier( BufferHandle buffer, FrameResourceState previousState, FrameResourceState newState) -> bool;
        auto InsertTextureBarrier( TextureHandle texture, FrameResourceState previousState, FrameResourceState newState) -> bool;

        auto FlushBarriers( CommandListHandle cmd ) -> void;
    };

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

        auto UpdateResourceBindings( std::string_view passName, CommonResourceGroup& passData ) -> void override;
        auto PrepareResourceBindings( std::string_view passName, PipelineDescription& desc ) -> void override;
        auto BindShaderResources( std::string_view passName, CommandListHandle cmdList ) -> void override;

        auto PushGlobalTexture( TextureHandle texture ) -> Int32  override;
        auto BindGlobalTextures(std::string_view passName, CommandListHandle cmdList) -> void override;

        auto PushBuffer(BufferHandle handle, std::string_view passName, UInt32 bindingSlot) -> void  override;
        auto PushTexture(TextureHandle handle, SamplerHandle sampler, std::string_view passName, UInt32 bindingSlot) -> void  override;
        auto PushConstants( std::string_view passName, const ConstantsGroup& constants, CommandListHandle cmd ) -> void override;

        auto InsertResourceBarrier(BufferHandle buffer, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd) -> bool  override;
        auto InsertResourceBarrier(TextureHandle texture, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd) -> bool  override;

        ~VulkanGraphicsContext() override = default;

    private:
        auto UpdatePassDescriptors(std::string_view passName, CommonResourceGroup& passData) -> void;
        auto CreatePassDescriptors(std::string_view passName, PipelineDescription& desc) -> void;
        auto CreatePipeline( PipelineDescription& description ) -> PipelineHandle;

        auto PushBuffer( BufferHandle handle, UInt32 groupBinding, VkDescriptorSet& sets) -> void;
        auto PushImage( TextureHandle textureHandle, SamplerHandle samplerHandle, UInt32 groupBinding, VkDescriptorSet& sets) -> void;

        auto CreateBindlessTexturesSet() -> void;
        auto UpdateBindlessTexturesSet(Texture* texture, Sampler* sampler, Size setIndex ) const -> void;

    private:
        struct FramePassInfo {
            PipelineHandle Pipeline{};

            // Set index -> Descriptor Set handle. Allocate as many as max frames in flight
            ankerl::unordered_dense::map<UInt32, VkDescriptorSet> DescriptorSets{};

            // Buffers this pass is using
            ankerl::unordered_dense::set<Buffer*> Buffers{};
            ankerl::unordered_dense::set<std::pair<Texture*, Sampler*>> CombinedImageSampler{};

            // TODO: find better approach
            // It is ordered by set index because of dynamic offsets order requirements, see bind descriptor
            std::map<UInt32, Buffer*> BuffersBindings{};
            std::vector<UInt32> DynamicOffsets{};

            ankerl::unordered_dense::map<UInt32, std::pair<Texture*, Sampler*>> CombinedImageSamplerBinding{};

            BarrierManager BarrierManager{};
        };

    private:
        VulkanDevice* m_Device{ nullptr };

        UInt32 m_CurrentFrameIndex{};
        UInt32 m_MaxFramesInFlight{};

#if defined( MKT_USE_VULKAN_BINDLESS )
        DescriptorSetLayoutHandle m_LayoutTextures{};
        VkDescriptorSet m_BindlessTexturesSet{};

        VkPipelineLayout m_TexturesPipelineLayout{};
#endif
    private:
        ankerl::unordered_dense::map<std::string, FramePassInfo> m_PassInfo{};

        // Resources by names
        ankerl::unordered_dense::map<std::string, TextureHandle> m_TexturesByNames{};
        ankerl::unordered_dense::map<std::string, PipelineHandle> m_PipelinesByNames{};
        ankerl::unordered_dense::map<std::string, BufferHandle> m_BuffersByNames{};
        ankerl::unordered_dense::map<std::string, SamplerHandle> m_SamplersByNames{};

        std::vector<SamplerHandle> m_Samplers{};

        // Global list of sampled textures
        GlobalTextures m_SrgTextures{};

        // Cached dynamic buffers. These will be staging buffers that we copy data to and upload to GPU
        // Staging -> Actual Device only GPU
        ankerl::unordered_dense::map<Buffer*, BufferHandle> m_DeviceLocalBuffers{};
    };
}// namespace Mikoto


#endif//MIKOTO_VULKAN_GRAPHIC_CONTEXT_HH
