//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Math/Math.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>
#include <Renderer/Passes/IBLPasses.hh>

namespace Mikoto {

    auto RegisterIrradiance( FrameGraph &graph ) -> void {

        struct IrradianceData {
            TextureHandle ColorTarget;
            PipelineHandle Pipeline;
            UInt32 MipLevels;
            UInt32 Dimensions;

            // Ubo data
        };

        // graph.RegisterPass<IrradianceData>(
        //         "IrradiancePass",
        //
        //         [&]( FramePassBuilder &b, IrradianceData &data ) {
        //             // Compute mip levels
        //             data.MipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( data.Dimensions ) ) ) + 1;
        //
        //             // Create target
        //             data.ColorTarget =
        //                     b.CreateCubeTexture( "IrradiancePass_ColorTarget",
        //                                          data.Dimensions,
        //                                          TextureFormat::TEXTURE_FORMAT_RGBA32_FLOAT,
        //                                          data.MipLevels );
        //
        //             // Create pipeline
        //             PipelineDescription desc{};
        //             desc.AddShader( "...Vert.sprv", ShaderStage::VERTEX_STAGE );
        //             desc.AddShader( "...Frag.sprv", ShaderStage::FRAGMENT_STAGE );
        //             desc.ColorRenderTargets.emplace_back( "IrradiancePass_ColorTarget" );
        //             data.Pipeline = b.CreateNamedPipeline( "IrradiancePass_Pipeline", desc );
        //
        //             // Declare framegraph usages
        //             b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget_Color );
        //             b.Read( "SkyboxPass_TextureCube", FrameResourceState::ShaderResource_Read );
        //
        //             b.BindBuffer( "IrradiancePass_CameraInfo", SRGType::SRG_PerPass, 0 );
        //             b.BindBuffer( "IrradiancePass_Parameters", SRGType::SRG_PerPass, 1 );
        //             b.BindTexture( "SkyboxPass_TextureCube", SRGType::SRG_PerPass, 2, "SkyboxPass_Sampler" );
        //         },
        //
        //         [&]( CommandContext &ctx, FrameBlackboard &blackboard ) {
        //             const IrradianceData &data{ blackboard.Get<IrradianceData>() };
        //
        //             ctx.BeginPass( "IrradiancePass" );
        //
        //             ctx.BindPipeline( data.Pipeline );
        //             ctx.SetViewport( 0, 0, 1920, 1080 );
        //             ctx.SetScissor( 0, 0, 1920, 1080 );
        //
        //             for (uint32_t mip = 0; mip < data.MipLevels; ++mip) {
        //                 for (uint32_t face = 0; face < 6; ++face) {
        //                     ctx.SetColorRenderTarget( data.ColorTarget, face, mip );
        //
        //                     // update UBOs
        //                     ctx.FillBuffer( "IrradiancePass_CameraInfo", &ubo, sizeof( ubo ) );
        //
        //                     ctx.BeginRender();
        //                     ctx.Draw( 6 );
        //                     ctx.EndRender();
        //                 }
        //             }
        //
        //             ctx.EndPass();
        //         } );

    }

    auto IrradiancePass::Setup( FrameGraphBuilder &builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/Irradiance_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/Irradiance_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        pipelineDesc.Description = GraphicsPipelineDescription{
            .VertexAttributesSpec{}
        };

        // TODO: temporary, specify the render targets this pipeline outputs to
        pipelineDesc.ColorRenderTargets.emplace_back( "IrradiancePass_ColorTarget" );

        builder.CreateNamedPipeline( "IrradiancePass_Pipeline", pipelineDesc );

        m_MipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( m_Dimensions ) ) ) + 1;

        builder.CreateCubeRenderTarget( "IrradiancePass_ColorTarget", m_Dimensions, TextureFormat::TEXTURE_FORMAT_RGBA32_FLOAT, m_MipLevels );

        builder.WriteTexture( this, "IrradiancePass_ColorTarget" );
        builder.ReadTexture( this, "SkyboxPass_TextureCube" );
    }

    auto IrradiancePass::Execute( CommandContext &context ) -> void {
        context.BeginPass( this );

        context.SetViewport( 0, 0, 1920, 1080 );
        context.SetScissor( 0, 0, 1920, 1080 );

        context.BindPipeline( "IrradiancePass_Pipeline" );

        context.SetBufferBindSlot( SRGType::SRG_PerPass, "IrradiancePass_CameraInfo", 0 );
        context.SetBufferBindSlot( SRGType::SRG_PerPass, "IrradiancePass_Parameters", 1 );

        context.SetTextureBindSlot( SRGType::SRG_PerPass, "SkyboxPass_TextureCube", "SkyboxPass_Sampler", 1 );

        context.BindResourceGroup( SRGType::SRG_PerPass );

        for (Size mipLevel{}; mipLevel < m_MipLevels; ++mipLevel) {
            for (Size count{}; count < MAX_CUBE_FACES; ++count) {
                context.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                m_CameraInfo.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * s_Matrices[count];

                context.FillBuffer( "IrradiancePass_CameraInfo", std::addressof( m_CameraInfo ), sizeof( m_CameraInfo ) );
                context.FillBuffer( "IrradiancePass_Parameters", std::addressof( m_Parameters ), sizeof( m_Parameters ) );

                PassRenderInfo renderInfo{
                    .ColorLoadOp{ LoadOp::CLEAR },
                    .DephtLoadOp{ LoadOp::CLEAR },
                };

                context.SetClearColor( { 0.0f, 0.0f, 0.0f, 1.0f } );

                context.BeginRender( renderInfo );
                context.BindPipeline( "IrradiancePass_Pipeline" );
                context.Draw( 6, 1, 0, 0 );

                context.EndRender();
            }
        }

        context.EndPass();
    }

    auto PrefilterPass::Setup( FrameGraphBuilder &builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/Prefilter_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/Prefilter_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        pipelineDesc.Description = GraphicsPipelineDescription{
            .VertexAttributesSpec{}
        };

        // TODO: temporary, specify the render targets this pipeline outputs to
        pipelineDesc.ColorRenderTargets.emplace_back( "PrefilterPass_ColorTarget" );

        builder.CreateNamedPipeline( "PrefilterPass_Pipeline", pipelineDesc );

        constexpr UInt32 dimensions{ 512 };
        builder.CreateCubeRenderTarget( "PrefilterPass_ColorTarget", dimensions, TextureFormat::TEXTURE_FORMAT_RGBA16_FLOAT );

        builder.WriteTexture( this, "PrefilterPass_ColorTarget" );
    }

    auto PrefilterPass::Execute( CommandContext &context ) -> void {
        context.BeginPass( this );

        context.SetViewport( 0, 0, 1920, 1080 );
        context.SetScissor( 0, 0, 1920, 1080 );

        // For different mip levels we render the same cube
        for (Size mipLevel{}; mipLevel < MAX_MIP_LEVELS; ++mipLevel) {
            m_Parameters.Roughness = static_cast<float>( mipLevel ) / static_cast<float>( MAX_MIP_LEVELS - 1 );

            context.FillBuffer( "IrradiancePass_Parameters", std::addressof( m_Parameters ), sizeof( m_Parameters ) );

            for (Size count{}; count < MAX_CUBE_FACES; ++count) {
                context.SetColorRenderTarget( "IrradiancePass_ColorTarget" );
                context.SetClearColor( { 0.0f, 0.0f, 0.0f, 1.0f } );

                PassRenderInfo renderInfo{
                    .ColorLoadOp{ LoadOp::CLEAR },
                    .DephtLoadOp{ LoadOp::CLEAR },
                };

                context.BeginRender( renderInfo );
                context.BindPipeline( "IrradiancePass_Pipeline" );

                context.SetBufferBindSlot( SRGType::SRG_PerPass, "IrradiancePass_CameraInfo", 0 );
                context.SetBufferBindSlot( SRGType::SRG_PerPass, "IrradiancePass_Parameters", 1 );
                context.BindResourceGroup( SRGType::SRG_PerPass );

                context.FillBuffer( "IrradiancePass_CameraInfo", std::addressof( m_CameraInfo ), sizeof( m_CameraInfo ) );

                context.Draw( 6, 1, 0, 0 );

                context.EndRender();
            }
        }

        context.Draw( 3, 1, 0, 0 );

        context.EndPass();
    }

    auto BRDFLutPass::Setup( FrameGraphBuilder &builder ) -> void {
        // Create resources it needs
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/BRDFLut_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "Resources/Shaders/vulkan-spirv/BRDFLut_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        // Configure pipeline stage
        pipelineDesc.Description = GraphicsPipelineDescription{
            .VertexAttributesSpec{}
        };

        // TODO: temporary, specify the render targets this pipeline outputs to
        pipelineDesc.ColorRenderTargets.emplace_back( "BRDFLutPass_ColorTarget" );
        pipelineDesc.DepthRenderTargets = "BRDFLutPass_DepthTarget";

        builder.CreateNamedPipeline( "BRDFLutPass_Pipeline", pipelineDesc );

        builder.CreateColorRenderTarget( "BRDFLutPass_ColorTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_RG16_FLOAT );
        builder.CreateDepthRenderTarget( "BRDFLutPass_DepthTarget", 1920, 1080, TextureFormat::TEXTURE_FORMAT_D32_FLOAT );

        builder.WriteTexture( this, "BRDFLutPass_ColorTarget" );
        builder.WriteTexture( this, "BRDFLutPass_DepthTarget" );
    }

    auto BRDFLutPass::Execute( CommandContext &context ) -> void {
        context.BeginPass( this );

        context.SetColorRenderTarget( "BRDFLutPass_ColorTarget" );
        context.SetDepthRenderTarget( "BRDFLutPass_DepthTarget" );

        context.SetClearColor( { 0.0f, 0.0f, 0.0f, 1.0f } );

        context.BeginRender( PassRenderInfo{} );
        context.BindPipeline( "BRDFLutPass_Pipeline" );

        context.SetViewport( 0, 0, 1920, 1080 );
        context.SetScissor( 0, 0, 1920, 1080 );

        context.Draw( 3, 1, 0, 0 );

        context.EndRender();

        context.EndPass();
    }

    auto SkyboxPass::Setup( FrameGraphBuilder &builder ) -> void {
        PipelineDescription pipelineDesc{};

        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Skybox_Vert.sprv", ShaderStage::VERTEX_STAGE );
        pipelineDesc.AddShader( "./Resources/Shaders/vulkan-spirv/Skybox_Frag.sprv", ShaderStage::FRAGMENT_STAGE );

        GraphicsPipelineDescription graphicsDesc{};
        graphicsDesc.DepthTest = false;
        graphicsDesc.DepthWrite = false;
        graphicsDesc.AlphaBlending = false;
        graphicsDesc.PipelineCullMode = CullMode::NONE;
        graphicsDesc.PrimitiveTopology = Topology::TRIANGLE_LIST;

        graphicsDesc.VertexAttributesSpec = {};

        pipelineDesc.Description = graphicsDesc;
        pipelineDesc.ColorRenderTargets.emplace_back( "FinalCompositionPass_ColorTarget" );

        builder.CreateNamedPipeline( "SkyboxPass_Pipeline", pipelineDesc );

        BufferDescription cameraInfo{};
        cameraInfo.WithData( nullptr )
                  .WithUsage( BufferUsage::BUFFER_USAGE_UNIFORM )
                  .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                  .ForElement( sizeof( SkyboxUBO ), 1 );
        builder.CreateNamedBuffer( "SkyboxPass_CameraInfo", cameraInfo );

        builder.WriteBuffer( this, "SkyboxPass_CameraInfo" );

        // For now will use its own render target but should use final composition one
        builder.WriteTexture( this, "FinalCompositionPass_ColorTarget" );

        builder.WriteTexture( this, "SkyboxPass_TextureCube" );
    }

    auto SkyboxPass::Execute( CommandContext &context ) -> void {
        context.BeginPass( this );

        if (m_CubeMap.IsEmpty()) { return; }

        context.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );

        context.BeginRender( PassRenderInfo{} );
        context.BindPipeline( "SkyboxPass_Pipeline" );

        SamplerDescription samplerDescription{ .CubeSampler{ true } };
        context.CreateNamedSampler( "SkyboxPass_Sampler", samplerDescription );
        context.RegisterNamedTexture( "SkyboxPass_TextureCube", m_CubeMap );

        context.SetBufferBindSlot( SRGType::SRG_PerPass, "SkyboxPass_CameraInfo", 0 );
        context.SetTextureBindSlot( SRGType::SRG_PerPass, "SkyboxPass_TextureCube", "SkyboxPass_Sampler", 1 );

        context.BindResourceGroup( SRGType::SRG_PerPass );

        context.SetViewport( 0, 0, 1920, 1080 );
        context.SetScissor( 0, 0, 1920, 1080 );

        context.FillBuffer( "SkyboxPass_CameraInfo", std::addressof( m_SkyboxUBO ), sizeof( SkyboxUBO ) );

        context.Draw( 36, 1, 0, 0 );

        context.EndRender();

        context.EndPass();
    }

    auto SkyboxPass::SetCamera( const Camera *camera ) -> void {
        m_SkyboxUBO.Projection = camera->GetProjection();
        m_SkyboxUBO.View = camera->GetViewMatrix();
    }

    auto SkyboxPass::SetCubeMap( TextureHandle cubeMap ) -> void { m_CubeMap = cubeMap; }

    auto SkyboxPass::SetExposure( float value ) -> void { m_SkyboxUBO.Exposure = value; }

    auto SkyboxPass::SetGamma( float value ) -> void { m_SkyboxUBO.Gamma = value; }

    auto ShadingPass::Setup( FrameGraphBuilder &builder ) -> void {
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
                  .ForElement( sizeof( ShaderCameraParams ), 1 );
        builder.CreateNamedBuffer( "PerFrame_CameraInfo", cameraInfo );

        BufferDescription meshInstanceInfo{};
        meshInstanceInfo.WithData( nullptr )
                        .WithUsage( BufferUsage::BUFFER_USAGE_SHADER_STORAGE )
                        .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_DYNAMIC )
                        .ForElement( sizeof( ShaderMaterialParams ), MAX_RENDERABLE_ENTITIES );
        builder.CreateNamedBuffer( "FinalCompositionPass_MeshInfo", meshInstanceInfo );

        // Prepare to have at least MAX_RENDERABLE_ENTITIES
        m_Meshes.resize( MAX_RENDERABLE_ENTITIES );
    }

    auto ShadingPass::SetDependencies( FrameGraphBuilder &builder ) -> void {
        builder.ReadBuffer( this, "AABBGenComp_CameraUBO" );
        builder.ReadBuffer( this, "AABBGenComp_Clusters" );
        builder.ReadBuffer( this, "LightCullingComp_LightsBuffer" );

        builder.ReadBuffer( this, "PerFrame_CameraInfo" );

        builder.WriteBuffer( this, "FinalCompositionPass_MeshInfo" );

        builder.ReadTexture( this, "FinalCompositionPass_ColorTarget" );
        builder.WriteTexture( this, "FinalCompositionPass_DepthTarget" );
    }

    auto ShadingPass::TraverseMeshList( CommandContext &context ) -> void {
        auto &registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for (auto &entity: renderables) {
            auto &tag{ registry.get<TagComponent>( entity ) };
            auto &transform{ registry.get<TransformComponent>( entity ) };
            auto &meshComponent{ registry.get<MeshComponent>( entity ) };
            auto &materialComp{ registry.get<MaterialComponent>( entity ) };

            MaterialHandle material{ materialComp.GetMaterial() };

            if (meshComponent.HasMesh() && !material.IsEmpty()) {
                MeshNode *meshNode{ meshComponent.GetMesh() };
                PBRMaterial *pbrMat{ dynamic_cast<PBRMaterial *>( material.GetRaw() ) };

                auto &[DrawIndexedState, ActiveEntities, Instances]{
                    m_MeshDrawState[meshNode]
                };

                ActiveEntities[tag.GetGUID()] = tag.IsActive();

                if (tag.IsActive()) {
                    ShaderMaterialParams &ubo{ Instances[tag.GetGUID()] };

                    ubo.Transform = transform.GetTransform();

                    ubo.Albedo = pbrMat->GetColor();
                    ubo.Factors.x = pbrMat->GetMetallicFactor();
                    ubo.Factors.y = pbrMat->GetRoughnessFactor();

                    ubo.AlbedoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                    ubo.NormalIndex = context.PushTexture( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
                    ubo.MetallicIndex = context.PushTexture( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
                    ubo.RoughnessIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                    ubo.AoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                }
            }
        }
    }

    auto ShadingPass::SetScene( Scene *scene ) -> void { m_Scene = scene; }

    auto ShadingPass::SetCamera( const Camera *camera ) -> void {
        m_FrameUBO.View = camera->GetViewMatrix();
        m_FrameUBO.Projection = camera->GetProjection();
        m_FrameUBO.CameraPosition = Vec4F{ camera->GetPosition(), 1.0f };
    }

    auto ShadingPass::EnableSkybox( bool enable ) -> void { m_UseSkybox = enable; }

    auto ShadingPass::SetClearColor( const Vec4F &vec ) -> void { m_ClearColor = vec; }

    auto ShadingPass::UploadInstanceData( CommandContext &context ) -> void {
        Size meshIndex{};
        Size firstInstance{};

        for (auto &[meshNode, instanceInfo]: m_MeshDrawState) {
            DrawIndexedState &drawState{ instanceInfo.InstanceDrawState };

            drawState.IndexBuffer = meshNode->GetIndexBuffer();

            if (drawState.VertexBuffers.empty()) { drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0 ); }

            Size drawCount{};
            for (const auto &[entityID, meshInstanceInfo]: instanceInfo.InstanceInfos) {
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

            context.DrawIndexed( drawState );
        }

        // Upload data
        context.FillBufferElement( "FinalCompositionPass_MeshInfo", m_Meshes.data(), sizeof( ShaderMaterialParams ), firstInstance );
    }

    auto ShadingPass::Execute( CommandContext &context ) -> void {
        context.BeginPass( this );

        LoadOp colorTargetLoadOP{ LoadOp::LOAD };
        if (!m_UseSkybox) {
            colorTargetLoadOP = LoadOp::CLEAR;
            context.SetClearColor( m_ClearColor );
        }

        PassRenderInfo renderInfo{
            .ColorLoadOp{ colorTargetLoadOP },
        };

        context.SetColorRenderTarget( "FinalCompositionPass_ColorTarget" );
        context.SetDepthRenderTarget( "FinalCompositionPass_DepthTarget" );

        context.BeginRender( renderInfo );

        context.BindPipeline( "FinalCompositionPass_Pipeline" );

        context.SetBufferBindSlot( SRGType::SRG_PerPass, "PerFrame_CameraInfo", 0 );
        context.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 1 );
        context.SetBufferBindSlot( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 2 );
        context.SetBufferBindSlot( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 3 );
        context.SetBufferBindSlot( SRGType::SRG_PerPass, "FinalCompositionPass_MeshInfo", 4 );

        context.BindResourceGroup( SRGType::SRG_Textures );
        context.BindResourceGroup( SRGType::SRG_PerPass );

        context.FillBuffer( "PerFrame_CameraInfo", std::addressof( m_FrameUBO ), sizeof( m_FrameUBO ) );

        context.SetViewport( 0, 0, 1920, 1080 );
        context.SetScissor( 0, 0, 1920, 1080 );

        TraverseMeshList( context );

        UploadInstanceData( context );

        context.EndRender();

        context.EndPass();
    }

}// namespace Mikoto
