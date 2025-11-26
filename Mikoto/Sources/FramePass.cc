//
// Created by kate on 11/24/25.
//

#include <Material/ShaderLibrary.hh>
#include <Material/ShaderModule.hh>
#include <Renderer/Core/FramePass.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace  Mikoto {

    auto FramePass::RegisterInput( std::string_view name ) -> void {
        m_Inputs.emplace_back( name );
    }

    auto FramePass::RegisterOutput( std::string_view name ) -> void {
        m_Outputs.emplace_back( name );
    }

    auto FramePass::RegisterResource( std::string_view name ) -> void {
        m_Resources.emplace_back( name );
    }

    auto FinalCompositionPass::Setup( GraphicsContext* context ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv" );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv" );

        context->CreateNamedPipeline( "FinalCompositionPass_Pipeline", pipelineDesc, PipelineType::GRAPHICS_PIPELINE );

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

        context->CreateNamedRenderTarget( "FinalCompositionPass_ColorTarget", colorDesc, RenderTargetType::COLOR );

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

        context->CreateNamedRenderTarget( "FinalCompositionPass_DepthTarget", depthDesc, RenderTargetType::DEPTH );

        // Declare its inputs and outputs
        RegisterInput( "ShadowPass_ColorTarget" );
        RegisterInput( "ShadowPass_LightsBuffer" );
        RegisterInput( "ShadowPass_ObjectInfo" );

        RegisterOutput( "FinalCompositionPass_ColorTarget" );
        RegisterOutput( "FinalCompositionPass_DepthTarget" );

        RegisterResource( "FinalCompositionPass_ColorTarget" );
        RegisterResource( "FinalCompositionPass_DepthTarget" );
    }

    auto FinalCompositionPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto FinalCompositionPass::Execute(PassCommandList& commandList) -> void {
        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "FinalCompositionPass_DepthTarget" );

        commandList.BeginRender();

        // Set render targets
        commandList.SetViewport(0, 0, 1920, 1080);
        commandList.SetScissor(0, 0, 1920, 1080);

        commandList.BindPipeline( "FinalCompositionPass_Pipeline" );

        commandList.BindBuffer( "ShadowPass_CameraInfo", 0, 0 );
        commandList.BindBuffer( "ShadowPass_LightsBuffer", 1, 0 );
        commandList.BindBuffer( "ShadowPass_ObjectInfo", 2, 0 );

        // Meshes
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComponent { registry.get<MeshComponent>( entity ) };
            auto& materialComp { registry.get<MaterialComponent>( entity ) };

            if (tag.IsActive() && meshComponent.HasMesh()) {
                MeshNode* mesh{ meshComponent.GetMesh() };

                // bind the textures etc

                commandList.BindVertexBuffer(mesh->GetVertexBuffer());
                commandList.BindIndexBuffer(mesh->GetIndexBuffer());

                commandList.SubmitDraw();
            }

        }

        // Lights
        auto lights{ registry.view<TagComponent, TransformComponent, LightComponent>() };
        for ( auto& entity: lights ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& lightComp { registry.get<LightComponent>( entity ) };

        }

        commandList.EndRender();

    }

    auto ShadowPass:: Setup( GraphicsContext* context ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Vert.sprv" );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Frag.sprv" );

        context->CreateNamedPipeline( "ShadowPass_Pipeline", pipelineDesc, PipelineType::GRAPHICS_PIPELINE );

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

        context->CreateNamedRenderTarget( "ShadowPass_ColorTarget", colorDesc, RenderTargetType::COLOR );

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

        context->CreateNamedRenderTarget( "ShadowPass_DepthTarget", depthDesc, RenderTargetType::DEPTH );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 0 ); // TODO
        context->CreateNamedBuffer( "ShadowPass_LightsBuffer", lightsBuffer );

        // Transform, texture indices, etc
        BufferDescription objectsInfo{};
        objectsInfo.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 0 ); // TODO
        context->CreateNamedBuffer( "ShadowPass_ObjectInfo", objectsInfo );

        // Camera
        BufferDescription camera{};
        objectsInfo.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 0 ); // TODO
        context->CreateNamedBuffer( "ShadowPass_CameraInfo", camera );

        // Declare its inputs and outputs
        RegisterOutput( "ShadowPass_ColorTarget" );
        RegisterOutput( "ShadowPass_DepthTarget" );
        RegisterOutput( "ShadowPass_ObjectInfo" );
        RegisterOutput( "ShadowPass_CameraInfo" );

        RegisterResource( "ShadowPass_ColorTarget" );
        RegisterResource( "ShadowPass_LightsBuffer" );
        RegisterResource( "ShadowPass_ObjectInfo" );
        RegisterResource( "ShadowPass_CameraInfo" );
    }

    auto ShadowPass::Execute( PassCommandList& commandList ) -> void {

        commandList.SetColorRenderTarget( "ShadowPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "ShadowPass_LightsBuffer" );

        commandList.BeginRender();

        commandList.BindBuffer( "ShadowPass_CameraInfo", 0, 0 );
        commandList.BindBuffer( "ShadowPass_LightsBuffer", 1, 0 );
        commandList.BindBuffer( "ShadowPass_ObjectInfo", 2, 0 );

        // Set render targets
        commandList.SetViewport(0, 0, 1920, 1080);
        commandList.SetScissor(0, 0, 1920, 1080);

        commandList.BindPipeline( "ShadowPass_Pipeline" );

        // Meshes
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComponent { registry.get<MeshComponent>( entity ) };
            auto& materialComp { registry.get<MaterialComponent>( entity ) };

            if (tag.IsActive() && meshComponent.HasMesh()) {
                MeshNode* mesh{ meshComponent.GetMesh() };

                commandList.BindVertexBuffer(mesh->GetVertexBuffer());
                commandList.BindIndexBuffer(mesh->GetIndexBuffer());

                commandList.SubmitDraw();
            }
        }

        // Lights
        auto lights{ registry.view<TagComponent, TransformComponent, LightComponent>() };
        for ( auto& entity: lights ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& lightComp { registry.get<LightComponent>( entity ) };

        }

        commandList.EndRender();
    }

    auto ShadowPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto TextPass::Setup( GraphicsContext* context ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/MSDFText_Vert.sprv" );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/MSDFText_Frag.sprv" );

        context->CreateNamedPipeline( "TextPass_Pipeline", pipelineDesc, PipelineType::GRAPHICS_PIPELINE );

        RegisterInput( "FinalCompositionPass_ColorTarget" );
        RegisterInput( "FinalCompositionPass_DepthTarget" );

        RegisterOutput( "FinalCompositionPass_ColorTarget" );
    }

    auto TextPass::Execute( PassCommandList& commandList ) -> void {
        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "FinalCompositionPass_DepthTarget" );

        commandList.BeginRender();

        // Set render targets
        commandList.SetViewport(0, 0, 1920, 1080);
        commandList.SetScissor(0, 0, 1920, 1080);

        commandList.BindPipeline( "ShadowPass_Pipeline" );

        // Meshes
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, TextComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& textComponent { registry.get<TextComponent>( entity ) };
            auto& materialComp { registry.get<MaterialComponent>( entity ) };
        }

        commandList.EndRender();
    }

    auto TextPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto SimpleComputePass::Setup( GraphicsContext* context ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv" );

        context->CreateNamedPipeline( "SimpleComputePass_Pipeline", pipelineDesc, PipelineType::COMPUTE_PIPELINE );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 30 * sizeof(UInt32) ); // TODO
        context->CreateNamedBuffer( "SimpleComputePass_Result", lightsBuffer );

        RegisterResource( "SimpleComputePass_Result" );
        RegisterOutput( "SimpleComputePass_Result" );
    }

    auto SimpleComputePass::Execute( PassCommandList& commandList ) -> void {
        commandList.BeginCompute();

        // Prime numbers up until this value
        constexpr  UInt32 limitNumbers{ 30 };

        // matches shader's local_size_x
        constexpr UInt32 localSize{ 64 };
        constexpr UInt32 groupCount{ (limitNumbers + localSize - 1) / localSize };

        commandList.Dispatch( groupCount, 1, 1 );

        commandList.EndCompute();

    }
}// namespace Mikoto
