//
// Created by kate on 11/25/25.
//

#include "Renderer/Vulkan/VulkanGraphicsContext.hh"

#include "Core/Profiler.hh"
#include "Renderer/Vulkan/VulkanDevice.hh"
#include "Renderer/Vulkan/VulkanPipeline.hh"
#include "Renderer/Vulkan/VulkanTexture.hh"

namespace Mikoto {

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

    auto VulkanGraphicsContext::RegisterPass( FramePass *pass, PipelineHandle pipeline ) -> void {
        if (m_PassInfo.contains( pass )) {
            return;
        }

        if (pipeline->GetPipelineType() == PipelineType::GRAPHICS_PIPELINE) {
            VulkanGraphicsPipeline* graphicsPipeline{ dynamic_cast<VulkanGraphicsPipeline*>(pipeline.GetRaw()) };

            for (auto& [setIndex, setLayout] : graphicsPipeline->m_ReflectionData.setLayouts) {
                const VkDescriptorSetLayout& layoutTextures{ graphicsPipeline->GetDescriptorSetLayout( setIndex ) };
                auto descriptorSet{ TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layoutTextures ) ) };

                m_PassInfo[pass].DescriptorSets.emplace( setIndex, descriptorSet );
            }
        }

        if (pipeline->GetPipelineType() == PipelineType::COMPUTE_PIPELINE) {
            VulkanComputePipeline* computePipeline{ dynamic_cast<VulkanComputePipeline*>(pipeline.GetRaw()) };

            for (auto& [setIndex, setLayout] : computePipeline->m_ReflectionData.setLayouts) {
                const VkDescriptorSetLayout& layoutTextures{ computePipeline->GetDescriptorSetLayout( setIndex ) };
                auto descriptorSet{ TO_VK_DEVICE( m_Device )->AllocateDescriptorSet( std::addressof( layoutTextures ) ) };

                m_PassInfo[pass].DescriptorSets.emplace( setIndex, descriptorSet );
            }
        }
    }

    auto VulkanGraphicsContext::BeginFrame( CommandListHandle cmd ) -> void {
        m_CmdList = cmd;
    }

    auto VulkanGraphicsContext::EndFrame() -> void {

    }

    auto VulkanGraphicsContext::BindPipeline( PipelineHandle pipeline ) -> void {
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

    auto VulkanGraphicsContext::PushBuffer( FramePass *pass, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType, ShaderResourceVisibility visibility, BufferHandle handle ) -> void {
        const auto it{ m_PassInfo.find( pass  ) };
        if (it == m_PassInfo.end()) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushBuffer - Pass does not exist" );
            return;
        }

        FramePassInfo& passInfo{ it->second };

        passInfo.BoundBuffers.emplace( std::make_pair(groupIndex, groupBinding), handle );

        DescriptorWriter writer{};
        writer.WriteBuffer( groupBinding, handle->GetNativeHandle( ObjectType::Vk_Buffer ), handle->GetSizeBytes(), 0, VK_DESCRIPTOR_TYPE_MAX_ENUM );

        writer.UpdateSet( VK_DEVICE(m_Device), passInfo.DescriptorSets.at( groupIndex ) );
    }

    auto VulkanGraphicsContext::PushImage( FramePass *pass, UInt32 groupIndex, UInt32 groupBinding, ShaderResourceType shaderResourceType, ShaderResourceVisibility visibility, TextureHandle handle ) -> void {
        const auto it{ m_PassInfo.find( pass  ) };
        if (it == m_PassInfo.end()) {
            MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::PushImage - Pass does not exist" );
            return;
        }

        FramePassInfo& passInfo{ it->second };

        passInfo.BoundTextures.emplace( std::make_pair(groupIndex, groupBinding), handle );

        DescriptorWriter writer{};
        VkSampler sampler{ VK_NULL_HANDLE };
        VkImageLayout layout{ dynamic_cast<VulkanTexture*>(handle.GetRaw())->GetCurrentLayout() };
        writer.WriteImage( groupBinding, handle->GetNativeHandle( ObjectType::Vk_ImageView ), sampler, layout, VK_DESCRIPTOR_TYPE_MAX_ENUM );

        writer.UpdateSet( VK_DEVICE(m_Device), passInfo.DescriptorSets.at( groupIndex ) );
    }

    auto VulkanGraphicsContext::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void {
        vkCmdDraw( m_CmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ), vertexCount, instanceCount, firstVertex, firstInstance );
    }

    auto VulkanGraphicsContext::Dispatch( UInt32 invX, UInt32 invY, UInt32 invZ ) -> void {}

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