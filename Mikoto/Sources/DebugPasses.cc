//
// Created by zanet on 1/5/2026.
//

#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Passes/DebugPasses.hh>

namespace Mikoto {

    auto MaterialPreviewPass::Setup( FrameGraphBuilder& builder ) -> void {

    }

    auto MaterialPreviewPass::Execute( PassCommandList& cmdList ) -> void {

    }

    auto TextPass::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        // Configure pipeline stage
        GraphicsPipelineDescription graphicseDesc{};

        pipelineDesc.Description = graphicseDesc;

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/MSDFText_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/MSDFText_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        builder.CreateNamedPipeline( "TextPass_Pipeline", pipelineDesc );

        builder.WriteTexture( this, "FinalCompositionPass_ColorTarget" );
        builder.WriteTexture( this, "FinalCompositionPass_DepthTarget" );
    }

    auto TextPass::Execute( PassCommandList& commandList ) -> void {
        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "FinalCompositionPass_DepthTarget" );

        commandList.BeginRender(this);

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.BindPipeline( "ShadowPass_Pipeline" );

        // Meshes
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, TextComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& textComponent{ registry.get<TextComponent>( entity ) };
            auto& materialComp{ registry.get<MaterialComponent>( entity ) };
        }

        commandList.EndRender();
    }

    auto TextPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto HelloCubePass::Setup( FrameGraphBuilder& builder ) -> void {
    }

    auto HelloCubePass::Execute( PassCommandList& cmdList ) -> void {
    }

    auto SimpleComputePass::Setup( FrameGraphBuilder& builder ) -> void {
        PipelineDescription pipelineDesc{};
        pipelineDesc.Description = ComputePipelineDescription{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE_STAGE );

        builder.CreateNamedPipeline( "SimpleComputePass_Pipeline", pipelineDesc );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( 30 * sizeof( float ) );
        builder.CreateNamedBuffer( "SimpleComputePass_Result", lightsBuffer );

        builder.WriteBuffer( this, "SimpleComputePass_Result" );
    }

    auto SimpleComputePass::Execute( PassCommandList& commandList ) -> void {
        commandList.BeginCompute(this);
        commandList.BindPipeline( "SimpleComputePass_Pipeline" );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "SimpleComputePass_Result", 0 );
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        // Prime numbers up until this value
        constexpr UInt32 limitNumbers{ 30 };

        // matches shader's local_size_x
        constexpr UInt32 localSize{ 64 };
        constexpr UInt32 groupCount{ ( limitNumbers + localSize - 1 ) / localSize };

        commandList.Dispatch( groupCount, 1, 1 );

        commandList.EndCompute();
    }

    auto HelloTrianglePass::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/HelloTriangle_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/HelloTriangle_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        pipelineDesc.Description = GraphicsPipelineDescription{
            .VertexAttributesSpec{}
        };

        // TODO: temporary, specify the render targets this pipeline outputs to
        pipelineDesc.ColorRenderTargets.emplace_back( "HelloTrianglePass_ColorTarget" );
        pipelineDesc.DepthRenderTargets = "HelloTrianglePass_DepthTarget";

        builder.CreateNamedPipeline( "HelloTrianglePass_Pipeline", pipelineDesc );

        builder.CreateColorRenderTarget( "HelloTrianglePass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );
        builder.CreateDepthRenderTarget( "HelloTrianglePass_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

        builder.WriteTexture( this, "HelloTrianglePass_ColorTarget" );
        builder.WriteTexture( this, "HelloTrianglePass_DepthTarget" );
    }

    auto HelloTrianglePass::Execute( PassCommandList& commandList ) -> void {
        commandList.SetColorRenderTarget( "HelloTrianglePass_ColorTarget" );
        commandList.SetDepthRenderTarget( "HelloTrianglePass_DepthTarget" );
        commandList.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );

        commandList.BeginRender(this);
        commandList.BindPipeline( "HelloTrianglePass_Pipeline" );

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.Draw( 3, 1, 0, 0 );

        commandList.EndRender();
    }

    auto HelloTexture::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/FullscreenTriangle_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/FullscreenTriangle_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        pipelineDesc.Description = GraphicsPipelineDescription{
            .VertexAttributesSpec{},
            .PrimitiveTopology{ Topology::TRIANGLE_STRIP }
        };

        // TODO: temporary, specify the render targets this pipeline outputs to
        pipelineDesc.ColorRenderTargets.emplace_back( "HelloTexture_ColorTarget" );
        pipelineDesc.DepthRenderTargets = "HelloTexture_DepthTarget";

        builder.CreateNamedPipeline( "HelloTexture_Pipeline", pipelineDesc );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( sizeof( HelloTextureUniformBuffer ) );
        builder.CreateNamedBuffer( "HelloTexture_TexturesBuffer", lightsBuffer );

        builder.CreateColorRenderTarget( "HelloTexture_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );
        builder.CreateDepthRenderTarget( "HelloTexture_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

        builder.WriteTexture( this, "HelloTexture_ColorTarget" );
        builder.WriteTexture( this, "HelloTexture_DepthTarget" );
    }

    auto HelloTexture::Execute( PassCommandList& commandList ) -> void {

        commandList.SetColorRenderTarget( "HelloTexture_ColorTarget" );
        commandList.SetDepthRenderTarget( "HelloTexture_DepthTarget" );
        commandList.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );

        commandList.BeginRender(this);
        commandList.BindPipeline( "HelloTexture_Pipeline" );

        TextureHandle textureHandle{ AssetsService::Get()->LoadAsset<Texture>( Path{ "Resources/Models/1 - Box texture/CatStare.png" } ) };

        static bool first{ true };
        if (first) {
            Int32 srgTextureIndex{ commandList.PushTexture( textureHandle ) };

            HelloTextureUniformBuffer uboData{};
            uboData.TextureIndex = srgTextureIndex;

            commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "HelloTexture_TexturesBuffer", 0 );
            commandList.FillBuffer( "HelloTexture_TexturesBuffer", std::addressof( uboData ), sizeof( HelloTextureUniformBuffer ));

            first = false;
        }

        commandList.BindResourceGroup(SRGType::SRG_Textures);
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.Draw( 4, 1, 0, 0 );

        commandList.EndRender();
    }
}