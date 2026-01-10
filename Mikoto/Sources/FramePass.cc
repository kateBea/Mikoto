//
// Created by kate on 11/24/25.
//

#include <Material/ShaderLibrary.hh>
#include <Material/ShaderModule.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

#include <Material/TextureCube.hh>

namespace Mikoto {

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

        TextureCubeLoadDescription loadDesc{};
        loadDesc.WithType( TextureType::TEXTURE_CUBE )
            .WithBasePath("Resources/Cubemaps/Lycksele2")
            .WithFacePath( "posx.jpg" )
            .WithFacePath( "negx.jpg" )

            .WithFacePath( "negy.jpg" )
            .WithFacePath( "posy.jpg" )

            .WithFacePath( "posz.jpg" )
            .WithFacePath( "negz.jpg" );

        // +X -> right.jpg
        // -X -> left.jpg
        // +Y -> top.jpg      // often needs vertical flip
        // -Y -> bottom.jpg   // often needs vertical flip
        // -Z -> front.jpg
        // +Z -> back.jpg

        TextureHandle skybox{ AssetsService::Get()->LoadAsset<TextureCube>( loadDesc ) };

        SamplerDescription samplerDescription{ .CubeSampler{ true } };
        commandList.CreateNamedSampler("SkyboxPass_Sampler", samplerDescription);
        commandList.RegisterNamedTexture("SkyboxPass_TextureCube", skybox);

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

        builder.WriteBuffer( this, "TextRenderPass_FontVertexBuffer" );
        builder.WriteBuffer( this, "TextRenderPass_FontIndexBuffer" );
        builder.WriteBuffer( this, "TextRenderPass_FontParams" );
        builder.WriteBuffer( this, "TextRenderPass_TextRenderParams" );
    }

    auto TextRenderPass::Execute( PassCommandList &commandList ) -> void {
        MKT_ASSERT( m_Scene != nullptr, "Scene cannot be NULL" );
        MKT_ASSERT( m_Camera != nullptr, "Camera cannot be NULL" );

        commandList.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );

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

            SetupTextForRender(textComponent.GetFontHandle(), position, textComponent.GetContents(), textSize, color, commandList );
        }

        commandList.FillBuffer( "TextRenderPass_TextRenderParams", m_TextRenderParams.data(), m_TextRenderParams.size() * sizeof( TextRenderParams ) );
    }

    auto TextRenderPass::SetupRenderParams(PassCommandList &commandList) -> void {
        m_TextRenderUBO.OutlineWidth = 0.0f;

        bool is3D{ true };

        if (!is3D) {
            m_TextRenderUBO.Proj = glm::ortho(0.0f, 1920.0f,1080.0f, 0.0f,-1.0f, 1.0f);
            m_TextRenderUBO.View = glm::mat4{ 1.0f };
        } else {
            m_TextRenderUBO.Proj = m_Camera->GetProjection();
            m_TextRenderUBO.View = m_Camera->GetViewMatrix();
        }

        commandList.FillBuffer( "TextRenderPass_FontParams", std::addressof( m_TextRenderUBO ), sizeof( m_TextRenderUBO ) );
    }

    auto TextRenderPass::SetupTextForRender( FontHandle font, Vec4F position, std::string_view text, double fontSize, Vec4F color, PassCommandList& commandList ) -> void {
        double xPos{ position.x };
        double yPos{ position.y };
        double scale{ fontSize / font->GetSize() };

        double lineHeight{ font->GetMaxHeight() * scale };

        for ( Size i{}; i < text.length(); ++i ) {
            if (text[i] == '\n') {
                xPos = position.x;
                yPos -= lineHeight;
                continue;
            }

            FontGlyph& glyph{ font->GetGlyph( static_cast<UInt32>( text[i] ) ) };

            if ( text[i] != ' ' ) {
                // Quad Coordinates
                double x0{ xPos + glyph.m_PlaneBounds.x * fontSize };
                double y0{ yPos - glyph.m_PlaneBounds.y * fontSize };

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

        BufferDescription meshInstanceInfo{};
        meshInstanceInfo.WithData( nullptr )
                .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                .ForElement( sizeof(ShaderMeshInfo), MAX_RENDER_ENTITIES );
        builder.CreateNamedBuffer( "FinalCompositionPass_MeshInfo", meshInstanceInfo );

        // Declare its inputs and outputs
        builder.ReadBuffer( this, "AABBGenComp_CameraUBO" );
        builder.ReadBuffer( this, "AABBGenComp_Clusters" );
        builder.ReadBuffer( this, "LightCullingComp_LightsBuffer" );

        builder.ReadBuffer( this, "PerFrame_CameraInfo" );

        builder.WriteBuffer( this, "FinalCompositionPass_MeshInfo" );

        builder.ReadTexture( this, "FinalCompositionPass_ColorTarget" );
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

                auto& [DrawIndexedState, Instances]{ m_MeshDrawState[meshNode] };

                ShaderMeshInfo& ubo{ Instances[tag.GetGUID()] };
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

    auto FinalCompositionPass::SetScene( Scene* scene ) -> void {
        m_Scene = scene;
    }

    auto FinalCompositionPass::SetCamera( const Camera* camera ) -> void {
        m_FrameUBO.View = camera->GetViewMatrix();
        m_FrameUBO.Projection = camera->GetProjection();
        m_FrameUBO.CameraPosition = Vec4F{ camera->GetPosition(), 1.0f };
    }

    auto FinalCompositionPass::SetCamera( const Vec4F &color ) -> void {
        m_ClearColor = color;
    }

    auto FinalCompositionPass::EnableSkybox( bool enable ) -> void {
        m_UseSkybox = enable;
    }

    auto FinalCompositionPass::UploadInstanceData(PassCommandList& commandList) -> void {
        UInt32 firstInstance{};

        Size meshIndex{};
        for ( auto& [meshNode, instanceInfo]: m_MeshDrawState ) {
            DrawIndexedState& drawState{ instanceInfo.InstanceDrawState };

            drawState.IndexBuffer = meshNode->GetIndexBuffer();

            if (drawState.VertexBuffers.empty()) {
                drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0);
            }

            drawState.IndicesCount = meshNode->GetIndexBuffer()->GetCount();
            drawState.InstancesCount = instanceInfo.InstanceInfos.size();
            drawState.FirstInstance = firstInstance;

            const Size totalInstancesOffsetSizeBytes{ firstInstance * sizeof( ShaderMeshInfo ) };

            UInt32 instanceIndex{ 0 };
            for (const auto &meshInstanceInfo: instanceInfo.InstanceInfos | std::views::values) {
                //commandList.FillBuffer( "FinalCompositionPass_MeshInfo", std::addressof(  meshInstanceInfo ), sizeof( ShaderMeshInfo ), totalInstancesOffsetSizeBytes + sizeof( ShaderMeshInfo ) * instanceIndex );
                m_Meshes[meshIndex++] = meshInstanceInfo;
                ++instanceIndex;
            }

            commandList.FillBufferElement( "FinalCompositionPass_MeshInfo", m_Meshes.data(), sizeof( ShaderMeshInfo ), m_Meshes.size() );

            firstInstance += instanceInfo.InstanceInfos.size();

            commandList.DrawIndexed(drawState);
        }
    }

    auto FinalCompositionPass::Execute( PassCommandList& commandList ) -> void {
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

                    uboLight.Direction = Vec4F( dir.GetDirection(), 0.0f );
                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );// optional for shadows
                    uboLight.Diffuse = Vec4F( dir.GetColor() * dir.GetIntensity(), 1.0f );
                    uboLight.ActiveLightType = static_cast<Int32>(FinalCompositionPass::LightInfo::ActiveLightType::LIGHT_TYPE_DIRECTIONAL);

                    break;
                }
            }

            ++lightsCount;
        }

        m_LightCullingUBO.LightCount = lightsCount;

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
