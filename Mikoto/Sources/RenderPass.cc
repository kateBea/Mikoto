#include "Renderer/RenderPass.hh"

#include <Scene/Component.hh>

namespace Mikoto {

    GBufferPass::GBufferPass( const UInt32_T viewportWidth, const UInt32_T viewportHeight )
        : RenderPass{ viewportWidth, viewportHeight } {
    }

    // GBufferPass
    auto GBufferPass::Init( GpuDevice* device ) -> void {
        // Create textures for GBuffer (Albedo, Normal, Position)
        m_GBuffer.Albedo = device->CreateTexture( { .format = TextureFormat::TEXTURE_FORMAT_RGBA8,
                                                    .width = m_ViewportWidth,
                                                    .height = m_ViewportHeight } );

        m_GBuffer.Normal = device->CreateTexture( { .format = TextureFormat::TEXTURE_FORMAT_RGBA8,
                                                    .width = m_ViewportWidth,
                                                    .height = m_ViewportHeight } );

        m_GBuffer.Position = device->CreateTexture( { .format = TextureFormat::TEXTURE_FORMAT_RGBA8,
                                                      .width = m_ViewportWidth,
                                                      .height = m_ViewportHeight } );

        // Create framebuffer for GBuffer pass
        FramebufferDescription description{};
        description
                .AddAttachment( m_GBuffer.Albedo )
                .AddAttachment( m_GBuffer.Normal )
                .AddAttachment( m_GBuffer.Position )
                .WithWidth( m_ViewportWidth )
                .WithHeight( m_ViewportHeight );

        m_Framebuffer = device->CreateFramebuffer( description );

        // Create a buffer for mesh instance data
        m_MeshInstanceData = device->CreateBuffer( {
                .usage = BufferUsage::BUFFER_USAGE_VERTEX,
                .size = sizeof( MeshInstanceData ) * 1000
        } );

        // Create the pipeline layout for binding resources (such as buffers and textures)
        BindingSetLayoutDescription bindingSetLayoutDesc{};
        bindingSetLayoutDesc
                .AddBinding( BindingSetItem::Texture( 0, ShaderStage::Fragment ) ) // Albedo texture (binding 0)
                .AddBinding( BindingSetItem::Sampler( 1, ShaderStage::Fragment ) );// Sampler (binding 1)

        m_MaterialLayout = device->CreateBindingSetLayout( bindingSetLayoutDesc );

        // Create the graphics pipeline for GBuffer pass
        PipelineDescription pipelineDesc{};
        pipelineDesc
                .AddShaderStage( device->CreateShaderStage( ShaderStage::Vertex, "path/to/vertex_shader.spv" ) )
                .AddShaderStage( device->CreateShaderStage( ShaderStage::Fragment, "path/to/fragment_shader.spv" ) )
                .WithDepthStencilState( DepthStencilState::DepthTestWrite )
                .WithRasterizationState( RasterizationState::FrontFaceCCW );

        m_GBufferPipeline = device->CreateGraphicsPipeline( pipelineDesc );
    }


    auto GBufferPass::Execute( GpuDevice* device, FrameBlackboard* blackboard ) -> void {
        CommandListHandle cmd = device->CreateCommandList();
        cmd->Begin();

        cmd->SetViewport({ 0, 0, m_ViewportWidth, m_ViewportHeight });
        cmd->SetGraphicsPipeline(m_GBufferPipeline);
        cmd->SetVertexBuffer(1, m_MeshInstanceData);

        for (Entity* entity : m_VisibleMeshes) {
            MeshNode* mesh{ entity->GetComponent<RenderComponent>().GetMesh() };
            cmd->SetVertexBuffer(0, mesh->GetVertexBuffer());
            cmd->SetVertexBuffer(1, entity->GetInstanceBuffer());

            cmd->SetIndexBuffer(mesh.GetIndexBuffer());
        }

        const auto& textures = mesh.GetTextures();
        if (!textures.empty()) {
            BindingSetDescription materialBindings{};
            materialBindings.SetLayout = m_MaterialLayout;
            materialBindings.AddBinding(BindingSetItem::Texture(0, textures[0]));
            materialBindings.AddBinding(BindingSetItem::Sampler(1, m_DefaultSampler));
            auto bindingSet = device->CreateBindingSet(materialBindings);
            cmd->SetBindingSet(0, bindingSet);
        }

        const uint32_t indexCount = GetIndexCountFor(mesh);
        const uint32_t instanceCount = GetInstanceCountFor(mesh);
        cmd->DrawIndexed(indexCount, instanceCount, 0, 0, 0);

        cmd->Close();
        device->SubmitCommandList(cmd);
    }
}
