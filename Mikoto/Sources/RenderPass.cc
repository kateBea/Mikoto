#include "Renderer/RenderPass.hh"

#include <Renderer/RendererBackend.hh>
#include <Scene/Component.hh>
#include <Renderer/Pipeline.hh>

namespace Mikoto {

    GBufferPass::GBufferPass( const GBufferPassDescription &description ) {}

    auto GBufferPass::Init( GpuDevice *device ) -> void {
        // Create textures for GBuffer (Albedo, Normal, Position)
        m_GBuffer.Albedo = device->CreateTexture( TextureDescription{ .Format = TextureFormat::TEXTURE_FORMAT_RGBA8,
                                                    .Width = m_ViewportWidth,
                                                    .Height = m_ViewportHeight } );

        m_GBuffer.Normal = device->CreateTexture( TextureDescription{ .Format = TextureFormat::TEXTURE_FORMAT_RGBA8,
                                                    .Width = m_ViewportWidth,
                                                    .Height = m_ViewportHeight } );

        m_GBuffer.Position = device->CreateTexture( TextureDescription{ .Format = TextureFormat::TEXTURE_FORMAT_RGBA8,
                                                    .Width = m_ViewportWidth,
                                                    .Height = m_ViewportHeight } );

        // Create framebuffer for GBuffer pass
        FramebufferDescription description{};
        description
                .AddAttachment( m_GBuffer.Albedo )
                .AddAttachment( m_GBuffer.Normal )
                .AddAttachment( m_GBuffer.Position )
                .WithWidth( m_ViewportWidth )
                .WithHeight( m_ViewportHeight );

        m_Framebuffer = device->CreateFramebuffer( description );

        GraphicsPipelineDescription pipelineDesc{};

        m_Pipeline = device->CreateGraphicsPipeline( pipelineDesc );
    }

    auto GBufferPass::Shutdown() -> void {

    }

    auto GBufferPass::Execute( RendererBackend *backend ) -> void {
        CommandListHandle commandList{ backend->CreateCommandList() };
        commandList->BeginRecording();

        commandList->BeginRenderPass( this );

        for (const auto &entity: m_Scene->GetEntities() | std::views::values) {
            if (entity->HasComponent<RenderComponent>()) {
                RenderComponent &renderComponent{ entity->GetComponent<RenderComponent>() };
                MaterialComponent &materialComponent{ entity->GetComponent<MaterialComponent>() };
                TransformComponent &transformComponent{ entity->GetComponent<TransformComponent>() };

                const auto subMesh{ renderComponent.GetMesh() };
                const auto material{ materialComponent.GetMaterial() };
                const auto& transform{ transformComponent.GetTransform() };

                commandList->SubmitMeshDraw( subMesh, material, transform );
            }
        }

        commandList->EndRecording();
        backend->SubmitCommandList( commandList );
    }

    auto LightCullingPass::Init( GpuDevice *device ) -> void {
        m_LightClusters = device->CreateBuffer(BufferDescription{
            .Usage = BufferUsage::BUFFER_USAGE_SHADER_STORAGE
        });

        m_LightCulling = device->CreateBuffer(BufferDescription{
            .Usage = BufferUsage::BUFFER_USAGE_SHADER_STORAGE
        });
    }

    auto LightCullingPass::Shutdown() -> void {}

    auto LightCullingPass::Execute( RendererBackend *backend ) -> void {
        CommandListHandle commandList{ backend->CreateCommandList() };
        commandList->BeginRecording();
        commandList->BeginComputePass( this );

        commandList->EndRecording();
        backend->SubmitCommandList( commandList );
    }

#if false
#endif
}
