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
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanGraphicsContext.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace Mikoto {

    struct VulkanResourceStateInfo {
        VkPipelineStageFlags Stages{ VK_FLAGS_NONE };
        VkAccessFlags Access{ VK_FLAGS_NONE };
        VkImageLayout Layout{ VK_IMAGE_LAYOUT_UNDEFINED }; // Buffers ignore this
    };

    MKT_NODISCARD constexpr auto GetAspectMask(VkFormat format) -> VkImageAspectFlags {
        switch (format) {
            // Color formats
            case VK_FORMAT_R8_UNORM:
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_R16G16B16A16_SFLOAT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return VK_IMAGE_ASPECT_COLOR_BIT;

                // Depth-only formats
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;

                // Depth + Stencil formats
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

                // Stencil-only formats (rare)
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;

            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    constexpr auto GetBufferDescriptorType( BufferUsage type ) noexcept -> VkDescriptorType {
        switch (type) {
            case BufferUsage::SSBO:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case BufferUsage::UNIFORM:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            default: ;
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

        case FrameResourceState::UnorderedAccess:
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
        : GraphicsContext{ device } {}

    auto VulkanGraphicsContext::Init() -> void {
        CreateBindlessTexturesSet();
    }

    auto VulkanGraphicsContext::Shutdown() -> void {
        m_LayoutTextures.Reset();
        vkDestroyPipelineLayout( VK_DEVICE(m_Device), m_TexturesPipelineLayout, nullptr );
    }

    auto VulkanGraphicsContext::BeginFrame() -> void {
    }

    auto VulkanGraphicsContext::EndFrame() -> void {

    }

    auto VulkanGraphicsContext::BindShaderResources( std::string_view passName, CommandListHandle cmdList ) -> void {
        // Here we only bind the PerPass shader resources
        VkCommandBuffer vkCmd{ cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        const auto it{ m_PassInfo.find( std::string{ passName } ) };
        if (it != m_PassInfo.end()) {

            if (!it->second.DescriptorSets.empty()) {
                VkPipelineLayout pipelineLayout{ it->second.Pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

                for (const auto &[setIndex, descriptorSet]: it->second.DescriptorSets) {
                    // Since this is a per pass descriptor set we will only bind the set at PER_PASS_DESCRIPTOR_SET_INDEX
                    if (PER_PASS_DESCRIPTOR_SET_INDEX == setIndex) {
                        switch (it->second.Pipeline->GetPipelineType()) {
                            case PipelineType::GRAPHICS_PIPELINE:
                                vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, setIndex, 1, std::addressof( descriptorSet ), 0, nullptr );
                                break;
                            case PipelineType::COMPUTE_PIPELINE:
                                vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, setIndex, 1, std::addressof( descriptorSet ), 0, nullptr );
                                break;
                            default:;
                        }
                    }
                }
            }
        }
    }

    auto VulkanGraphicsContext::CreatePassDescriptors( std::string_view passName, PipelineDescription& desc ) -> void {
        const auto it{ m_PassInfo.find( std::string{ passName } ) };

        // Pass already has descriptor sets ready
        if (it != m_PassInfo.end() && !it->second.DescriptorSets.empty()) {
            return;
        }

        FramePassInfo& passInfo{ it->second };
        passInfo.Pipeline = CreatePipeline( desc );

        VulkanPipeline* vulkanPipeline{ MKT_TO_VK_PIPELINE(passInfo.Pipeline) };

        for (const auto& setIndex: vulkanPipeline->GetDescriptorSetIndices()) {
            // We only create the descriptor sets for per pass shader resource sets for now
            if (setIndex == TEXTURES_DESCRIPTOR_SET_INDEX || setIndex == PER_FRAME_DESCRIPTOR_SET_INDEX) {
                continue;
            }

            const VkDescriptorSetLayout &layout{ vulkanPipeline->GetDescriptorSetLayout( setIndex ) };
            VkDescriptorSet descriptorSet{ TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layout ) ) };

            passInfo.DescriptorSets[setIndex] = descriptorSet;
        }
    }

    auto VulkanGraphicsContext::UpdatePassDescriptors(  std::string_view passName, SRGPerPass& passData ) -> void {
        const auto it{ m_PassInfo.find( std::string{ passName } ) };

        if (it != m_PassInfo.end() && passData.IsDirty()) {
            for (const auto& [Name, Info]: passData) {

                switch (Info.Type) {
                    case ShaderResourceType::BUFFER:
                        PushBuffer( GetBuffer( Name ), Info.Binding, it->second.DescriptorSets[PER_PASS_DESCRIPTOR_SET_INDEX] );
                        break;
                    case ShaderResourceType::COMBINED_IMAGE_SAMPLER:
                        PushImage( GetTexture( Name ), GetSampler(Info.SamplerName), Info.Binding, it->second.DescriptorSets[PER_PASS_DESCRIPTOR_SET_INDEX] );
                        break;
                    default: ;
                }
            }

            // Update once unless strictly necessary
            passData.ClearDirty();
        }
    }

    auto VulkanGraphicsContext::PushBuffer( BufferHandle handle, UInt32 groupBinding, VkDescriptorSet set ) -> void {
        DescriptorWriter writer{};
        writer.WriteBuffer( groupBinding, handle->GetNativeHandle( ObjectType::Vk_Buffer ), handle->GetSizeBytes(), 0, GetBufferDescriptorType( handle->GetUsage() ) );
        writer.UpdateSet( VK_DEVICE( m_Device ), set );
    }

    auto VulkanGraphicsContext::PushImage( TextureHandle textureHandle, SamplerHandle samplerHandle, UInt32 groupBinding, VkDescriptorSet set ) -> void {

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

        writer.UpdateSet( VK_DEVICE( m_Device ), set );
    }

    auto VulkanGraphicsContext::CreateBindlessTexturesSet() -> void {
        const UInt32 maxBindlessTextures{ SRGTextures::GetMaxTextureCount() };

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
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

        m_LayoutTextures = TO_VK_DEVICE( m_Device )->AllocateDescriptorSetLayout(  layoutInfo );
        m_LayoutTextures->SetDebugName( "DescriptorSetLayout for VulkanGraphicsContext bindless textures" );

        std::array<UInt32, 1> variableCount{ maxBindlessTextures };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
        variableCountInfo.descriptorSetCount = static_cast<UInt32>( variableCount.size() );
        variableCountInfo.pDescriptorCounts = variableCount.data();

        VkDescriptorSetLayout layoutTextures{ m_LayoutTextures->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ) };
        m_BindlessTexturesSet = TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layoutTextures ), std::addressof( variableCountInfo ) );

        // We create the pipeline layout for this descriptor set
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = std::addressof( layoutTextures ),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };

        vkCreatePipelineLayout(
            VK_DEVICE( m_Device ),
            &pipelineLayoutInfo,
            nullptr,
            std::addressof( m_TexturesPipelineLayout )
        );
    }

    auto VulkanGraphicsContext::BindGlobalTextures(CommandListHandle cmdList) -> void {
        vkCmdBindDescriptorSets(
            cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_TexturesPipelineLayout,
            TEXTURES_DESCRIPTOR_SET_INDEX,
            1,
            &m_BindlessTexturesSet,
            0,
            nullptr
        );
    }

    auto VulkanGraphicsContext::PushBuffer( BufferHandle handle, std::string_view passName, UInt32 bindingSlot ) -> void {
        // Verify the pass exists and is not already using that buffer

        const auto it{ m_PassInfo.find( std::string{ passName } ) };
        if (it == m_PassInfo.end()) {
            return;
        }

        // If the combined image sampler does not exist
        if (!it->second.Buffers.contains( handle.GetRaw() )) {
            PushBuffer( handle, bindingSlot, it->second.DescriptorSets[PER_PASS_DESCRIPTOR_SET_INDEX] );

            it->second.Buffers.emplace( handle.GetRaw() );
        }
    }

    auto VulkanGraphicsContext::PushTexture( TextureHandle handle, SamplerHandle sampler, std::string_view passName, UInt32 bindingSlot ) -> void {
        // Verify the pass exists and is not already using the texture and sampler

        const auto it{ m_PassInfo.find( std::string{ passName } ) };
        if (it == m_PassInfo.end()) {
            return;
        }

        // If the combined image sampler does not exist
        if (!it->second.CombinedImageSampler.contains( std::make_pair( handle.GetRaw(), sampler.GetRaw() ) )) {
            PushImage( handle, sampler, bindingSlot, it->second.DescriptorSets[PER_PASS_DESCRIPTOR_SET_INDEX] );

            it->second.CombinedImageSampler.emplace( std::make_pair( handle.GetRaw(), sampler.GetRaw() ) );
        }
    }

    auto VulkanGraphicsContext::PushConstants( std::string_view name, const SRGConstants &constants, CommandListHandle cmd ) -> void {
        const auto it{ m_PassInfo.find( StringUtil::From( name ) ) };
        if (it == m_PassInfo.end()) {
            return;
        }

        PipelineHandle pipeline{ it->second.Pipeline };

        VulkanPipeline* vulkanPipeline{ MKT_TO_VK_PIPELINE( pipeline ) };

        VkShaderStageFlags pcShaderStages{ vulkanPipeline->GetPushConstantRangeShaderFlags() };

        vkCmdPushConstants(
            cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
            pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ),
            pcShaderStages,
            0,
            constants.GetSize(),
            constants.GetData());
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
            .size = VK_WHOLE_SIZE
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

        auto oldInfo{ GetVulkanState(previousState) };
        auto newInfo{ GetVulkanState(newState) };

        VkImageMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = oldInfo.Stages,
            .srcAccessMask = oldInfo.Access,
            .dstStageMask = newInfo.Stages,
            .dstAccessMask = newInfo.Access,
            .oldLayout = oldInfo.Layout,
            .newLayout = newInfo.Layout,
            .image = texture->GetNativeHandle( ObjectType::Vk_Image ),
            .subresourceRange = {
                .aspectMask = GetAspectMask(VulkanHelpers::ToVkFormat( texture->GetFormat() )),
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS
            }
        };

        // TODO: update layout of underlying VulkanTexture
        if (const auto vulkanTexture{ dynamic_cast<VulkanTexture*>(texture.GetRaw()) }) {
            vulkanTexture->SetCurrentLayout(newInfo.Layout);
        }

        if (const auto vulkanTexture{ dynamic_cast<VulkanTextureCube*>(texture.GetRaw()) }) {
            vulkanTexture->SetCurrentLayout(newInfo.Layout);
        }

        VkDependencyInfo depInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = std::addressof( barrier )
        };

        vkCmdPipelineBarrier2(cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ), std::addressof( depInfo ) );

        return true;
    }

    auto VulkanGraphicsContext::UpdateBindlessTexturesSet(Texture* texture, Sampler* sampler, Size setIndex ) const -> void {
        MKT_ASSERT(setIndex < SRGTextures::GetMaxTextureCount(), "Set index must be smaller than max bindless textures");

        VkSampler vkSampler{ sampler->GetNativeHandle( ObjectType::Vk_Sampler ) };
        VkImageView vkImageView{ texture->GetNativeHandle( ObjectType::Vk_ImageView ) };

        DescriptorWriter writer{};
        writer.WriteImage( 0, vkImageView, vkSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setIndex )
            .UpdateSet( VK_DEVICE( m_Device ), m_BindlessTexturesSet );
    }

    auto VulkanGraphicsContext::PushGlobalTexture( TextureHandle texture ) -> Int32 {
        if (texture.IsEmpty()) {
            return SRGTextures::INVALID_TEXTURE_INDEX;
        }

        SamplerHandle sampler{ m_Device->GetDummySampler() };
        MKT_ASSERT( !sampler.IsEmpty(), "Dummy sampler cannot be empty" );

        // If combined sampler has already been registered return its index
        Int32 index{ m_SrgTextures.GetIndex(texture, sampler) };
        if (index != SRGTextures::INVALID_TEXTURE_INDEX) {
            return index;
        }

        // If combined sampler does not exist, register it
        Int32 result{ m_SrgTextures.Bind( texture, sampler ) };
        if (result != SRGTextures::INVALID_TEXTURE_INDEX) {
            UpdateBindlessTexturesSet( texture.GetRaw(), sampler.GetRaw(), result );
        }

        return result;

    }

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

    auto VulkanGraphicsContext::PrepareResourceBindings( std::string_view passName, PipelineDescription& desc ) -> void {
        m_PassInfo.try_emplace( std::string{ passName }, FramePassInfo{} );
        CreatePassDescriptors( passName, desc );
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

    auto VulkanGraphicsContext::UpdateResourceBindings( std::string_view passName, SRGPerPass& passData ) -> void {
        UpdatePassDescriptors( passName, passData );
    }

}// namespace Mikoto