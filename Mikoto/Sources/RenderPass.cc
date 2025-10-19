//
// Created by kate on 10/16/25.
//

#include <Material/ShaderLibrary.hh>
#include <Renderer/RenderPass.hh>

namespace Mikoto {

    auto ShadingPass::Init( GpuDevice *device ) -> void {
        m_Device = device;

        // Color Device attachment
        TextureDescription colorDesc{};
        colorDesc
                .WithWidth( 1920 )  // framebuffer width
                .WithHeight( 1080 )// framebuffer height
                .WithChannelCount( 4 )          // RGBA
                .WithData( nullptr )            // no initial data
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM )// common for color attachments
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_ColorTarget = m_Device->CreateTexture( colorDesc );
        m_ColorTarget->SetDebugName( "Final pass shading texture" );

        // Depth attachment
        TextureDescription depthDesc{};
        depthDesc
                .WithWidth( 1920 )
                .WithHeight( 1080 )
                .WithChannelCount( 1 )
                .WithData( nullptr )
                .WithType( TextureType::TEXTURE_2D )
                .WithTextureUsage( TextureUsage::TEXTURE_USAGE_DEPTH )
                .WithFormat( TextureFormat::TEXTURE_FORMAT_D32_FLOAT )
                .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_DepthTarget = m_Device->CreateTexture( depthDesc );
        m_DepthTarget->SetDebugName( "Final pass depth texture" );

        // Build your PBR graphics pipeline
        ShaderModuleHandle pbrVertex{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/FullscreenTriangle_Vert.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle pbrFragment{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/FullscreenTriangle_Frag.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { pbrVertex, pbrFragment };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.DefaultVertexLayout = {};
        pipelineDesc.DepthTexture = m_DepthTarget;
        pipelineDesc.ColorAttachments = { m_ColorTarget };

        m_Pipeline = m_Device->CreatePipeline(pipelineDesc);
    }

    auto ShadingPass::Shutdown() -> void {
    }

    auto ShadingPass::SetScene( Scene *scene) -> void {
        m_Scene = scene;
    }

    auto ShadingPass::Render( RendererBackend *backend, CommandListHandle cmd ) -> void {
        backend->BeginRender( { .DepthAttachment{ m_DepthTarget },
                                .ColorAttachments{ { m_ColorTarget } } },
                              cmd );

        backend->SetViewport( 0, 0, 1920, 1080 );
        backend->SetPipeline( m_Pipeline );

        backend->DrawScene( m_Scene );

        backend->EndRender();
    }

    auto ShadingPass::GetFinalComposition() -> TextureHandle {
        return m_ColorTarget;
    }

    auto ShadingPass::OnResize( const UInt32 width, const UInt32 height ) -> void {

    }

    auto ComputeBasic::Init( GpuDevice *device ) -> void {
    }

    auto ComputeBasic::Shutdown() -> void {
    }

    auto ComputeBasic::Execute(CommandListHandle cmd) -> void {

    }
}// namespace Mikoto