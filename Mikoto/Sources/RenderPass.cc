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
        ShaderModuleHandle pbrVertex{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/PBRVertexShader.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle pbrFragment{ ShaderLibrary::Get()->LoadShader( "./Resources/Shaders/vulkan-spirv/PBRFragmentShader.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { pbrVertex, pbrFragment };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;

        m_Pipeline = m_Device->CreatePipeline(pipelineDesc);
    }

    auto ShadingPass::Shutdown() -> void {
    }

    auto ShadingPass::SetRenderData( Scene *scene, std::span<Light> lights ) -> void {
        m_Scene = scene;
        m_Lights = lights;
    }

    auto ShadingPass::Render( const FrameContext &frame, RendererBackend *backend ) -> void {
        backend->BeginRender(frame, {
            .DepthAttachment{ m_DepthTarget },
            .ColorAttachments{ { m_ColorTarget } }
        });

        backend->DrawScene(m_Scene, m_Lights);

        backend->EndRender();
    }

    auto ShadingPass::OnResize( const UInt32 width, const UInt32 height ) -> void {

    }

    auto ComputeBasic::Init( GpuDevice *device ) -> void {
    }

    auto ComputeBasic::Shutdown() -> void {
    }

    auto ComputeBasic::Execute() -> void {

    }
}// namespace Mikoto