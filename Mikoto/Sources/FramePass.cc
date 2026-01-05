//
// Created by kate on 11/24/25.
//

#include <Material/ShaderLibrary.hh>
#include <Material/ShaderModule.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace Mikoto {

    auto FinalCompositionPass::Setup( FrameGraphBuilder& builder ) -> void {
        PipelineDescription builderPipelineDesc{};

        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX_STAGE );
        builderPipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest = true;
        graphicsDesc.DepthWrite = true;
        graphicsDesc.AlphaBlending = true;

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
                .ForElement( sizeof(FrameUBO), 1 );
        builder.CreateNamedBuffer( "PerFrame_CameraInfo", cameraInfo );

        // Declare its inputs and outputs
        builder.ReadBuffer( this, "AABBGenComp_CameraUBO" );
        builder.ReadBuffer( this, "AABBGenComp_Clusters" );
        builder.ReadBuffer( this, "LightCullingComp_LightsBuffer" );

        builder.ReadBuffer( this, "PerFrame_CameraInfo" );

        builder.WriteTexture( this, "FinalCompositionPass_ColorTarget" );
        builder.WriteTexture( this, "FinalCompositionPass_DepthTarget" );
    }

    auto FinalCompositionPass::TraverseMeshList(PassCommandList& commandList) -> void {
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComponent{ registry.get<MeshComponent>( entity ) };
            auto& materialComp{ registry.get<MaterialComponent>( entity ) };

            MaterialHandle material{ materialComp.GetMaterial() };

            if ( tag.IsActive() && meshComponent.HasMesh() && !material.IsEmpty() ) {
                MeshNode* meshNode{ meshComponent.GetMesh() };
                PBRMaterial* pbrMat{ dynamic_cast<PBRMaterial*>( material.GetRaw() ) };

                // We need to update this vertex buffer if we don't have this mesh
                if ( !m_MeshInstanceData.contains( meshNode ) ) {
                    m_UpdateInstanceData = true;
                }

                auto& [Mesh, Instances]{ m_MeshInstanceData[meshNode] };

                // We need to update this vertex buffer if we don't have its contents
                if ( !Instances.contains( tag.GetGUID() ) ) {
                    m_UpdateInstanceData = true;
                }

                ShadingPassMeshBufferUBO& ubo{ Instances[tag.GetGUID()] };
                ubo.i_TransformCol0 = transform.GetTransform()[0];
                ubo.i_TransformCol1 = transform.GetTransform()[1];
                ubo.i_TransformCol2 = transform.GetTransform()[2];
                ubo.i_TransformCol3 = transform.GetTransform()[3];

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

        if ( m_UpdateInstanceData ) {
            UpdateInstancedData();
            m_UpdateInstanceData = false;
        }

        // Copy contents
        UploadInstanceData();

        for ( auto& [meshNode, instanceData]: m_MeshInstanceData ) {
            DrawIndexedState drawIndexedState{};

            drawIndexedState.IndexBuffer = meshNode->GetIndexBuffer();
            drawIndexedState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0);
            drawIndexedState.VertexBuffers.emplace_back(instanceData.first, 1 );

            drawIndexedState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();
            drawIndexedState.InstancesCount = instanceData.second.size();

            commandList.DrawIndexed(drawIndexedState);
        }
    }

    auto FinalCompositionPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto FinalCompositionPass::SetCamera( const Camera* camera ) -> void {
        m_FrameUBO.View = camera->GetViewMatrix();
        m_FrameUBO.Projection = camera->GetProjection();
    }

    auto FinalCompositionPass::UploadInstanceData() -> void {
        for ( auto& meshInfo: m_MeshInstanceData | std::views::values ) {
            std::vector<ShadingPassMeshBufferUBO> instancesData{};

            for ( auto& instanceData: meshInfo.second | std::views::values ) {
                instancesData.emplace_back( instanceData );
            }

            meshInfo.first->CopyFromBlock( instancesData.data(), instancesData.size() * sizeof( ShadingPassMeshBufferUBO ) );
        }
    }

    auto FinalCompositionPass::UpdateInstancedData() -> void{
        for ( auto& meshInfo: m_MeshInstanceData | std::views::values ) {
            std::vector<ShadingPassMeshBufferUBO> instancesData{};

            for ( auto& instanceData: meshInfo.second | std::views::values ) {
                instancesData.emplace_back( instanceData );
            }

            BufferDescription vertexDesc{};
            vertexDesc.WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                    .WithData( reinterpret_cast<Byte*>( instancesData.data() ) )
                    .WithSizeBytes( InferSize<ShadingPassMeshBufferUBO>( instancesData.size() ) )
                    .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

            meshInfo.first = m_Device->CreateBuffer( vertexDesc );
        }
    }

    auto FinalCompositionPass::Execute( PassCommandList& commandList ) -> void {
        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "FinalCompositionPass_DepthTarget" );
        commandList.SetClearColor( m_ClearColor );

        commandList.BeginRender(this);
        commandList.BindPipeline( "FinalCompositionPass_Pipeline" );

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "PerFrame_CameraInfo", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 1 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 2 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 3 );

        commandList.BindResourceGroup(SRGType::SRG_Textures);
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        commandList.FillBuffer( "PerFrame_CameraInfo", std::addressof( m_FrameUBO ), sizeof( m_FrameUBO ) );

        TraverseMeshList(commandList);

        commandList.EndRender();
    }

    auto AABBGenComp::Setup( FrameGraphBuilder &builder ) -> void {
        PipelineDescription pipelineDesc{};
        pipelineDesc.Description = ComputePipelineDescription{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/AABBGen_Comp.sprv", ShaderStage::COMPUTE_STAGE );

        builder.CreateNamedPipeline( "AABBGenComp_Pipeline", pipelineDesc );

        BufferDescription cameraUBO{};
        cameraUBO.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof( CameraUBO ), 1 );
        builder.CreateNamedBuffer( "AABBGenComp_CameraUBO", cameraUBO );

        BufferDescription aabbBuffer{};
        aabbBuffer.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( numClusters * sizeof( Cluster ) );
        builder.CreateNamedBuffer( "AABBGenComp_Clusters", aabbBuffer );

        builder.WriteBuffer( this, "AABBGenComp_Clusters" );
        builder.WriteBuffer( this, "AABBGenComp_CameraUBO" );
    }

    auto AABBGenComp::Execute( PassCommandList &commandList ) -> void {
        commandList.BeginCompute(this);
        commandList.BindPipeline( "AABBGenComp_Pipeline" );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 1 );
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        CameraUBO cameraUBO{
            .ViewMatrix{ m_Camera->GetViewMatrix() },
            .InverseProjection{ glm::inverse(m_Camera->GetProjection()) },
            .GridSize{ glm::vec4{ gridSizeX, gridSizeY, gridSizeZ, 0.0f } }, // from the repo on clustered shading
            .Screen{ glm::vec4{m_Camera->GetNearPlane(), m_Camera->GetFarPlane(), 1920.0f, 1080.0f } },
        };

        commandList.FillBuffer( "AABBGenComp_CameraUBO", std::addressof( cameraUBO ), sizeof( CameraUBO ));

        commandList.Dispatch(gridSizeX, gridSizeY, gridSizeZ);

        commandList.EndCompute();
    }

    auto AABBGenComp::SetCamera( const Camera *camera ) -> void {
        m_Camera = camera;
    }

    auto LightCullingComp::Setup( FrameGraphBuilder &builder ) -> void {
        PipelineDescription pipelineDesc{};
        pipelineDesc.Description = ComputePipelineDescription{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/LightCulling_Comp.sprv", ShaderStage::COMPUTE_STAGE );

        builder.CreateNamedPipeline( "LightCullingComp_Pipeline", pipelineDesc );

        BufferDescription lightsBuffer{};
        lightsBuffer.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( sizeof(FinalCompositionPass::LightTypeInfo) * m_Lights.size()  );
        builder.CreateNamedBuffer( "LightCullingComp_LightsBuffer", lightsBuffer );

        builder.ReadBuffer( this, "AABBGenComp_CameraUBO" );
        builder.ReadBuffer( this, "AABBGenComp_Clusters" );

        builder.WriteBuffer( this, "LightCullingComp_LightsBuffer" );
    }

    auto LightCullingComp::Execute( PassCommandList &commandList ) -> void {
        commandList.BeginCompute(this);
        commandList.BindPipeline( "LightCullingComp_Pipeline" );

        TraverseLights( commandList );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 1 );
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        // 1 invocation = 1 tile
        commandList.Dispatch(27, 1, 1);

        commandList.EndCompute();
    }

    auto LightCullingComp::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto LightCullingComp::TraverseLights( const PassCommandList &commandList ) -> void {
        auto& registry{ m_Scene->GetRegistry() };
        auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        Int32 lightsCount{};

        for ( auto& lightEntity: lightsView ) {
            TagComponent& tag{ registry.get<TagComponent>( lightEntity ) };
            LightComponent& lightComp{ registry.get<LightComponent>( lightEntity ) };
            TransformComponent& transformCom{ registry.get<TransformComponent>( lightEntity ) };

            if ( lightsCount >= FinalCompositionPass::MAX_LIGHTS ) break;

            auto& uboLight{ m_Lights[lightsCount] };

            if (!tag.IsActive()) {
                uboLight.ActiveLightType = static_cast<Int32>(FinalCompositionPass::LightInfo::ActiveLightType::LIGHT_TYPE_INACTIVE);
                continue;
            }

            switch ( lightComp.GetActiveType() ) {
                case LightType::POINT_LIGHT_TYPE: {

                    auto& point{ lightComp.Get<PointLight>() };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Diffuse = Vec4F( point.GetColor(), 0.0f );

                    uboLight.Intensity = point.GetIntensity();
                    uboLight.Radius = point.GetRadius();

                    uboLight.ActiveLightType = static_cast<Int32>(FinalCompositionPass::LightInfo::ActiveLightType::LIGHT_TYPE_POINT);

                    break;
                }

                case LightType::SPOT_LIGHT_TYPE: {
                    auto& spot{ lightComp.Get<SpotLight>() };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Direction = Vec4F( spot.GetDirection(), 0.0f );
                    uboLight.Diffuse = Vec4F( spot.GetColor() * spot.GetIntensity(), 1.0f );

                    uboLight.CutOff = spot.GetCutOff();
                    uboLight.OuterCutOff = spot.GetOuterCutOff();

                    uboLight.Intensity = spot.GetIntensity();
                    uboLight.Radius = spot.GetRadius();

                    uboLight.ActiveLightType = static_cast<Int32>(FinalCompositionPass::LightInfo::ActiveLightType::LIGHT_TYPE_SPOT);

                    break;
                }

                case LightType::DIRECTIONAL_LIGHT_TYPE: {
                    auto& dir{ lightComp.Get<DirectionalLight>() };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );// optional for shadows
                    uboLight.Diffuse = Vec4F( dir.GetColor() * dir.GetIntensity(), 1.0f );
                    uboLight.ActiveLightType = static_cast<Int32>(FinalCompositionPass::LightInfo::ActiveLightType::LIGHT_TYPE_DIRECTIONAL);

                    break;
                }
            }

            ++lightsCount;
        }

        // Copy to GPU buffer
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 2 );
        commandList.FillBuffer( "LightCullingComp_LightsBuffer", m_Lights.data(), m_Lights.size() * sizeof(FinalCompositionPass::LightTypeInfo));
    }

    auto ShadowPass::Setup( FrameGraphBuilder& builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Shadowmap_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};

        pipelineDesc.Description = graphicsDesc;

        builder.CreateNamedPipeline( "ShadowPass_Pipeline", pipelineDesc );

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
                .WithSizeBytes( 0 );// TODO
        builder.CreateNamedBuffer( "ShadowPass_LightsBuffer", lightsBuffer );

        // Transform, texture indices, etc
        BufferDescription objectsInfo{};
        objectsInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( 0 );// TODO
        builder.CreateNamedBuffer( "ShadowPass_ObjectInfo", objectsInfo );

        // Camera
        BufferDescription camera{};
        objectsInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( 0 );// TODO
        builder.CreateNamedBuffer( "ShadowPass_CameraInfo", camera );

        // Declare its inputs and outputs
        builder.WriteTexture( this, "ShadowPass_ColorTarget" );
        builder.WriteTexture( this, "ShadowPass_DepthTarget" );
        builder.WriteBuffer( this, "ShadowPass_ObjectInfo" );
        builder.WriteBuffer( this, "ShadowPass_CameraInfo" );
    }

    auto ShadowPass::Execute( PassCommandList& commandList ) -> void {

        commandList.SetColorRenderTarget( "ShadowPass_ColorTarget" );
        commandList.SetDepthRenderTarget( "ShadowPass_LightsBuffer" );

        commandList.BeginRender(this);

        // commandList.BindStorageBuffer( "ShadowPass_CameraInfo", 0, 0);
        // commandList.BindStorageBuffer( "ShadowPass_LightsBuffer", 1, 0);
        // commandList.BindStorageBuffer( "ShadowPass_ObjectInfo", 2, 0);

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.BindPipeline( "ShadowPass_Pipeline" );

        // Meshes
        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for ( auto& entity: renderables ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& meshComponent{ registry.get<MeshComponent>( entity ) };
            auto& materialComp{ registry.get<MaterialComponent>( entity ) };

            MaterialHandle material{ materialComp.GetMaterial() };

            if ( tag.IsActive() && meshComponent.HasMesh() && !material.IsEmpty() ) {
                PBRMaterial* matPtr{ dynamic_cast<PBRMaterial*>( material.GetRaw() ) };

                MeshNode* mesh{ meshComponent.GetMesh() };


            }
        }

        // Lights
        auto lights{ registry.view<TagComponent, TransformComponent, LightComponent>() };
        for ( auto& entity: lights ) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& lightComp{ registry.get<LightComponent>( entity ) };
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

    auto HelloCubePass::Setup( FrameGraphBuilder& device ) -> void {
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

}// namespace Mikoto
