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

#include <Core/Profiler.hh>
#include <Common/String.hh>

#include <Material/ShaderLibrary.hh>

#include <Renderer/Core/RenderService.hh>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanGraphicsContext.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace Mikoto {

    struct VulkanResourceStateInfo {
        VkPipelineStageFlags Stages{ VK_FLAGS_NONE };
        VkAccessFlags Access{ VK_FLAGS_NONE };
        VkImageLayout Layout{ VK_IMAGE_LAYOUT_UNDEFINED }; // Buffers ignore this
    };

    static auto GetBindPoint(PipelineType pipeline ) -> VkPipelineBindPoint {
        switch (pipeline) {
            case PipelineType::GRAPHICS_PIPELINE:
                return VK_PIPELINE_BIND_POINT_GRAPHICS;
            case PipelineType::COMPUTE_PIPELINE:
                return VK_PIPELINE_BIND_POINT_COMPUTE;
            default:
                MKT_ASSERT( false, "Unsupported type of pipeline bind point" );
        }

        return VK_PIPELINE_BIND_POINT_MAX_ENUM;
    }

    static auto GetSetIndex(ResourceGroup group ) -> Size {
        switch (group) {

            case ResourceGroup::Constants:
                return CONSTANTS_SET_INDEX;
            case ResourceGroup::ImageViews:
                return IMAGE_VIEWS_SET_INDEX;;
            case ResourceGroup::BufferViews:
                return BUFFER_VIEWS_SET_INDEX;;
            case ResourceGroup::UnorderedAccessViews:
                return UAV_SET_INDEX;;
            case ResourceGroup::StaticSamplers:
                return STATIC_SAMPLERS_SET_INDEX;;
            case ResourceGroup::DynamicSamplers:
                return DYNAMIC_SAMPLERS_SET_INDEX;;
            case ResourceGroup::UnboundedBufferViews:
                return UNBOUNDED_BV_SAMPLERS_SET_INDEX;;
            case ResourceGroup::UnboundedImageViews:
                return UNBOUNDED_IV_SAMPLERS_SET_INDEX;;
        }

        // Should not happen
        return CONSTANTS_SET_INDEX;
    }

    constexpr auto GetBufferDescriptorType( BufferUsage type, ResourceUsageType usage ) noexcept -> VkDescriptorType {
        if (usage == ResourceUsageType::RESOURCE_USAGE_STREAMING) {
            switch (type) {
                case BufferUsage::SHADER_STORAGE:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

                case BufferUsage::UNIFORM:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                default: ;
            }
        } else {
            switch (type) {
                case BufferUsage::SHADER_STORAGE:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

                case BufferUsage::UNIFORM:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                default: ;
            }
        }

        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    MKT_NODISCARD constexpr auto GetVulkanState(FrameResourceState state) -> VulkanResourceStateInfo {
    switch (state) {

        case FrameResourceState::ShaderRead_GraphicsPipeline:
            return {
                .Stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .Access = VK_ACCESS_SHADER_READ_BIT,
                .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

        case FrameResourceState::ShaderRead_ComputePipeline:
            return {
                .Stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .Access = VK_ACCESS_SHADER_READ_BIT,
                .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

        case FrameResourceState::UniformBuffer:
            return {
                .Stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .Access = VK_ACCESS_UNIFORM_READ_BIT,
                .Layout = VK_IMAGE_LAYOUT_UNDEFINED // N/A for buffers
            };

        case FrameResourceState::VertexIndexBuffer:
            return {
                .Stages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                .Access = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT,
                .Layout = VK_IMAGE_LAYOUT_UNDEFINED
            };

        case FrameResourceState::UnorderedAccessView:
            return {
                .Stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .Access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .Layout = VK_IMAGE_LAYOUT_GENERAL
            };

        case FrameResourceState::DepthWrite:
            return {
                .Stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .Access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            };

        case FrameResourceState::DepthRead:
            return {
                .Stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .Access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            };

        case FrameResourceState::TransferSrc:
            return {
                .Stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .Access = VK_ACCESS_TRANSFER_READ_BIT,
                .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            };

        case FrameResourceState::TransferDst:
            return {
                .Stages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .Access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            };

        case FrameResourceState::Present:
            return {
                .Stages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .Access = 0,
                .Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            };

        case FrameResourceState::RenderTarget:
            return {
                .Stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .Access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            };

        case FrameResourceState::Undefined:
        default:
            return {
                .Stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .Access = 0,
                .Layout = VK_IMAGE_LAYOUT_UNDEFINED
            };
    }
}

    VulkanGraphicsContext::VulkanGraphicsContext( GpuDevice *device )
        : GraphicsContext{}, m_Device{ TO_VK_DEVICE( device ) } {}

    auto VulkanGraphicsContext::Init() -> void {
        CreateBindlessTexturesSet();

        m_MaxFramesInFlight = MKT_VK_CTX( RenderService::Get()->GetContext() )->GetMaxFramesInFlight();
    }

    auto VulkanGraphicsContext::Shutdown() -> void {
        m_PassInfo.clear();
        m_TexturesByNames.clear();
        m_BuffersByNames.clear();
        m_PipelinesByNames.clear();
        m_SamplersByNames.clear();
        m_Samplers.clear();

        m_LayoutTextures.Reset();
        vkDestroyPipelineLayout( m_Device->GetLogicalDevice(), m_TexturesPipelineLayout, nullptr );
    }

    auto VulkanGraphicsContext::BeginFrame() -> void {
        m_CurrentFrameIndex = MKT_VK_CTX( RenderService::Get()->GetContext() )->GetCurrentFrameIndex();
    }

    auto VulkanGraphicsContext::EndFrame() -> void {

    }

    auto VulkanGraphicsContext::BindShaderResources( std::string_view passName, CommandListHandle cmdList ) -> void {
        if (!m_PassInfo.contains( std::string{ passName } )) {
            return;
        }

        FramePassInfo& passInfo{ m_PassInfo.find( std::string{ passName } )->second };
        VkCommandBuffer vkCmd{ cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        VkPipelineLayout pipelineLayout{ passInfo.Pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

        if (!passInfo.DescriptorSets.empty()) {
            for (const auto &[setIndex, descriptorSet]: passInfo.DescriptorSets) {
                if (descriptorSet == VK_NULL_HANDLE) {
                    continue;
                }

                const VkPipelineBindPoint bindPoint{ GetBindPoint( passInfo.Pipeline->GetPipelineType() ) };

                if (setIndex == BUFFER_VIEWS_SET_INDEX) {
                    for ( const auto& [slot, buffer] : passInfo.BuffersBindings) {
                        UInt32 dynamicOffset{ m_CurrentFrameIndex * MKT_VK_BUFFER_PTR( buffer )->GetAlignedSize() };
                        passInfo.DynamicOffsets[slot] = dynamicOffset;
                    }
                    // Buffer views buffers use ring buffer, as many slices as frames in flight
                    vkCmdBindDescriptorSets( vkCmd, bindPoint, pipelineLayout, setIndex, 1, std::addressof( descriptorSet ),
                        static_cast<UInt32>(passInfo.DynamicOffsets.size()), passInfo.DynamicOffsets.data() );
                } else {
                    std::array sets{ descriptorSet };
                    vkCmdBindDescriptorSets( vkCmd, bindPoint, pipelineLayout, setIndex,
                        static_cast<UInt32>(sets.size()), sets.data(), 0, nullptr );
                }
            }
        }
    }

    auto VulkanGraphicsContext::CreatePassDescriptors( std::string_view pass, PipelineDescription& desc ) -> void {
        const auto it{ m_PassInfo.find( std::string{ pass } ) };

        // Pass already has descriptor sets ready
        if (it != m_PassInfo.end() && !it->second.DescriptorSets.empty()) {
            return;
        }

        FramePassInfo& passInfo{ it->second };
        passInfo.Pipeline = CreatePipeline( desc );

        VulkanPipeline* vulkanPipeline{ MKT_TO_VK_PIPELINE(passInfo.Pipeline) };

        for (const auto& setIndex: vulkanPipeline->GetDescriptorSetIndices()) {
            const VkDescriptorSetLayout &layout{ vulkanPipeline->GetDescriptorSetLayout( setIndex ) };
            VkDescriptorSet descriptorSet{ m_Device->AllocateDescriptorSet( std::addressof( layout ) ) };

            if ( descriptorSet != VK_NULL_HANDLE ) {
                passInfo.DescriptorSets[setIndex] = descriptorSet;
            }
        }

        // If it uses dynamic uniform buffers or dynamic storage buffers preallocate the vector
        passInfo.DynamicOffsets.resize( vulkanPipeline->GetDynamicBuffersSetBindingCount() );
    }

    auto VulkanGraphicsContext::PushBuffer( BufferHandle handle, UInt32 groupBinding, VkDescriptorSet& sets ) -> void {
        if (sets == VK_NULL_HANDLE) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushBuffer - Null descriptor set. Buffer [{}]. Slot [0]", handle->GetDebugName(), groupBinding );
            return;
        }

        DescriptorWriter writer{};

        Size range{handle->GetSizeBytes() };
        if (handle->IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_STREAMING )) {
            range = MKT_VK_BUFFER( handle )->GetAlignedSize();
        }

        writer.WriteBuffer( groupBinding, handle->GetNativeHandle(ObjectType::Vk_Buffer),
            range, 0, GetBufferDescriptorType( handle->GetUsage(), handle->GetResourceUsage() ) )
                .UpdateSet( m_Device->GetLogicalDevice(), sets );
    }

    auto VulkanGraphicsContext::PushImage( TextureHandle textureHandle, SamplerHandle samplerHandle, UInt32 groupBinding, VkDescriptorSet& sets ) -> void {
        if (sets == VK_NULL_HANDLE) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushImage - Null descriptor set. Image [{}]. Slot [0]", textureHandle->GetDebugName(), groupBinding );
            return;
        }

        VkSampler sampler{ VK_NULL_HANDLE };

        if (!samplerHandle.IsEmpty()) {
            sampler = samplerHandle->GetNativeHandle(ObjectType::Vk_Sampler);
        }

        DescriptorWriter writer{};

        VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };
        if (textureHandle->IsTextureType(TextureType::TEXTURE_CUBE)) {
            layout = dynamic_cast<VulkanTextureCube *>( textureHandle.GetRaw() )->GetCurrentLayout();
        } else {
            layout = dynamic_cast<VulkanTexture *>( textureHandle.GetRaw() )->GetCurrentLayout();
        }
        writer.WriteImage( groupBinding, textureHandle->GetNativeHandle( ObjectType::Vk_ImageView ), sampler, layout, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER );

        writer.UpdateSet( m_Device->GetLogicalDevice(), sets );
    }

    auto VulkanGraphicsContext::CreateBindlessTexturesSet() -> void {
        // TODO: Review below, could not figure out yet a way to use push constants with GBuffer which used push constants AND bindless textures array
        // NOTES: https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-compatibility
        // https://www.reddit.com/r/vulkan/comments/1l53wth/does_vkcmdbinddescriptorsets_invalidate_sets_with/

        // There was an Issue with the GBuffer pass which used a push constant but this pipeline layout declares none
        const UInt32 maxBindlessTextures{ GlobalTextures::GetMaxTextureCount() };

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = TEXTURES_DESCRIPTOR_SET_INDEX;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = maxBindlessTextures;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = nullptr;

        VkDescriptorBindingFlags bindingFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = 1,
            .pBindingFlags = &bindingFlags
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &flagsInfo,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = 1,
            .pBindings = &binding
        };

        m_LayoutTextures = m_Device->AllocateDescriptorSetLayout(  layoutInfo );
        m_LayoutTextures->SetDebugName( "DescriptorSetLayout for VulkanGraphicsContext bindless textures" );

        std::array variableCount{ maxBindlessTextures };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = static_cast<UInt32>( variableCount.size() );
        variableCountInfo.pDescriptorCounts = variableCount.data();

        VkDescriptorSetLayout layoutTextures{ m_LayoutTextures->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ) };
        m_BindlessTexturesSet = m_Device->AllocateDescriptorSet( std::addressof( layoutTextures ), std::addressof( variableCountInfo ) );

        // For Passes that use push constants
        // This PS is declared the same way as in the pipeline reflection
        // This allows for descriptor sets compatibility as otherwise it would lead to this
        // descriptor set ot get unbound if we bind a non-compatible set after it,
        // We declare by default in the pipeline layouts here and in the reflected pipeline
        // the global push_constants even if they are later not used by the pass
        VkPushConstantRange psRange{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset     = 0,
            .size       = MINIMUM_REQUIRED_PUSH_CONSTANTS_SIZE
        };

        std::array pushConstants{ psRange };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = std::addressof( layoutTextures ),
            .pushConstantRangeCount = static_cast<UInt32>( pushConstants.size() ),
            .pPushConstantRanges = pushConstants.data()
        };

        vkCreatePipelineLayout(
            m_Device->GetLogicalDevice(),
            std::addressof( pipelineLayoutInfo ),
            nullptr,
            std::addressof( m_TexturesPipelineLayout )
        );
    }

    auto VulkanGraphicsContext::PushBuffer( BufferHandle handle, std::string_view passName, UInt32 bindingSlot ) -> void {
        // Verify the pass exists and is not already using that buffer

        const auto it{ m_PassInfo.find( std::string{ passName } ) };
        if (it == m_PassInfo.end()) {
            return;
        }

        // If the combined buffer does not exist
        auto setIndex{ handle->IsResourceUsage( ResourceUsageType::RESOURCE_USAGE_DYNAMIC ) ? 
            DYNAMIC_BUFFERS_SET_INDEX : DYNAMIC_RESOURCE_SET_INDEX };
        if (!it->second.Buffers.contains( handle.GetRaw() )) {

            PushBuffer( handle, bindingSlot, it->second.DescriptorSets[setIndex] );

            it->second.Buffers.emplace( handle.GetRaw() );
            it->second.BuffersBindings[bindingSlot] = handle.GetRaw();
        } else {
            // Update the binding if it is a new buffer
            if (it->second.BuffersBindings[bindingSlot] != handle.GetRaw()) {
                PushBuffer( handle, bindingSlot, it->second.DescriptorSets[setIndex] );
                it->second.BuffersBindings[bindingSlot] = handle.GetRaw();
            }
        }
    }

    auto VulkanGraphicsContext::PushTexture( TextureHandle handle, SamplerHandle sampler, std::string_view passName, UInt32 bindingSlot ) -> void {
        const auto it{ m_PassInfo.find( std::string{ passName } ) };
        if (it == m_PassInfo.end()) {
            return;
        }

        // If the combined image sampler does not exist
        const auto itCombinedImageSampler{ it->second.CombinedImageSampler.find( std::make_pair( handle.GetRaw(), sampler.GetRaw() ) ) };

        // Individual textures go to the static resources set
        if (itCombinedImageSampler == it->second.CombinedImageSampler.end()) {
            PushImage( handle, sampler, bindingSlot, it->second.DescriptorSets[DYNAMIC_RESOURCE_SET_INDEX] );
            it->second.CombinedImageSampler.emplace( std::make_pair( handle.GetRaw(), sampler.GetRaw() ) );

            it->second.CombinedImageSamplerBinding[bindingSlot] = std::make_pair( handle.GetRaw(), sampler.GetRaw() );
        } else {
            // Update if there is a different pair of image and sampler
            if (it->second.CombinedImageSamplerBinding[bindingSlot] != std::make_pair( handle.GetRaw(), sampler.GetRaw() )) {
                PushImage( handle, sampler, bindingSlot, it->second.DescriptorSets[DYNAMIC_RESOURCE_SET_INDEX] );
                it->second.CombinedImageSamplerBinding[bindingSlot] = std::make_pair( handle.GetRaw(), sampler.GetRaw() );
            }
        }
    }

    auto VulkanGraphicsContext::PushConstants( std::string_view passName, const ConstantsGroup &constants, CommandListHandle cmd ) -> void {
        const auto it{ m_PassInfo.find( StringUtil::From( passName ) ) };
        if (it == m_PassInfo.end()) {
            return;
        }

        PipelineHandle pipeline{ it->second.Pipeline };

        // Push constants are declared as globals visible across all stages
        VkShaderStageFlags pcShaderStages{ VK_SHADER_STAGE_ALL };

        std::array<Byte, MINIMUM_REQUIRED_PUSH_CONSTANTS_SIZE> byteCode{};
        std::memcpy( byteCode.data(), constants.GetData(), constants.GetSize() );

        vkCmdPushConstants(
            cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
            pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ),
            pcShaderStages,
            0,
            static_cast<UInt32>(byteCode.size()),
            byteCode.data());
    }

    auto VulkanGraphicsContext::InsertResourceBarrierBatch( std::span<ResourceBarrierInfo> barriers, CommandListHandle cmd ) -> void {

    }

    auto VulkanGraphicsContext::InsertResourceBarrier( BufferHandle buffer, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd ) -> bool {
        if (previousState == newState) {
            return false;
        }

        auto oldInfo{ GetVulkanState(previousState) };
        auto newInfo{ GetVulkanState(newState) };

        VkBufferMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = oldInfo.Stages,
            .srcAccessMask = oldInfo.Access,
            .dstStageMask = newInfo.Stages,
            .dstAccessMask = newInfo.Access,
            .buffer = buffer->GetNativeHandle( ObjectType::Vk_Buffer ),
            .offset = 0,
            .size = VK_WHOLE_SIZE // for dynamic buffers maybe its better to protect the current frame region?
        };

        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &barrier
        };

        vkCmdPipelineBarrier2(cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ), &depInfo);

        return true;
    }

    auto VulkanGraphicsContext::InsertResourceBarrier( TextureHandle texture, FrameResourceState previousState, FrameResourceState newState, CommandListHandle cmd ) -> bool {
        if (previousState == newState) {
            return false;
        }

        const auto newInfo{ GetVulkanState(newState) };

        if (texture->IsTextureType( TextureType::TEXTURE_2D )) {
            dynamic_cast<VulkanTexture*>(texture.GetRaw())->SubmitLayoutTransition( newInfo.Layout, cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) );
        }

        if (texture->IsTextureType( TextureType::TEXTURE_CUBE )) {
            dynamic_cast<VulkanTextureCube*>(texture.GetRaw())->SubmitLayoutTransition( newInfo.Layout, cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) );
        }

        return true;
    }

    auto VulkanGraphicsContext::PushBuffer( ResourceGroup group, BufferHandle buffer, std::string_view pass, ResourceSlot slot ) -> void {
        // Verify the pass exists and is not already using that buffer

        const auto it{ m_PassInfo.find( std::string{ pass } ) };
        if (it == m_PassInfo.end()) {
            return;
        }

        FramePassInfo& info{ it->second };

        auto bindingSlot{ (UInt32)slot };
        auto setIndex{ GetSetIndex(group) };

        // If the combined buffer does not exist
        if (!info.Buffers.contains( buffer.GetRaw() )) {
            PushBuffer( buffer, bindingSlot, it->second.DescriptorSets[setIndex] );

            it->second.Buffers.emplace( buffer.GetRaw() );
            it->second.BuffersBindings[bindingSlot] = buffer.GetRaw();
        } else {
            // Update the binding if it is a new buffer
            if (it->second.BuffersBindings[bindingSlot] != buffer.GetRaw()) {
                PushBuffer( buffer, bindingSlot, it->second.DescriptorSets[setIndex] );
                it->second.BuffersBindings[bindingSlot] = buffer.GetRaw();
            }
        }
    }

    auto VulkanGraphicsContext::PushTexture( ResourceGroup group, TextureHandle texture, std::string_view pass, ResourceSlot slot ) -> void {

    }

    auto VulkanGraphicsContext::PushTexture( ResourceGroup group, TextureHandle texture, SamplerHandle sampler, std::string_view pass, ResourceSlot slot ) -> void {
        const auto it{ m_PassInfo.find( std::string{ pass } ) };
        if (it == m_PassInfo.end()) {
            return;
        }


    }

    auto VulkanGraphicsContext::UpdateBindlessTexturesSet(Texture* texture, Sampler* sampler, Size setIndex ) const -> void {
        MKT_ASSERT(setIndex < GlobalTextures::GetMaxTextureCount(), "Set index must be smaller than max bindless textures");

        VkSampler vkSampler{ sampler->GetNativeHandle( ObjectType::Vk_Sampler ) };
        VkImageView vkImageView{ texture->GetNativeHandle( ObjectType::Vk_ImageView ) };

        DescriptorWriter writer{};
        writer.WriteImage( 0, vkImageView, vkSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setIndex )
            .UpdateSet( m_Device->GetLogicalDevice(), m_BindlessTexturesSet );
    }

    // auto VulkanGraphicsContext::PushGlobalTexture( TextureHandle texture ) -> Int32 {
    //     if (texture.IsEmpty()) {
    //         return GlobalTextures::INVALID_TEXTURE_INDEX;
    //     }
    //
    //     SamplerHandle sampler{ m_Device->GetDummySampler() };
    //     MKT_ASSERT( !sampler.IsEmpty(), "Dummy sampler cannot be empty" );
    //
    //     // If combined sampler has already been registered return its index
    //     Int32 index{ m_SrgTextures.GetIndex(texture, sampler) };
    //     if (index != GlobalTextures::INVALID_TEXTURE_INDEX) {
    //         return index;
    //     }
    //
    //     // If combined sampler does not exist, register it
    //     Int32 result{ m_SrgTextures.Bind( texture, sampler ) };
    //     if (result != GlobalTextures::INVALID_TEXTURE_INDEX) {
    //         UpdateBindlessTexturesSet( texture.GetRaw(), sampler.GetRaw(), result );
    //     }
    //
    //     return result;
    //
    // }

    auto VulkanGraphicsContext::GetTexture( std::string_view name ) -> TextureHandle {
        const auto it{ m_TexturesByNames.find( std::string{ name } ) };
        if (it != m_TexturesByNames.end()) {
            return it->second;
        }

        return TextureHandle::CreateEmpty();
    }

    auto VulkanGraphicsContext::GetPipeline( std::string_view name ) -> PipelineHandle {
        const auto it{ m_PipelinesByNames.find( std::string{ name } ) };
        if (it != m_PipelinesByNames.end()) {
            return it->second;
        }

        return PipelineHandle::CreateEmpty();
    }

    auto VulkanGraphicsContext::GetBuffer( std::string_view name ) -> BufferHandle {
        const auto it{ m_BuffersByNames.find( std::string{ name } ) };
        if (it != m_BuffersByNames.end()) {
            return it->second;
        }

        return BufferHandle::CreateEmpty();
    }

    auto VulkanGraphicsContext::GetSampler( std::string_view name ) -> SamplerHandle {
        const auto it{ m_SamplersByNames.find( std::string{ name } ) };
        if (it != m_SamplersByNames.end()) {
            return it->second;
        }

        return SamplerHandle::CreateEmpty();
    }

    auto VulkanGraphicsContext::PrepareResourceBindings( std::string_view pass, PipelineDescription& desc ) -> void {
        m_PassInfo.try_emplace( std::string{ pass }, FramePassInfo{} );
        CreatePassDescriptors( pass, desc );
    }

    auto VulkanGraphicsContext::CreateTexture( std::string_view name, const TextureDescription &description ) -> TextureHandle {
        if (m_TexturesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameGraph::CreateTexture - Named texture [{}] already exists.", name );
            return m_TexturesByNames[std::string{ name }];;
        }

        TextureHandle texture{ m_Device->CreateTexture( description ) };

        if (!texture.IsEmpty()) {
            texture->SetDebugName( StringUtil::Format( "VulkanTexture2D. Named: '{}'", name ) );
            m_TexturesByNames.emplace( std::string{ name }, texture );
        }

        return texture;
    }

    auto VulkanGraphicsContext::CreateTexture( std::string_view name, const TextureCubeCreateDescription &description ) -> TextureHandle {
        if (m_TexturesByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameGraph::CreateTexture - Named texture cube [{}] already exists.", name );
            return m_TexturesByNames[std::string{ name }];
        }

        TextureHandle texture{ m_Device->CreateTexture( description ) };

        if (!texture.IsEmpty()) {
            texture->SetDebugName( StringUtil::Format( "VulkanTextureCube. Named: '{}'", name ) );
            m_TexturesByNames.emplace( std::string{ name }, texture );
        }

        return texture;
    }

    auto VulkanGraphicsContext::CreatePipeline( PipelineDescription& description ) -> PipelineHandle {
        if (m_PipelinesByNames.contains( description.Name )) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::CreatePipeline - Named pipeline [{}] already exists.", description.Name );
            return m_PipelinesByNames[description.Name ];
        }

        PipelineHandle pipeline{ PipelineHandle::CreateEmpty() };

        if (std::holds_alternative<GraphicsPipelineDescription>( description.Description )) {
            GraphicsPipelineDescription &desc{ std::get<GraphicsPipelineDescription>( description.Description ) };

            for (auto &[stage, shaderPath]: description.Shaders) {
                desc.ShaderStages.emplace_back( ShaderLibrary::Get()->LoadShader( shaderPath, stage ) );
            }

            pipeline = m_Device->CreatePipeline( desc );
        } else if (std::holds_alternative<ComputePipelineDescription>( description.Description )) {
            ComputePipelineDescription &desc{ std::get<ComputePipelineDescription>( description.Description ) };

            for (auto &[stage, shaderPath]: description.Shaders) {
                desc.Stage = ShaderLibrary::Get()->LoadShader( shaderPath, stage );
            }

            pipeline = m_Device->CreatePipeline( desc );
        }

        if (!pipeline.IsEmpty()) {
            pipeline->SetDebugName( description.Name );
            m_PipelinesByNames.emplace( std::string{ description.Name }, pipeline );
        }

        return pipeline;
    }

    auto VulkanGraphicsContext::CreateBuffer( std::string_view name, BufferDescription description ) -> BufferHandle {
        if (m_BuffersByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameGraph::CreateBuffer - Named buffer [{}] already exists.", name );
            return m_BuffersByNames[std::string{ name }];
        }

        BufferHandle buffer{ m_Device->CreateBuffer( description ) };

        if (!buffer.IsEmpty()) {
            buffer->SetDebugName( name );
            m_BuffersByNames.emplace( std::string{ name }, buffer );
        }

        return buffer;
    }

    auto VulkanGraphicsContext::CreateSampler( SamplerDescription &description ) -> SamplerHandle {
        SamplerHandle samplerHandle{ m_Device->CreateSampler( description ) };

        if (!samplerHandle.IsEmpty()) {
            m_Samplers.push_back( samplerHandle );
        }

        return samplerHandle;
    }

    auto VulkanGraphicsContext::CreateSampler( std::string_view name, const SamplerDescription& description ) -> void {
        if (m_SamplersByNames.contains( std::string{ name } )) {
            MKT_CORE_LOGGER_WARN( "FrameGraph::CreateSample - Named sampler [{}] already exists.", name );
            return;
        }

        SamplerHandle buffer{ m_Device->CreateSampler( description ) };

        if (!buffer.IsEmpty()) {
            buffer->SetDebugName( name );
            m_SamplersByNames.emplace( std::string{ name }, buffer );
        }
    }
 }