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

    auto TextRenderPass::Setup( FrameGraphBuilder &builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Text_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Text_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        GraphicsPipelineDescription graphicsDesc{};

        graphicsDesc.DepthTest = true;
        graphicsDesc.DepthWrite = true;
        graphicsDesc.AlphaBlending = true;

        // we're providing none as the shader expects none
        const BufferLayout layout{
                        { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                        { ShaderDataType::UINT_TYPE, "a_TexCoordIndex" },
                    };
        graphicsDesc.VertexAttributesSpec = { AttributesSpec{
            .DefaultVertexLayout { layout },
            .InputRateSpec{ .BindingIndex = { 0 }, .AttributeRate { InputRate::PER_VERTEX } }
        } };
        graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;

        pipelineDesc.Description = graphicsDesc;

        pipelineDesc.ColorRenderTargets.emplace_back( "FinalCompositionPass_ColorTarget" );
        //pipelineDesc.DepthRenderTargets = "FinalCompositionPass_DepthTarget";

        builder.CreateNamedPipeline( "TextRenderPass_Pipeline", pipelineDesc );

        // TODO: use the final composition render target because this pass render text on the final composition target
        //builder.CreateColorRenderTarget( "TextRenderPass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );
        //builder.CreateDepthRenderTarget( "TextRenderPass_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

        BufferDescription fontRenderParams{};
        fontRenderParams.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .WithSizeBytes( sizeof(TextRenderParams) * MAX_STRING );
        builder.CreateNamedBuffer( "TextRenderPass_TextRenderParams", fontRenderParams );

        BufferDescription fontParamsUBO{};
        fontParamsUBO.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof(TextParamsUBO), 1 );
        builder.CreateNamedBuffer( "TextRenderPass_FontParams", fontParamsUBO );

        // Vertex buffer and index buffer for the quads
        BufferDescription vertexDesc{};
        vertexDesc.WithData( reinterpret_cast<Byte*>( VERTICES.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
                .WithSizeBytes( InferSize<FontVertex>( VERTICES.size() ) )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        builder.CreateNamedBuffer( "TextRenderPass_FontVertexBuffer", vertexDesc );

        BufferDescription indexDesc{};
        indexDesc.WithData( reinterpret_cast<Byte*>( INDICES.data() ) )
                .WithUsage( BufferUsage::BUFFER_USAGE_INDEX )
                .WithSizeBytes( InferSize<UInt32>( INDICES.size() ) )
                .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
        builder.CreateNamedBuffer( "TextRenderPass_FontIndexBuffer", indexDesc );

        builder.ReadTexture( this, "FinalCompositionPass_ColorTarget" );
        //builder.ReadTexture( this, "FinalCompositionPass_DepthTarget" );

        builder.WriteBuffer( this, "TextRenderPass_FontVertexBuffer" );
        builder.WriteBuffer( this, "TextRenderPass_FontIndexBuffer" );
        builder.WriteBuffer( this, "TextRenderPass_FontParams" );
        builder.WriteBuffer( this, "TextRenderPass_TextRenderParams" );
    }

    auto TextRenderPass::Execute( PassCommandList &commandList ) -> void {
        MKT_ASSERT( m_Scene != nullptr, "Scene cannot be NULL" );
        MKT_ASSERT( m_Camera != nullptr, "Camera cannot be NULL" );

        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        //commandList.SetDepthRenderTarget( "TextRenderPass_DepthTarget" );
        //commandList.SetClearColor( m_ClearColor );

        commandList.BeginRender(this, LoadOp::LOAD);
        commandList.BindPipeline( "TextRenderPass_Pipeline" );

        // Set render targets
        commandList.SetViewport( 0, 0, 1920, 1080 );
        commandList.SetScissor( 0, 0, 1920, 1080 );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "TextRenderPass_FontParams", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "TextRenderPass_TextRenderParams", 1 );

        commandList.BindResourceGroup(SRGType::SRG_Textures);
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        TraverseTextList(commandList);
        SetupRenderParams(commandList);

        DrawIndexedState drawIndexedState{};

        BufferHandle vertexBuffer{ commandList.GetNamedBuffer("TextRenderPass_FontVertexBuffer") };
        BufferHandle indexBuffer{ commandList.GetNamedBuffer("TextRenderPass_FontIndexBuffer") };

        drawIndexedState.IndexBuffer = indexBuffer;
        drawIndexedState.VertexBuffers.emplace_back( vertexBuffer, 0);

        drawIndexedState.IndicesCount = indexBuffer->GetCount();
        drawIndexedState.InstancesCount = m_TextRenderParams.size();

        commandList.DrawIndexed(drawIndexedState);

        commandList.EndRender();
    }

    auto TextRenderPass::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto TextRenderPass::SetCamera( const Camera *camera ) -> void {
        m_Camera = camera;
    }

    auto TextRenderPass::TraverseTextList( PassCommandList &commandList ) -> void {
        FontHandle testFont{ AssetsService::Get()->LoadAsset<Font>( Path{ "Resources/Fonts/Google_Sans_Code/GoogleSansCode-VariableFont_wght.ttf" } ) };

        // Clear text data as we refill it every frame
        m_TextRenderParams.clear();

        auto& registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, TextComponent>() };

        for (auto& entity : renderables) {
            auto& tag{ registry.get<TagComponent>( entity ) };
            auto& transform{ registry.get<TransformComponent>( entity ) };
            auto& textComponent{ registry.get<TextComponent>( entity ) };

            if (!textComponent.HasFont()) {
                continue;
            }

            const float textSize{ textComponent.GetSize() };
            const glm::vec4 color{ textComponent.GetColor() };
            const glm::vec4 position{ transform.GetTranslation(), 1.0f };

            SetupTextForRender(textComponent.GetFontHandle(), position, transform.GetTransform(), textComponent.GetContents(), textSize, color, commandList );
        }

        commandList.FillBuffer( "TextRenderPass_TextRenderParams", m_TextRenderParams.data(), m_TextRenderParams.size() * sizeof( TextRenderParams ) );
    }

    auto TextRenderPass::SetupRenderParams(PassCommandList &commandList) -> void {
        m_TextRenderUBO.OutlineWidth = 0.0f;

        bool is3D{ false };

        if (!is3D) {
            m_TextRenderUBO.Proj = glm::ortho(0.0f, 1920.0f,1080.0f, 0.0f,-1.0f, 1.0f);
            m_TextRenderUBO.View = glm::mat4{ 1.0f };
        } else {
            m_TextRenderUBO.Proj = m_TextRenderUBO.Proj = m_Camera->GetProjection();
            m_TextRenderUBO.View = m_Camera->GetViewMatrix();
        }
        commandList.FillBuffer( "TextRenderPass_FontParams", std::addressof( m_TextRenderUBO ), sizeof( m_TextRenderUBO ) );
    }

    auto TextRenderPass::SetupTextForRender( FontHandle font, Vec4F position, Mat4F model, std::string_view text, double fontSize, Vec4F color, PassCommandList& commandList ) -> void {
        double xPos{ position.x };
        double yPos{ position.y };
        double scale{ fontSize / font->GetSize() };

        double lineHeight{ font->GetMaxHeight() * scale };

        for ( Size i{}; i < text.length(); ++i ) {
            if (text[i] == '\n') {
                xPos = position.x;
                yPos += lineHeight;
                continue;
            }

            FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( text[i] ) ) };

            if ( text[i] != ' ' ) {
                // Quad Coordinates
                double x0 = xPos + glyph.m_PlaneBounds.x * fontSize;
                double y0 = yPos - glyph.m_PlaneBounds.y * fontSize;

                // UV Coordinates
                TextureHandle atlas{ font->GetAtlas() };
                double s0 = glyph.m_AtlasBounds.x / atlas->GetWidth();
                double t0 = glyph.m_AtlasBounds.w / atlas->GetHeight();
                double s1 = glyph.m_AtlasBounds.z / atlas->GetWidth();
                double t1 = glyph.m_AtlasBounds.y / atlas->GetHeight();

                TextRenderParams fontParams{};
                fontParams.Position = { x0, y0 + std::round( ( font->GetMaxHeight() * scale ) ) - ( glyph.m_Height * scale ), position.z, position.w };
                fontParams.Size = { glyph.m_Width * scale, glyph.m_Height * scale, 0.0f, 0.0f };
                fontParams.Color = color;
                fontParams.Model = model;
                fontParams.TexIndex = commandList.PushTexture( atlas );
                fontParams.TexCoords[0] = { s0, t0 };// top left
                fontParams.TexCoords[1] = { s1, t0 };// bottom left
                fontParams.TexCoords[2] = { s1, t1 };// bottom right
                fontParams.TexCoords[3] = { s0, t1 };// top right

                m_TextRenderParams.emplace_back( fontParams );
            }

            xPos += glyph.m_AdvanceX * fontSize;
        }
    }

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
                .WithSizeBytes( m_NumClusters * sizeof( Cluster ) );
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

        m_CameraUBO = {
            .ViewMatrix{ m_Camera->GetViewMatrix() },
            .InverseProjection{ glm::inverse(m_Camera->GetProjection()) },

            .GridSize{ glm::vec4{ m_GridSizeX, m_GridSizeY, m_GridSizeZ, 0.0f } },
            .ViewPosition{ glm::vec4{ m_Camera->GetPosition(), 0.0f } },

            .Screen{ m_Camera->GetNearPlane(), m_Camera->GetFarPlane(), 1920.0f, 1080.0f },
            .LightInfo{ m_CameraUBO.LightInfo.x }
        };

        commandList.FillBuffer( "AABBGenComp_CameraUBO", std::addressof( m_CameraUBO ), sizeof( CameraUBO ));

        commandList.Dispatch(m_GridSizeX, m_GridSizeY, m_GridSizeZ);

        commandList.EndCompute();
    }

    auto AABBGenComp::SetCamera( const Camera *camera ) -> void {
        m_Camera = camera;
    }

    auto AABBGenComp::SetHeatMap( bool enable ) -> void {
        m_CameraUBO.LightInfo.x = enable == 1 ? 1.0 : 0.0;
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

        BufferDescription lightCulling{};
        lightCulling.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement(  sizeof(LightCullingUBO), 1 );
        builder.CreateNamedBuffer( "LightCullingComp_LightsCullingInfo", lightCulling );

        builder.ReadBuffer( this, "AABBGenComp_CameraUBO" );
        builder.ReadBuffer( this, "AABBGenComp_Clusters" );

        builder.WriteBuffer( this, "LightCullingComp_LightsBuffer" );
        builder.WriteBuffer( this, "LightCullingComp_LightsCullingInfo" );
    }

    auto LightCullingComp::Execute( PassCommandList &commandList ) -> void {
        MKT_ASSERT( m_NumClusters != 0, "Number of cluster must different to 0" );

        commandList.BeginCompute(this);
        commandList.BindPipeline( "LightCullingComp_Pipeline" );

        TraverseLights( commandList );

        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 0 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 1 );
        commandList.BindResourceGroup(SRGType::SRG_PerPass);

        const auto numWorkGroupsX{ (m_NumClusters + m_LocalSize - 1) / m_LocalSize };
        commandList.Dispatch(numWorkGroupsX, 1, 1);

        commandList.EndCompute();
    }

    auto LightCullingComp::SetClusterCount( UInt32 clusterCount ) -> void {
        m_NumClusters = clusterCount;
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

            if ( lightsCount >= FinalCompositionPass::MAX_LIGHTS ) {
                break;
            }

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

        m_LightCullingUBO.LightInfo.x = static_cast<float>( lightsCount );

        // Copy to GPU buffer
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 2 );
        commandList.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsCullingInfo", 3 );

        // Just copy the amount of active lights we visited
        commandList.FillBuffer( "LightCullingComp_LightsBuffer", m_Lights.data(), lightsCount  * sizeof(FinalCompositionPass::LightTypeInfo));
        commandList.FillBuffer( "LightCullingComp_LightsCullingInfo", std::addressof( m_LightCullingUBO ), sizeof(LightCullingUBO) );
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

        builder.CreateColorRenderTarget( "ShadowPass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM );
        builder.CreateDepthRenderTarget( "ShadowPass_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

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

}// namespace Mikoto
