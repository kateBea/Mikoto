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

    auto VulkanGraphicsContext::Init() -> void {}

    auto VulkanGraphicsContext::Shutdown() -> void {}

    auto VulkanGraphicsContext::BeginRender( GfxRenderInfo &beginInfo ) -> void {
        VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        std::vector<VkRenderingAttachmentInfo> colorImages{};

        for (auto &colorImage: beginInfo.ColorRenderTargets) {
            VkRenderingAttachmentInfo &colorAttachment{ colorImages.emplace_back( VkRenderingAttachmentInfo{} ) };
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = colorImage->GetNativeHandle( ObjectType::Vk_ImageView );
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = { beginInfo.ClearColor.r, beginInfo.ClearColor.g, beginInfo.ClearColor.b, beginInfo.ClearColor.a };
        }

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = beginInfo.DepthRenderTarget->GetNativeHandle( ObjectType::Vk_ImageView );
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { { 0, 0 }, { 1920, 1080 } };
        renderingInfo.layerCount = 1;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<UInt32>( colorImages.size() );
        renderingInfo.pColorAttachments = colorImages.data();
        renderingInfo.pDepthAttachment = std::addressof( depthAttachment );

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

    auto VulkanGraphicsContext::BindPipeline( PipelineHandle pipeline, PassResources &resources ) -> void {
        VkCommandBuffer vkCmd{ m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };

        // Bind the resources that are viable for this pipeline.
        // If they do not exist we create them first
        CreatePassDescriptors( pipeline.GetRaw(), resources );

        // Bind resources if any
        const auto it{ m_PassInfo.find( pipeline.GetRaw() ) };
        if (it != m_PassInfo.end()) {
            VkPipelineLayout pipelineLayout{ pipeline->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

            for (const auto &[setIndex, descriptorSet]: it->second.DescriptorSets) {
                switch (pipeline->GetPipelineType()) {
                    case PipelineType::GRAPHICS_PIPELINE:
                        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, setIndex, 1, std::addressof( descriptorSet ), 0, nullptr );
                        break;
                    case PipelineType::COMPUTE_PIPELINE:
                        vkCmdBindDescriptorSets( vkCmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, setIndex, 1, std::addressof( descriptorSet ), 0, nullptr );
                        break;
                }
            }
        }

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

    auto VulkanGraphicsContext::BindBuffer( BufferHandle texture ) -> void {}

    auto VulkanGraphicsContext::RegisterImage( TextureHandle texture ) -> void {
        SamplerHandle dummySampler{ m_Device->GetDummySampler() };
        RegisterImage( texture, dummySampler );
    }

    auto VulkanGraphicsContext::RegisterImage( TextureHandle texture, SamplerHandle sampler ) -> void {
        const auto it{ m_CombinedSamplerIndices.find( std::make_pair( texture.GetRaw(), sampler.GetRaw() ) ) };

        // Combined image sampler does not exist
        if (it == m_CombinedSamplerIndices.end()) { m_CombinedSamplerIndices.emplace( std::make_pair( texture.GetRaw(), sampler.GetRaw() ), m_CombinedSamplerIndices.size() ); }
    }

    auto VulkanGraphicsContext::HasDescriptorSets( IPipeline *pipeline ) -> bool {
        const auto it{ m_PassInfo.find( pipeline ) };
        return it != m_PassInfo.end() && !it->second.DescriptorSets.empty();
    }

    auto VulkanGraphicsContext::CreatePassDescriptors( IPipeline *pipeline, PassResources &resources ) -> void {
        if (pipeline == nullptr) { return; }

        // Create the descriptor se
        if (!HasDescriptorSets( pipeline )) {
            if (pipeline->GetPipelineType() == PipelineType::COMPUTE_PIPELINE) {
                VulkanComputePipeline *vulkanPipeline{ dynamic_cast<VulkanComputePipeline *>( pipeline ) };

                const Size descriptorLayoutCount{ vulkanPipeline->GetDescriptorLayoutCount() };
                for (Size index{}; index < descriptorLayoutCount; ++index) {
                    const VkDescriptorSetLayout &layout{ vulkanPipeline->GetDescriptorSetLayout( index ) };
                    VkDescriptorSet descriptorSet{ TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layout ) ) };
                    m_PassInfo[pipeline].DescriptorSets[index] = descriptorSet;
                }
            }

            if (pipeline->GetPipelineType() == PipelineType::GRAPHICS_PIPELINE) {
                VulkanGraphicsPipeline *vulkanPipeline{ dynamic_cast<VulkanGraphicsPipeline *>( pipeline ) };

                const Size descriptorLayoutCount{ vulkanPipeline->GetDescriptorLayoutCount() };
                for (Size index{}; index < descriptorLayoutCount; ++index) {
                    const VkDescriptorSetLayout &layout{ vulkanPipeline->GetDescriptorSetLayout( index ) };
                    VkDescriptorSet descriptorSet{ TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layout ) ) };
                    m_PassInfo[pipeline].DescriptorSets[index] = descriptorSet;
                }
            }
        }

        // Updates descriptors
        const auto it{ m_PassInfo.find( pipeline ) };
        if (it != m_PassInfo.end() && it->second.Dirty) {

            for (auto &[srgType, group]: resources.m_SRGs) {

                if (srgType == SRGType::SRG_PerCompute) {
                    // This goes to set 0 of every compute pipeline
                    for (auto &binding : group) {
                        switch (binding.Type) {
                            case ShaderResourceType::SHADER_STORAGE_BUFFER:
                            case ShaderResourceType::SHADER_RESOURCE_UNIFORM_BUFFER:
                                PushBuffer( pipeline, binding.Name, 0, binding.Binding, binding.Type );
                                break;
                            case ShaderResourceType::SHADER_RESOURCE_COMBINED_IMAGE_SAMPLER:
                                PushImage( pipeline, binding.Name, 0, binding.Binding, binding.Type );
                                break;
                            case ShaderResourceType::SHADER_RESOURCE_UNDEFINED:
                                break;
                        }
                    }
                }
            }

            // Update once unless strictly necessary
            it->second.Dirty = false;
        }
    }

    auto VulkanGraphicsContext::PushBuffer( IPipeline *pipeline, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType ) -> void {
        const auto it{ m_PassInfo.find( pipeline ) };
        if (it == m_PassInfo.end()) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushBuffer - Pass does not exist" );
            return;
        }

        FramePassInfo &passInfo{ it->second };
        BufferHandle handle{ m_Blackboard->GetBuffer( name ) };

        passInfo.BoundBuffers.emplace( std::make_pair( groupIndex, groupBinding ), handle );

        DescriptorWriter writer{};
        writer.WriteBuffer( groupBinding, handle->GetNativeHandle( ObjectType::Vk_Buffer ), handle->GetSizeBytes(), 0, ToVkDescriptorType( shaderResourceType ) );

        writer.UpdateSet( VK_DEVICE( m_Device ), passInfo.DescriptorSets.at( groupIndex ) );
    }

    auto VulkanGraphicsContext::PushImage( IPipeline *pipeline, std::string_view name, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType ) -> void {
        const auto it{ m_PassInfo.find( pipeline ) };
        if (it == m_PassInfo.end()) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushImage - Pass does not exist" );
            return;
        }

        FramePassInfo &passInfo{ it->second };
        TextureHandle handle{ m_Blackboard->GetTexture( name ) };

        passInfo.BoundTextures.emplace( std::make_pair( groupIndex, groupBinding ), handle );

        DescriptorWriter writer{};
        VkSampler sampler{ VK_NULL_HANDLE };
        VkImageLayout layout{ dynamic_cast<VulkanTexture *>( handle.GetRaw() )->GetCurrentLayout() };
        writer.WriteImage( groupBinding, handle->GetNativeHandle( ObjectType::Vk_ImageView ), sampler, layout, ToVkDescriptorType( shaderResourceType ) );

        writer.UpdateSet( VK_DEVICE( m_Device ), passInfo.DescriptorSets.at( groupIndex ) );
    }

    auto VulkanGraphicsContext::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void { vkCmdDraw( m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ), vertexCount, instanceCount, firstVertex, firstInstance ); }

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
}// namespace Mikoto