//
// Created by kate on 1/12/26.
//

#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>
#include <Renderer/Passes/IBLPasses.hh>

namespace Mikoto {

    auto EnvCubePass::Setup( FrameGraphBuilder &builder ) -> void {
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader("./Resources/Shaders/vulkan-spirv/EnvironmentMap_Vert.sprv", ShaderStage::VERTEX_STAGE);
        pipelineDesc.AddShader("./Resources/Shaders/vulkan-spirv/EnvironmentMap_Frag.sprv", ShaderStage::FRAGMENT_STAGE);

        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest  = false;
        graphicsDesc.DepthWrite = false;
        graphicsDesc.AlphaBlending = false;
        graphicsDesc.PipelineCullMode = CullMode::NONE;
        graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;

        graphicsDesc.VertexAttributesSpec = {};

        pipelineDesc.Description = graphicsDesc;
        pipelineDesc.ColorRenderTargets.emplace_back("EnvCubePass_ColorTarget");

        builder.CreateNamedPipeline("EnvCubePass_Pipeline", pipelineDesc);

        // CUbemap render target
        builder.CreateColorRenderTarget( "EnvCubePass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );

        builder.WriteTexture(this, "EnvCubePass_ColorTarget");
    }

    auto EnvCubePass::Execute( PassCommandList &commandList ) -> void {
        if (m_Hdr.IsEmpty()) {
            return;
        }

        commandList.SetColorRenderTarget("EnvCubePass_ColorTarget");

        commandList.BeginRender(this);
        commandList.BindPipeline("EnvCubePass_Pipeline");

        commandList.SetViewport(0, 0, 1920, 1080);
        commandList.SetScissor(0, 0, 1920, 1080);

        m_UBO.HDRTextureIndex = commandList.PushTexture( m_Hdr );

        commandList.FillBuffer( "EnvCubePass_Uniform", std::addressof( m_UBO ), sizeof(m_UBO) );

        commandList.BindResourceGroup(SRGType::SRG_PerPass);
        commandList.BindResourceGroup(SRGType::SRG_Textures);

        commandList.Draw(36, 1, 0, 0 );

        commandList.EndRender();
    }

    auto EnvCubePass::SetTextureHDR( TextureHandle hdr ) -> void {
        if (!hdr.IsEmpty()) {
            m_Hdr = hdr;

            MarkDirty();
        }
    }

    auto IrradiancePass::Setup( FrameGraphBuilder &builder ) -> void {

    }

    auto IrradiancePass::Execute( PassCommandList &commandList ) -> void {

    }

    auto PrefilterPass::Setup( FrameGraphBuilder &builder ) -> void {

    }

    auto PrefilterPass::Execute( PassCommandList &commandList ) -> void {

    }

    auto BRDFLutPass::Setup( FrameGraphBuilder &builder ) -> void {

    }

    auto BRDFLutPass::Execute( PassCommandList &commandList ) -> void {

    }

    auto SkyboxPass::Setup( FrameGraphBuilder &builder ) -> void {
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader("./Resources/Shaders/vulkan-spirv/Skybox_Vert.sprv", ShaderStage::VERTEX_STAGE);
        pipelineDesc.AddShader("./Resources/Shaders/vulkan-spirv/Skybox_Frag.sprv", ShaderStage::FRAGMENT_STAGE);

        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest  = false;
        graphicsDesc.DepthWrite = false;
        graphicsDesc.AlphaBlending = false;
        graphicsDesc.PipelineCullMode = CullMode::NONE;
        graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;

        graphicsDesc.VertexAttributesSpec = {};

        pipelineDesc.Description = graphicsDesc;
        pipelineDesc.ColorRenderTargets.emplace_back("FinalCompositionPass_ColorTarget");

        builder.CreateNamedPipeline("SkyboxPass_Pipeline", pipelineDesc);

        BufferDescription cameraInfo{};
        cameraInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof(SkyboxUBO), 1 );
        builder.CreateNamedBuffer( "SkyboxPass_CameraInfo", cameraInfo );

        builder.WriteBuffer(this, "SkyboxPass_CameraInfo");

        // For now will use its own render target but should use final composition one
        builder.WriteTexture(this, "FinalCompositionPass_ColorTarget");
    }

    auto SkyboxPass::Execute( PassCommandList &commandList ) -> void {

        if (m_CubeMap.IsEmpty()) {
            return;
        }

        SamplerDescription samplerDescription{ .CubeSampler{ true } };
        commandList.CreateNamedSampler("SkyboxPass_Sampler", samplerDescription);
        commandList.RegisterNamedTexture("SkyboxPass_TextureCube", m_CubeMap);

        commandList.SetColorRenderTarget("FinalCompositionPass_ColorTarget");

        commandList.BeginRender(this, LoadOp::CLEAR);
        commandList.BindPipeline("SkyboxPass_Pipeline");

        commandList.SetViewport(0, 0, 1920, 1080);
        commandList.SetScissor(0, 0, 1920, 1080);

        commandList.SetBufferBindSlot(SRGType::SRG_PerPass,"SkyboxPass_CameraInfo", 0);
        commandList.SetTextureBindSlot(SRGType::SRG_PerPass, "SkyboxPass_TextureCube", "SkyboxPass_Sampler", 1);

        commandList.FillBuffer( "SkyboxPass_CameraInfo", std::addressof( m_SkyboxUBO ), sizeof(SkyboxUBO) );

        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        commandList.Draw(36, 1, 0, 0 );

        commandList.EndRender();
    }

    auto SkyboxPass::SetCamera( const Camera *camera ) -> void {
        m_SkyboxUBO.Projection = camera->GetProjection();
        m_SkyboxUBO.View = camera->GetViewMatrix();
    }

    auto SkyboxPass::SetCubeMap( TextureHandle cubeMap ) -> void {
        m_CubeMap = cubeMap;
    }

    auto ShadingPass::Setup( FrameGraphBuilder& builder ) -> void {
        PipelineDescription builderPipelineDesc{};

        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX_STAGE );
        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest = true;
        graphicsDesc.DepthWrite = true;
        graphicsDesc.AlphaBlending = true;
        graphicsDesc.PipelineCullMode = CullMode::CULL_BACK;

        // Graphics context will specify the texture formats for the render targets we can redner to with this pipeline
        // It will also create the shader modules first and assign them to this description which will be used to create the actual pipeline
        // TODO: temporary, specify the render targets this pipeline outputs to
        builderPipelineDesc.ColorRenderTargets.emplace_back( "FinalCompositionPass_ColorTarget" );
        builderPipelineDesc.DepthRenderTargets = "FinalCompositionPass_DepthTarget";
        builderPipelineDesc.Description = graphicsDesc;

        builder.CreateNamedPipeline( "FinalCompositionPass_Pipeline", builderPipelineDesc );

        builder.CreateColorRenderTarget( "FinalCompositionPass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );
        builder.CreateDepthRenderTarget( "FinalCompositionPass_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

        BufferDescription cameraInfo{};
        cameraInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof(ShaderCameraParams), 1 );
        builder.CreateNamedBuffer( "PerFrame_CameraInfo", cameraInfo );

        BufferDescription meshInstanceInfo{};
        meshInstanceInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof(ShaderMaterialParams), MAX_RENDERABLE_ENTITIES );
        builder.CreateNamedBuffer( "FinalCompositionPass_MeshInfo", meshInstanceInfo );

        // Declare its inputs and outputs
        builder.ReadBuffer( this, "AABBGenComp_CameraUBO" );
        builder.ReadBuffer( this, "AABBGenComp_Clusters" );
        builder.ReadBuffer( this, "LightCullingComp_LightsBuffer" );

        builder.ReadBuffer( this, "PerFrame_CameraInfo" );

        builder.WriteBuffer( this, "FinalCompositionPass_MeshInfo" );

        builder.ReadTexture( this, "FinalCompositionPass_ColorTarget" );
        builder.WriteTexture( this, "FinalCompositionPass_DepthTarget" );

        // Prepare to have at least MAX_RENDERABLE_ENTITIES
        m_Meshes.resize( MAX_RENDERABLE_ENTITIES );
    }

    auto ShadingPass::TraverseMeshList(PassCommandList& commandList) -> void {
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

    auto ShadingPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto ShadingPass::SetCamera( const Camera* camera ) -> void {
        m_FrameUBO.View = camera->GetViewMatrix();
        m_FrameUBO.Projection = camera->GetProjection();
        m_FrameUBO.CameraPosition = Vec4F{ camera->GetPosition(), 1.0f };
    }

    auto ShadingPass::EnableSkybox( bool enable ) -> void {
        m_UseSkybox = enable;
    }

    auto ShadingPass::SetClearColor( const Vec4F& vec ) -> void {
        m_ClearColor = vec;
    }

    auto ShadingPass::UploadInstanceData(PassCommandList& commandList) -> void {
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


                    // We need to ensure it does not get drawn later
                    instanceInfo.Disable( entityID );
                }
            }

            drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();
            drawState.FirstInstance = firstInstance;
            drawState.InstancesCount = drawCount;

            firstInstance += drawCount;

            commandList.DrawIndexed(drawState);
        }

        // Upload data
        commandList.FillBufferElement( "FinalCompositionPass_MeshInfo", m_Meshes.data(), sizeof( ShaderMaterialParams ), firstInstance );
    }

    auto ShadingPass::Execute( PassCommandList& commandList ) -> void {
        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "FinalCompositionPass_DepthTarget" );

        LoadOp colorTargetLoadOP{ LoadOp::LOAD };
        if (!m_UseSkybox) {
            colorTargetLoadOP = LoadOp::CLEAR;
            commandList.SetClearColor( m_ClearColor );
        }

        commandList.BeginRender(this, colorTargetLoadOP);
        commandList.BindPipeline( "FinalCompositionPass_Pipeline" );

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "PerFrame_CameraInfo", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 1 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 2 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 3 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "FinalCompositionPass_MeshInfo", 4 );

        commandList.BindResourceGroup(SRGType::SRG_Textures);
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        commandList.FillBuffer( "PerFrame_CameraInfo", std::addressof( m_FrameUBO ), sizeof( m_FrameUBO ) );

        TraverseMeshList(commandList);

        UploadInstanceData(commandList);

        commandList.EndRender();
    }

}// namespace Mikoto
