/**
 * VulkanRenderer.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <array>

// Third-Party Libraries
#include <volk.h>

#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {

    struct FrameUBO {
        glm::mat4 View{};
        glm::mat4 Projection{};
    };

    struct ObjectUBO {
        glm::mat4 Transform{};
    };

    VulkanRenderer::VulkanRenderer( GpuDevice *device, std::string_view name )
        : RendererBackend{ { name, device } }
    {}

    auto VulkanRenderer::Init() -> void {
        // Create data that is needed per frame
        BufferDescription bufferDesc{};
        bufferDesc.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithSizeBytes( sizeof(FrameUBO) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STREAM );
        m_FrameUBOBuffer = m_GraphicsDevice->CreateBuffer( bufferDesc );

        m_IsInitialized = true;
    }

    auto VulkanRenderer::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }
    }

    auto VulkanRenderer::EndRender() -> void {
        VkCommandBuffer cmd{ *m_GraphicsCommandList->GetNativeHandle<VulkanCmdList>() };

        vkCmdEndRendering(cmd);

        // transition image after wars so it can be read from, i am not entirely sure about this
        for (AttachmentInfo& info : m_ColorAttachments) {
            VulkanTexture* texture{ dynamic_cast<VulkanTexture *>(info.Image.GetRaw()) };
            texture->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd );
        }
    }

    auto VulkanRenderer::BeginRender( const RenderInfo &info, CommandListHandle cmdList ) -> void {
        m_GraphicsCommandList = cmdList;

        // Save attachments to process later
        m_ColorAttachments = info.ColorAttachments;
        m_DepthAttachment = info.DepthAttachment;

        VkCommandBuffer nativeCmd{ *m_GraphicsCommandList->GetNativeHandle<VulkanCmdList>() };

        // Dynamic rendering setup
        std::vector<VkRenderingAttachmentInfo> colorAttachments;
        colorAttachments.reserve( info.ColorAttachments.size() );

        for ( const auto &color: info.ColorAttachments ) {
            const VulkanTexture *colorImage{ dynamic_cast<const VulkanTexture *>( color.Image.GetRaw() ) };
            VkRenderingAttachmentInfo colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = *colorImage->GetView();
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = color.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            colorAttachment.storeOp = color.Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.clearValue.color = { color.ClearColor.x, color.ClearColor.y, color.ClearColor.z, color.ClearColor.w };
            colorAttachments.push_back( colorAttachment );
        }

        const VulkanTexture *depthImage{ dynamic_cast<const VulkanTexture *>( info.DepthAttachment.Image.GetRaw() ) };

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = *depthImage->GetView();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = info.DepthAttachment.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = info.DepthAttachment.Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = { info.DepthAttachment.ClearDepth, 0 };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { VkOffset2D{ 0, 0 }, VkExtent2D{ 1920, 1080 } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<uint32_t>( colorAttachments.size() );
        renderingInfo.pColorAttachments = colorAttachments.data();
        renderingInfo.pDepthAttachment = std::addressof( depthAttachment );

        vkCmdBeginRendering( nativeCmd, std::addressof( renderingInfo ) );
    }

    auto VulkanRenderer::SetPipeline( PipelineHandle pipeline ) -> void {
        m_Pipeline = pipeline;
    }

    auto VulkanRenderer::DrawScene( Scene *scene ) -> void {
        VkCommandBuffer cmd{*m_GraphicsCommandList->GetNativeHandle<VulkanCmdList>()};
        VulkanGraphicsPipeline* pipeline{m_Pipeline->GetNativeHandle<VulkanGraphicsPipeline>()};

        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Get());

        // TODO: test only this goees for the FullScreen shaders
        vkCmdDraw(cmd, 3, 1, 0, 0); // Only 3 vertices, no vertex buffers or descriptors

        // We descriptor sets per frequency usage
        // Bind per-frame descriptor set (set 0)
        //vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        //                        pipeline->GetLayout(), 0, 1, std::addressof( m_FrameDescriptorSet ), 0, nullptr);
        //
        // // Draw each mesh
        // for (size_t i = 0; i < scene->GetRenderableMeshes().size(); ++i) {
        //     auto& mesh = scene->GetRenderableMeshes()[i];
        //
        //     // --- Update object transform ---
        //     ObjectUBO objectUBO{};
        //     objectUBO.Transform = mesh.transform;
        //     m_ObjectUBOBuffer->CopyFromBlock(std::addressof(objectUBO), sizeof(ObjectUBO),
        //                                     i * sizeof(ObjectUBO));
        //
        //     // Bind per-object descriptor set
        //     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        //                             pipeline->GetLayout(), 1, 1, &m_ObjectDescriptorSet, 0, nullptr);
        //
        //     // Bind vertex/index buffers
        //     VkDeviceSize offsets[] = {0};
        //     vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, offsets);
        //     vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        //
        //     // Draw
        //     vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        // }
    }

    auto VulkanRenderer::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto VulkanRenderer::SetCamera( const Camera *camera ) -> void {
        FrameUBO frameUBO{};
        frameUBO.View = camera->GetViewMatrix();
        frameUBO.Projection = camera->GetProjection();

        m_FrameUBOBuffer->CopyFromBlock(std::addressof( frameUBO ), sizeof(FrameUBO));
    }

    auto VulkanRenderer::SetViewport( const float x, const float y, const float width, const float height ) -> void {
        VkCommandBuffer cmd{ *m_GraphicsCommandList->GetNativeHandle<VulkanCmdList>() };

        m_Viewport.x = x;
        m_Viewport.y = y;
        m_Viewport.width  = width;
        m_Viewport.height = height;
        m_Viewport.minDepth = 0.0f;
        m_Viewport.maxDepth = 1.0f;

        m_Scissor.offset = { static_cast<Int32>(x), static_cast<Int32>(y) };
        m_Scissor.extent = { static_cast<UInt32>(width), static_cast<UInt32>(height) };

        vkCmdSetViewport(cmd, 0, 1, std::addressof( m_Viewport ));
        vkCmdSetScissor(cmd, 0, 1, std::addressof( m_Scissor ));
    }

}// namespace Mikoto