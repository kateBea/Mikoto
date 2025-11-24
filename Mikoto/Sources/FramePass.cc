//
// Created by kate on 11/24/25.
//

#include <Material/ShaderLibrary.hh>
#include <Material/ShaderModule.hh>
#include <Renderer/Core/FramePass.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace  Mikoto {

    auto FinalCompositionPass::Setup( GpuDevice *device ) -> void {
        m_Device = device;

        // Color attachment
        TextureDescription colorDesc{};
        colorDesc.WithWidth( 1920 )
            .WithHeight( 1080 )
            .WithChannelCount( 4 )
            .WithData( nullptr )
            .WithType( TextureType::TEXTURE_2D )
            .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
            .WithFormat( TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM )
            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_ColorTarget = m_Device->CreateTexture( colorDesc );
        m_ColorTarget->SetDebugName( "FinalCompositionPass Color Target" );

        // Depth attachment
        TextureDescription depthDesc{};
        depthDesc.WithWidth( 1920 )
            .WithHeight( 1080 )
            .WithChannelCount( 1 )
            .WithData( nullptr )
            .WithType( TextureType::TEXTURE_2D )
            .WithTextureUsage( TextureUsage::TEXTURE_USAGE_DEPTH )
            .WithFormat( TextureFormat::TEXTURE_FORMAT_D32_FLOAT )
            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_DepthTarget = m_Device->CreateTexture( depthDesc );
        m_DepthTarget->SetDebugName( "FinalCompositionPass Depth Target" );

        // Graphics pipeline
        ShaderModuleHandle vertShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle fragShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { vertShader, fragShader };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.DepthTexture = m_DepthTarget;
        pipelineDesc.ColorAttachments = { m_ColorTarget };

        m_Pipeline = m_Device->CreatePipeline( pipelineDesc );
    }

    auto FinalCompositionPass::RegisterInput( ResourceHandle resource ) -> void {
        m_Inputs.emplace_back( resource );

    }

    auto FinalCompositionPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto FinalCompositionPass::Execute(GraphicsContext& context) -> void {

        // Set render targets
        context.SetRenderTarget(m_ColorTarget, m_DepthTarget);
        context.SetViewport(0, 0, 1920, 1080);
        context.ClearColor(m_ColorTarget, m_ClearColor);
        context.ClearDepth(m_DepthTarget, 1.0f);

        context.BeginRender();

        // Bind PBR pipeline
        context.BindPipeline(m_Pipeline);

        // Build batches per mesh
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComp { registry.get<MeshComponent>( entity ) };
            auto& materialComp { registry.get<MaterialComponent>( entity ) };

            if (tag.IsActive() && meshComp.HasMesh() && materialComp.HasMaterial()) {
                context.Draw( meshComp.GetMesh(), materialComp.GetMaterial(), transform.GetTransform() );
            }
        }

        auto lights{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        for ( auto& entity: lights ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& lightComp { registry.get<LightComponent>( entity ) };

            if (tag.IsActive()) {
                if (lightComp.IsTypeActive( LightType::POINT_LIGHT_TYPE )) {
                    context.RegisterLight( std::addressof( lightComp.Get<PointLight>() ) );
                }
            }
        }

        context.EndRender();

    }

    auto ShadowPass::Setup( GpuDevice *device ) -> void {
        m_Device = device;

        // Color attachment
        TextureDescription colorDesc{};
        colorDesc.WithWidth( 1920 )
            .WithHeight( 1080 )
            .WithChannelCount( 4 )
            .WithData( nullptr )
            .WithType( TextureType::TEXTURE_2D )
            .WithTextureUsage( TextureUsage::TEXTURE_USAGE_COLOR )
            .WithFormat( TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM )
            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_ColorTarget = m_Device->CreateTexture( colorDesc );
        m_ColorTarget->SetDebugName( "FinalCompositionPass Color Target" );

        // Depth attachment
        TextureDescription depthDesc{};
        depthDesc.WithWidth( 1920 )
            .WithHeight( 1080 )
            .WithChannelCount( 1 )
            .WithData( nullptr )
            .WithType( TextureType::TEXTURE_2D )
            .WithTextureUsage( TextureUsage::TEXTURE_USAGE_DEPTH )
            .WithFormat( TextureFormat::TEXTURE_FORMAT_D32_FLOAT )
            .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        m_DepthTarget = m_Device->CreateTexture( depthDesc );
        m_DepthTarget->SetDebugName( "FinalCompositionPass Depth Target" );

        // Graphics pipeline
        ShaderModuleHandle vertShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/Shadows.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle fragShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/Shadows.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { vertShader, fragShader };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.DepthTexture = m_DepthTarget;
        pipelineDesc.ColorAttachments = { m_ColorTarget };

        m_Pipeline = m_Device->CreatePipeline( pipelineDesc );
    }

    auto ShadowPass::Execute( GraphicsContext &context ) -> void {
        // Set render targets
        context.SetRenderTarget(m_ColorTarget, m_DepthTarget);
        context.SetViewport(0, 0, 1920, 1080);
        context.ClearColor(m_ColorTarget, m_ClearColor);
        context.ClearDepth(m_DepthTarget, 1.0f);

        context.BeginRender();

        // Bind Shadow pipeline
        context.BindPipeline(m_Pipeline);

        // Build batches per mesh
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComp { registry.get<MeshComponent>( entity ) };
            auto& materialComp { registry.get<MaterialComponent>( entity ) };

            if (tag.IsActive() && meshComp.HasMesh() && materialComp.HasMaterial()) {
                context.Draw( meshComp.GetMesh(), materialComp.GetMaterial(), transform.GetTransform() );
            }
        }

        auto lights{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        for ( auto& entity: lights ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& lightComp { registry.get<LightComponent>( entity ) };

            if (tag.IsActive()) {
                if (lightComp.IsTypeActive( LightType::POINT_LIGHT_TYPE )) {
                    context.RegisterLight( std::addressof( lightComp.Get<PointLight>() ) );
                }
            }
        }

        context.EndRender();
    }

    auto ShadowPass::RegisterInput( ResourceHandle resource ) -> void {
        m_Inputs.push_back( resource );
    }

    auto ShadowPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto TextPass::Setup( GpuDevice *device ) -> void {
        m_Device = device;

        // This pass takes its output depth
        // and color image from the final composition

        // Graphics pipeline
        ShaderModuleHandle vertShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/Text.sprv", ShaderStage::VERTEX_STAGE ) };
        ShaderModuleHandle fragShader{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/Text.sprv", ShaderStage::FRAGMENT_STAGE ) };

        GraphicsPipelineDescription pipelineDesc{};
        pipelineDesc.ShaderStages = { vertShader, fragShader };
        pipelineDesc.DepthTest = true;
        pipelineDesc.DepthWrite = true;
        pipelineDesc.AlphaBlending = true;
        pipelineDesc.DepthTexture = m_DepthTarget;
        pipelineDesc.ColorAttachments = { m_ColorTarget };

        m_Pipeline = m_Device->CreatePipeline( pipelineDesc );
    }

    auto TextPass::Execute( GraphicsContext &context ) -> void {

    }

    auto TextPass::RegisterInput( ResourceHandle resource ) -> void {
        m_Inputs.emplace_back( resource );
    }

    auto SimpleComputePass::Setup( GpuDevice *device ) -> void {
        m_Device = device;

        // Create small storage buffer
        const Size totalSize{ sizeof( UInt32 ) * m_Limit };
        BufferDescription desc{};
        desc.WithSizeBytes( totalSize )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

        m_StorageBuffer = m_Device->CreateBuffer( desc );
        m_StorageBuffer->SetDebugName( "ComputeBasic SSBO" );

        // Pipeline setup
        ShaderModuleHandle compModule{ ShaderLibrary::Get()->LoadShader("./Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE_STAGE ) };
        ComputePipelineDescription description{
            .Stage{ compModule }
        };

        m_Pipeline = m_Device->CreatePipeline( description );
    }

    auto SimpleComputePass::Execute( GraphicsContext &context ) -> void {
        context.BindPipeline( m_Pipeline );

        context.BindBuffer( m_StorageBuffer );

        context.Dispatch();
    }

    auto SimpleComputePass::RegisterInput( ResourceHandle resource ) -> void {
        m_Inputs.emplace_back( resource );
    }
}// namespace Mikoto
