//
// Created by zanet on 1/5/2026.
//

#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Passes/DebugPasses.hh>

namespace Mikoto {

    auto ObjectOutlinePass::Setup( FrameGraphBuilder& builder ) -> void {

    }

    auto ObjectOutlinePass::Execute( PassCommandList& cmdList ) -> void {
    }

    auto WireFramePass::Setup( FrameGraphBuilder& builder ) -> void {
        PipelineDescription builderPipelineDesc{};

        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Wireframe_Vert.sprv", ShaderStage::VERTEX_STAGE );
        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Wireframe_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest = true;
        graphicsDesc.DepthWrite = true;
        graphicsDesc.AlphaBlending = true;
        graphicsDesc.Wireframe = true;

        // Graphics context will specify the texture formats for the render targets we can redner to with this pipeline
        // It will also create the shader modules first and assign them to this description which will be used to create the actual pipeline
        // TODO: temporary, specify the render targets this pipeline outputs to
        builderPipelineDesc.ColorRenderTargets.emplace_back( "WireFramePass_ColorTarget" );
        builderPipelineDesc.DepthRenderTargets = "WireFramePass_DepthTarget";
        builderPipelineDesc.Description = graphicsDesc;

        builder.CreateNamedPipeline( "WireFramePass_Pipeline", builderPipelineDesc );

        builder.CreateColorRenderTarget( "WireFramePass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );
        builder.CreateDepthRenderTarget( "WireFramePass_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

        // Declare its inputs and outputs
        builder.ReadBuffer( this, "PerFrame_CameraInfo" );
        builder.ReadBuffer( this, "FinalCompositionPass_MeshInfo" );

        builder.WriteTexture( this, "WireFramePass_ColorTarget" );
        builder.WriteTexture( this, "WireFramePass_DepthTarget" );

        // Prepare to have at least MAX_RENDERABLE_ENTITIES
        m_Meshes.resize( MAX_RENDERABLE_ENTITIES );
    }

    auto WireFramePass::TraverseMeshList(PassCommandList& commandList) -> void {
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComponent{ registry.get<MeshComponent>( entity ) };
            auto& materialComp{ registry.get<MaterialComponent>( entity ) };

            MaterialHandle material{ materialComp.GetMaterial() };

            if ( meshComponent.HasMesh() && !material.IsEmpty() ) {
                MeshNode* meshNode{ meshComponent.GetMesh() };
                PBRMaterial* pbrMat{ dynamic_cast<PBRMaterial*>( material.GetRaw() ) };

                auto& [DrawIndexedState, ActiveEntities, Instances] {
                    m_MeshDrawState[meshNode]
                };

                ActiveEntities[tag.GetGUID()] = tag.IsActive();

                if (tag.IsActive() ) {
                    ShaderMaterialParams& ubo{ Instances[tag.GetGUID()] };

                    ubo.Transform = transform.GetTransform();

                    ubo.Albedo = pbrMat->GetColor();
                    ubo.Factors.x = pbrMat->GetMetallicFactor();
                    ubo.Factors.y = pbrMat->GetRoughnessFactor();

                    ubo.AlbedoIndex = commandList.PushTexture( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                    ubo.NormalIndex = commandList.PushTexture( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
                    ubo.MetallicIndex = commandList.PushTexture( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
                    ubo.RoughnessIndex = commandList.PushTexture( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                    ubo.AoIndex = commandList.PushTexture( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                }
            }
        }
    }

    auto WireFramePass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto WireFramePass::SetClearColor( const Vec4F& vec ) -> void {
        m_ClearColor = vec;
    }

    auto WireFramePass::DrawObjects(PassCommandList& commandList) -> void {
        Size meshIndex{};
        Size firstInstance{};

        for ( auto& [meshNode, instanceInfo]: m_MeshDrawState ) {

            DrawIndexedState& drawState{ instanceInfo.InstanceDrawState };

            drawState.IndexBuffer = meshNode->GetIndexBuffer();

            if (drawState.VertexBuffers.empty()) {
                drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0);
            }

            Size drawCount{};
            for (const auto& [entityID, meshInstanceInfo]: instanceInfo.InstanceInfos) {
                if (instanceInfo.IsActive( entityID )) {
                    m_Meshes[meshIndex++] = meshInstanceInfo;
                    ++drawCount;
                }
            }

            drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();
            drawState.FirstInstance = firstInstance;
            drawState.InstancesCount = drawCount;

            firstInstance += drawCount;

            commandList.DrawIndexed(drawState);
        }
    }

    auto WireFramePass::Execute( PassCommandList& commandList ) -> void {
        commandList.SetColorRenderTarget( "WireFramePass_ColorTarget" );
        commandList.SetDepthRenderTarget( "WireFramePass_DepthTarget" );

        commandList.SetClearColor( m_ClearColor );

        commandList.BeginRender(this);
        commandList.BindPipeline( "WireFramePass_Pipeline" );

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "PerFrame_CameraInfo", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "FinalCompositionPass_MeshInfo", 1 );

        commandList.BindResourceGroup(SRGType::SRG_Textures);
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        TraverseMeshList(commandList);

        DrawObjects(commandList);

        commandList.EndRender();
    }

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