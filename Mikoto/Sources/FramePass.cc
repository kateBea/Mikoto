//
// Created by kate on 11/24/25.
//

#include <Material/ShaderLibrary.hh>
#include <Material/ShaderModule.hh>
#include <Renderer/Core/FramePass.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace  Mikoto {

    auto FinalCompositionPass::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription builderPipelineDesc{};

        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX_STAGE );
        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest = true;
        graphicsDesc.DepthWrite = true;
        graphicsDesc.AlphaBlending = true;

        // Graphics context will specify the texture formats for the render targets we can redner to with this pipeline
        // It will also create the shader modules first and assign them to this description which will be used to create the actual pipeline

        // Input rate
        // Vertices
        AttributesSpec verticesData{
            .DefaultVertexLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT },
            .InputRateSpec{ .BindingIndex{ 0 }, .AttributeRate{ InputRate::PER_VERTEX } }
        };

        // Attributes
        AttributesSpec instancedData{
            .DefaultVertexLayout{
                // Model matrix columns
                        { ShaderDataType::FLOAT4_TYPE, "i_Model0" },// mat4 column 0
                        { ShaderDataType::FLOAT4_TYPE, "i_Model1" },// mat4 column 1
                        { ShaderDataType::FLOAT4_TYPE, "i_Model2" },// mat4 column 2
                        { ShaderDataType::FLOAT4_TYPE, "i_Model3" },// mat4 column 3

                        // Material properties
                        { ShaderDataType::FLOAT4_TYPE, "i_Albedo" },
                        { ShaderDataType::FLOAT4_TYPE, "i_Factors" },

                        // Texture indices (flat ints)
                        { ShaderDataType::INT_TYPE, "i_AlbedoIndex" },
                        { ShaderDataType::INT_TYPE, "i_NormalIndex" },
                        { ShaderDataType::INT_TYPE, "i_MetallicIndex" },
                        { ShaderDataType::INT_TYPE, "i_RoughnessIndex" },
                        { ShaderDataType::INT_TYPE, "i_AoIndex" } },
                    .InputRateSpec{ .BindingIndex{ 1 }, .AttributeRate{ InputRate::PER_INSTANCE } }
        };

        graphicsDesc.VertexAttributesSpec = { verticesData, instancedData };

        builderPipelineDesc.Description = graphicsDesc;

        builder.RegisterPipeline( "FinalCompositionPass_Pipeline", builderPipelineDesc );

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

        builder.CreateNamedRenderTarget( "FinalCompositionPass_ColorTarget", colorDesc );

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

        builder.CreateNamedRenderTarget( "FinalCompositionPass_DepthTarget", depthDesc );

        // Declare its inputs and outputs
        builder.RegisterInput( this, "ShadowPass_ColorTarget" );
        builder.RegisterInput( this, "ShadowPass_LightsBuffer" );
        builder.RegisterInput( this, "ShadowPass_ObjectInfo" );

        builder.WriteTexture( this, "FinalCompositionPass_ColorTarget" );
        builder.WriteTexture( this, "FinalCompositionPass_DepthTarget" );
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

            MaterialHandle material { materialComp.GetMaterial() };

            if (tag.IsActive() && meshComponent.HasMesh() && !material.IsEmpty()) {
                MeshNode* mesh{ meshComponent.GetMesh() };
                PBRMaterial* matPtr{ dynamic_cast<PBRMaterial*>( material.GetRaw() ) };

                commandList.BindTexture( matPtr->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::NORMAL_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::METALLIC_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::EMISSIVE_TEXTURE ) );

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

    auto ShadowPass:: Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Frag.sprv", ShaderStage::FRAGMENT_STAGE);

        // Configure pipeline stage
        GraphicsPipelineDescription graphicseDesc{};

        pipelineDesc.Description = graphicseDesc;

        builder.RegisterPipeline( "ShadowPass_Pipeline", pipelineDesc );

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

        builder.CreateNamedRenderTarget( "ShadowPass_ColorTarget", colorDesc );

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

        builder.CreateNamedRenderTarget( "ShadowPass_DepthTarget", depthDesc );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 0 ); // TODO
        builder.CreateNamedBuffer( "ShadowPass_LightsBuffer", lightsBuffer );

        // Transform, texture indices, etc
        BufferDescription objectsInfo{};
        objectsInfo.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 0 ); // TODO
        builder.CreateNamedBuffer( "ShadowPass_ObjectInfo", objectsInfo );

        // Camera
        BufferDescription camera{};
        objectsInfo.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 0 ); // TODO
        builder.CreateNamedBuffer( "ShadowPass_CameraInfo", camera );

        // Declare its inputs and outputs
        builder.WriteTexture( this, "ShadowPass_ColorTarget" );
        builder.WriteTexture( this, "ShadowPass_DepthTarget" );
        builder.WriteTexture( this, "ShadowPass_ObjectInfo" );
        builder.WriteTexture( this, "ShadowPass_CameraInfo" );
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

            MaterialHandle material{ materialComp.GetMaterial() };

            if (tag.IsActive() && meshComponent.HasMesh() && !material.IsEmpty()) {
                PBRMaterial* matPtr{ dynamic_cast<PBRMaterial*>( material.GetRaw() ) };

                MeshNode* mesh{ meshComponent.GetMesh() };

                commandList.BindTexture( matPtr->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::NORMAL_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::METALLIC_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                commandList.BindTexture( matPtr->GetTextureType( MapType::EMISSIVE_TEXTURE ) );

                commandList.BindVertexBuffer(mesh->GetVertexBuffer());
                commandList.BindIndexBuffer(mesh->GetIndexBuffer());

                // We probably do not need the transform here the shadow pass should happen before
                // this pass which creates and update the buffer that has the contents to render out geometry

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

    auto TextPass::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        // Configure pipeline stage
        GraphicsPipelineDescription graphicseDesc{};

        pipelineDesc.Description = graphicseDesc;

        builder.RegisterPipeline( "TextPass_Pipeline", pipelineDesc );

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/MSDFText_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/MSDFText_Frag.sprv", ShaderStage::FRAGMENT_STAGE);

        builder.RegisterPipeline( "TextPass_Pipeline", pipelineDesc );

        builder.RegisterInput( this, "FinalCompositionPass_ColorTarget" );
        builder.RegisterInput( this, "FinalCompositionPass_DepthTarget" );

        builder.WriteTexture( this, "FinalCompositionPass_ColorTarget" );
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

    auto HelloCubePass::Setup( FrameGraphBuilder& device ) -> void {
    }

    auto HelloCubePass::Execute( PassCommandList& cmdList ) -> void {
    }

    auto SimpleComputePass::Setup( FrameGraphBuilder& builder ) -> void {
        PipelineDescription pipelineDesc{};
        pipelineDesc.Description = ComputePipelineDescription{};

        // Configure shader stages
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE_STAGE );

        builder.CreateNamedPipeline( this, "SimpleComputePass_Pipeline", pipelineDesc );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
            .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
            .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
            .WithSizeBytes( 30 * sizeof(float) );
        builder.CreateNamedBuffer( "SimpleComputePass_Result", lightsBuffer );

        builder.WriteBuffer( this, "SimpleComputePass_Result" );
    }

    auto SimpleComputePass::Execute( PassCommandList& commandList ) -> void {
        commandList.BeginCompute();

        commandList.BindPipeline( "SimpleComputePass_Pipeline" );

        // Prime numbers up until this value
        constexpr  UInt32 limitNumbers{ 30 };

        // matches shader's local_size_x
        constexpr UInt32 localSize{ 64 };
        constexpr UInt32 groupCount{ (limitNumbers + localSize - 1) / localSize };

        commandList.BindBuffer( "SimpleComputePass_Result", 0, 0 );

        commandList.Dispatch( groupCount, 1, 1 );

        commandList.EndCompute();

    }

    auto HelloTrianglePass::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/HelloTriangle_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/HelloTriangle_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest = true;
        graphicsDesc.DepthWrite = true;
        graphicsDesc.AlphaBlending = true;
        graphicsDesc.VertexAttributesSpec = {};

        // Graphics context will specify the texture formats for the render targets we can redner to with this pipeline
        // It will also create the shader modules first and assign them to this description which will be used to create the actual pipeline

        pipelineDesc.Description = graphicsDesc;

        // TODO: temporary, specify the render targets this pipeline outputs to
        pipelineDesc.ColorRenderTargets.emplace_back( "HelloTrianglePass_ColorTarget" );
        pipelineDesc.DepthRenderTargets = "HelloTrianglePass_DepthTarget";

        builder.CreateNamedPipeline( this, "HelloTrianglePass_Pipeline", pipelineDesc );

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

        builder.CreateNamedRenderTarget( "HelloTrianglePass_ColorTarget", colorDesc );

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

        builder.CreateNamedRenderTarget( "HelloTrianglePass_DepthTarget", depthDesc );

        builder.WriteTexture( this, "HelloTrianglePass_ColorTarget" );
        builder.WriteTexture( this, "HelloTrianglePass_DepthTarget" );
    }

    auto HelloTrianglePass::Execute( PassCommandList& commandList ) -> void {

        commandList.SetColorRenderTarget( "HelloTrianglePass_ColorTarget" );
        commandList.SetDepthRenderTarget( "HelloTrianglePass_DepthTarget" );
        commandList.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );

        commandList.BeginRender();

        // Set render targets
        commandList.SetViewport(0, 0, 1920, 1080);
        commandList.SetScissor(0, 0, 1920, 1080);

        commandList.BindPipeline( "HelloTrianglePass_Pipeline" );

        commandList.Draw(3, 1, 0, 0);

        commandList.EndRender();
    }
}// namespace Mikoto
