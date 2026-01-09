//
// Created by kate on 11/25/25.
//

#include "Renderer/Vulkan/VulkanGraphicsContext.hh"

#include "Core/Profiler.hh"
#include "Renderer/Vulkan/VulkanDevice.hh"
#include "Renderer/Vulkan/VulkanPipeline.hh"
#include "Renderer/Vulkan/VulkanTexture.hh"

namespace Mikoto {

    constexpr auto ToVkDescriptorType( ShaderResourceType type ) noexcept -> VkDescriptorType {
        switch (type) {
            case ShaderResourceType::SHADER_STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

            case ShaderResourceType::SHADER_RESOURCE_UNIFORM_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            case ShaderResourceType::SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            case ShaderResourceType::SHADER_RESOURCE_UNDEFINED:
            default:
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    VulkanGraphicsContext::VulkanGraphicsContext( GpuDevice *device )
        : GraphicsContext{ device } {}

    auto VulkanGraphicsContext::Init() -> void {
        CreateBindlessTexturesSet();

        InitSharedShaderResourceGroups();
    }

    auto VulkanGraphicsContext::Shutdown() -> void {
        m_LayoutTextures.Reset();
        vkDestroyPipelineLayout( VK_DEVICE(m_Device), m_TexturesPipelineLayout, nullptr );
    }

    auto VulkanGraphicsContext::BeginRender( GfxRenderInfo &beginInfo ) -> void {
        VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        std::vector<VkRenderingAttachmentInfo> colorImages{};

        for (auto &colorImage: beginInfo.ColorRenderTargets) {
            VkAttachmentLoadOp loadOp{ beginInfo.LoadOp == LoadOp::CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };
            VkRenderingAttachmentInfo &colorAttachment{ colorImages.emplace_back( VkRenderingAttachmentInfo{} ) };
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = colorImage->GetNativeHandle( ObjectType::Vk_ImageView );
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = loadOp;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = { beginInfo.ClearColor.r, beginInfo.ClearColor.g, beginInfo.ClearColor.b, beginInfo.ClearColor.a };
        }

        VkRenderingAttachmentInfo depthAttachment{};
        if (!beginInfo.DepthRenderTarget.IsEmpty()) {
            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = beginInfo.DepthRenderTarget->GetNativeHandle( ObjectType::Vk_ImageView );
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
        }

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { { 0, 0 }, { 1920, 1080 } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<UInt32>( colorImages.size() );
        renderingInfo.pColorAttachments = colorImages.data();
        renderingInfo.pDepthAttachment = beginInfo.DepthRenderTarget.IsEmpty() ? nullptr : std::addressof( depthAttachment );

        vkCmdBeginRendering( vkCmd, std::addressof( renderingInfo ) );
    }

    auto VulkanGraphicsContext::EndRender( GfxRenderInfo &info ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const auto vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        vkCmdEndRendering( vkCmd );

        // Transition color target to shader read
        for (auto &texture: info.ColorRenderTargets) {
            const auto tex{ dynamic_cast<VulkanTexture *>( texture.GetRaw() ) };
            tex->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkCmd );
        }
    }

    auto VulkanGraphicsContext::BeginCompute() -> void {}
    auto VulkanGraphicsContext::EndCompute() -> void {}

    auto VulkanGraphicsContext::BeginFrame( FrameBlackboard *blackboard ) -> void {
        // Queue type according to command types, we could switch later depending on the type of pass
        m_CmdList = m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE );
        m_CmdList->Begin();

        m_Blackboard = blackboard;
        MKT_ASSERT( m_Blackboard, "Blackboard must not be NULL" );
    }

    auto VulkanGraphicsContext::EndFrame() -> void {
        m_CmdList->End();
        m_Device->SubmitCommands( m_CmdList );
    }

    auto VulkanGraphicsContext::BindPipeline( PipelineHandle pipeline, FramePass* Pass ) -> void {
        m_PassInfo[Pass].Pipeline = pipeline;

        VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        VkPipelineBindPoint bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM };

        switch (pipeline->GetPipelineType()) {
            case PipelineType::GRAPHICS_PIPELINE:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case PipelineType::COMPUTE_PIPELINE:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            case PipelineType::RAY_TRACING_PIPELINE:
            default:
                MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::BindPipeline - Unsupported pipeline type." );
                break;
        }

        vkCmdBindPipeline( vkCmd, bindPoint, pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) );
    }


    auto VulkanGraphicsContext::HasDescriptorSets( FramePass *pipeline ) -> bool {
        const auto it{ m_PassInfo.find( pipeline ) };
        return it != m_PassInfo.end() && !it->second.DescriptorSets.empty();
    }

    auto VulkanGraphicsContext::BindPassDescriptors( FramePass *pass ) -> void {
        VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        const auto it{ m_PassInfo.find( pass ) };
        if (it != m_PassInfo.end()) {
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

    auto VulkanGraphicsContext::CreatePassDescriptors( FramePass* pass) -> void {
        if (pass == nullptr) { return; }

        const auto it{ m_PassInfo.find( pass ) };

        if (!HasDescriptorSets( pass )) {
            if (it->second.Pipeline->GetPipelineType() == PipelineType::COMPUTE_PIPELINE) {
                VulkanComputePipeline *vulkanPipeline{ dynamic_cast<VulkanComputePipeline *>( it->second.Pipeline.GetRaw() ) };

                for (const auto& setIndex: vulkanPipeline->GetDescriptorSetIndices()) {
                    // Skip creating descriptor sets for the frame and the textures as those already exist
                    if (setIndex == TEXTURES_DESCRIPTOR_SET_INDEX || setIndex == PER_FRAME_DESCRIPTOR_SET_INDEX) {
                        continue;
                    }

                    const VkDescriptorSetLayout &layout{ vulkanPipeline->GetDescriptorSetLayout( setIndex ) };
                    VkDescriptorSet descriptorSet{ TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layout ) ) };

                    m_PassInfo[pass].DescriptorSets[setIndex] = descriptorSet;
                }
            }

            if (it->second.Pipeline->GetPipelineType() == PipelineType::GRAPHICS_PIPELINE) {
                VulkanGraphicsPipeline *vulkanPipeline{ dynamic_cast<VulkanGraphicsPipeline *>( it->second.Pipeline.GetRaw() ) };

                for (const auto& setIndex: vulkanPipeline->GetDescriptorSetIndices()) {
                    // Skip creating descriptor sets for the frame and the textures as those already exist
                    if (setIndex == TEXTURES_DESCRIPTOR_SET_INDEX || setIndex == PER_FRAME_DESCRIPTOR_SET_INDEX) {
                        continue;
                    }

                    const VkDescriptorSetLayout &layout{ vulkanPipeline->GetDescriptorSetLayout( setIndex ) };
                    VkDescriptorSet descriptorSet{ TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layout ) ) };

                    m_PassInfo[pass].DescriptorSets[setIndex] = descriptorSet;
                }
            }
        }

        if (it != m_PassInfo.end() && it->second.Dirty) {
            for (const auto& [Name, SamplerName, Binding, Type]: it->second.PassResources) {

                switch (Type) {
                    case ShaderResourceType::SHADER_STORAGE_BUFFER:
                    case ShaderResourceType::SHADER_RESOURCE_UNIFORM_BUFFER:
                        PushBuffer( pass, Name, PER_PASS_DESCRIPTOR_SET_INDEX, Binding, Type );
                        break;
                    case ShaderResourceType::SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER:
                        PushImage( pass, Name, SamplerName,  PER_PASS_DESCRIPTOR_SET_INDEX, Binding, Type );
                        break;
                    case ShaderResourceType::SHADER_RESOURCE_UNDEFINED:
                        break;
                }
            }

            // Update once unless strictly necessary
            it->second.Dirty = false;
        }
    }

    auto VulkanGraphicsContext::PushBuffer( FramePass *pass, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType ) -> void {
        const auto it{ m_PassInfo.find( pass ) };
        if (it == m_PassInfo.end()) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushBuffer - Pass does not exist" );
            return;
        }

        FramePassInfo &passInfo{ it->second };
        BufferHandle handle{ m_Blackboard->GetBuffer( name ) };

        DescriptorWriter writer{};
        writer.WriteBuffer( groupBinding, handle->GetNativeHandle( ObjectType::Vk_Buffer ), handle->GetSizeBytes(), 0, ToVkDescriptorType( shaderResourceType ) );

        writer.UpdateSet( VK_DEVICE( m_Device ), passInfo.DescriptorSets.at( groupIndex ) );
    }

    auto VulkanGraphicsContext::PushImage( FramePass *pass,std::string_view textureName, std::string_view samplerName, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType ) -> void {
        const auto it{ m_PassInfo.find( pass ) };
        if (it == m_PassInfo.end()) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushImage - Pass does not exist" );
            return;
        }

        FramePassInfo &passInfo{ it->second };
        TextureHandle textureHandle{ m_Blackboard->GetTexture( textureName ) };

        SamplerHandle samplerHandle{ m_Blackboard->GetSampler( samplerName ) };
        VkSampler sampler{ VK_NULL_HANDLE };

        if (!samplerHandle.IsEmpty()) {
            sampler = samplerHandle->GetNativeHandle(ObjectType::Vk_Sampler);
        }

        DescriptorWriter writer{};

        VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };
        if (textureHandle->GetTextureUsage() == TextureUsage::TEXTURE_USAGE_CUBE) {
            layout = dynamic_cast<VulkanTextureCube *>( textureHandle.GetRaw() )->GetCurrentLayout();
        } else {
            layout = dynamic_cast<VulkanTexture *>( textureHandle.GetRaw() )->GetCurrentLayout();
        }
        writer.WriteImage( groupBinding, textureHandle->GetNativeHandle( ObjectType::Vk_ImageView ), sampler, layout, ToVkDescriptorType( shaderResourceType ) );

        writer.UpdateSet( VK_DEVICE( m_Device ), passInfo.DescriptorSets.at( groupIndex ) );
    }

    auto VulkanGraphicsContext::CreateBindlessTexturesSet() -> void {
        const UInt32 maxBindlessTextures{ SRGTextures::GetMaxBindlessTextureCount() };

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

    auto VulkanGraphicsContext::BindTextureList() -> void {
        vkCmdBindDescriptorSets(
            m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_TexturesPipelineLayout,
            TEXTURES_DESCRIPTOR_SET_INDEX,
            1,
            &m_BindlessTexturesSet,
            0,
            nullptr
        );
    }

    auto VulkanGraphicsContext::BindFrameResources() -> void {

    }

    auto VulkanGraphicsContext::UpdateBindlessTexturesSet(Texture* texture, Sampler* sampler, Size setIndex ) const -> void {
        MKT_ASSERT(setIndex < SRGTextures::GetMaxBindlessTextureCount(), "Set index must be smaller than max bindless textures");

        VkSampler vkSampler{ sampler->GetNativeHandle( ObjectType::Vk_Sampler ) };
        VkImageView vkImageView{ texture->GetNativeHandle( ObjectType::Vk_ImageView ) };

        DescriptorWriter writer{};
        writer.WriteImage( 0, vkImageView, vkSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setIndex )
            .UpdateSet( VK_DEVICE( m_Device ), m_BindlessTexturesSet );
    }

    auto VulkanGraphicsContext::InitSharedShaderResourceGroups() -> void {
        m_SRG[SRGType::SRG_Textures] = CreateScope<SRGTextures>();
    }

    auto VulkanGraphicsContext::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void {
        vkCmdDraw( m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ), vertexCount, instanceCount, firstVertex, firstInstance );
    }

    auto VulkanGraphicsContext::BindIndexBuffer( BufferHandle indexBuffer ) -> void {
        VkCommandBuffer cmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        vkCmdBindIndexBuffer( cmd, indexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), 0, VK_INDEX_TYPE_UINT32 ); // TODO: infer index buffer data type
    }

    auto VulkanGraphicsContext::BindVertexBuffer( BufferHandle vertexBuffer, UInt32 binding ) -> void {
        VkCommandBuffer cmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        const std::array<VkDeviceSize, 1> offsets{};
        const std::array<VkBuffer, 1> vertexBuffers{ vertexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ) };

        vkCmdBindVertexBuffers( cmd, binding, 1, vertexBuffers.data(), offsets.data() );
    }

    auto VulkanGraphicsContext::DrawInstanced( Size indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance ) -> void {
        VkCommandBuffer cmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        vkCmdDrawIndexed( cmd, indexCount, instanceCount, 0, 0, 0 );
    }

    auto VulkanGraphicsContext::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void { m_CmdList->Dispatch( invX, invY, invZ ); }

    auto VulkanGraphicsContext::SetViewport( const PassViewport &vp ) -> void {
        VkCommandBuffer cmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        VkViewport viewport{};

        viewport.x = vp.X;
        viewport.y = vp.Height;
        viewport.width = vp.Width;
        viewport.height = -vp.Height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport( cmd, 0, 1, std::addressof( viewport ) );
    }

    auto VulkanGraphicsContext::SetScissor( const PassScissor &vp ) -> void {
        VkCommandBuffer cmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        VkRect2D scissor{};
        scissor.offset = { static_cast<Int32>( vp.X ), static_cast<Int32>( vp.X ) };
        scissor.extent = { static_cast<UInt32>( vp.Width ), static_cast<UInt32>( vp.Height ) };

        vkCmdSetScissor( cmd, 0, 1, std::addressof( scissor ) );
    }

    auto VulkanGraphicsContext::PushImage( TextureHandle texture ) -> Int32 {
        if (texture.IsEmpty()) {
            return SRGTextures::INVALID_TEXTURE_INDEX;
        }

        SRGTextures* textureShaderGroup{ dynamic_cast<SRGTextures *>( m_SRG[SRGType::SRG_Textures].get() ) };

        SamplerHandle sampler{ m_Device->GetDummySampler() };
        MKT_ASSERT( !sampler.IsEmpty(), "Dummy sampler cannot be empty" );

        // If combined sampler has already been registered return its index
        Int32 index{ textureShaderGroup->GetIndex(texture, sampler) };
        if (index != SRGTextures::INVALID_TEXTURE_INDEX) {
            return index;
        }

        // If combined sampler does not exist, register it
        Int32 result{ textureShaderGroup->Bind( texture, sampler ) };
        if (result != SRGTextures::INVALID_TEXTURE_INDEX) {
            UpdateBindlessTexturesSet( texture.GetRaw(), sampler.GetRaw(), result );
        }

        return result;

    }

    auto VulkanGraphicsContext::BindPassResources( FramePass* pass ) -> void {
        CreatePassDescriptors( pass );

        BindPassDescriptors( pass );
    }

    auto VulkanGraphicsContext::GetPassSRG( FramePass* pass ) -> SRGPerPass * {
        return std::addressof( m_PassInfo[pass].PassResources );
    }

    auto VulkanGraphicsContext::InsertResourceBarrier( FramePass *pass ) -> void {
        static VkPipelineStageFlags lastStage{ VK_PIPELINE_STAGE_NONE };

        VkPipelineStageFlagBits dstStage{ pass->IsCompute()
                ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };

        if (lastStage == VK_PIPELINE_STAGE_NONE) {
            lastStage = dstStage;
            return;
        }

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
            lastStage,
            dstStage,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr
        );

        lastStage = dstStage;
    }

}// namespace Mikoto