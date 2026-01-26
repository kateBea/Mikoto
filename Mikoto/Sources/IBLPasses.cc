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

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/FramePassResource.hh>
#include <Renderer/Core/FrameGraphBlackboard.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/IBLPasses.hh>

namespace Mikoto {

    IBLPasses::IBLPasses( RenderResolution resolution )
        : m_Resolution{ resolution } {}

    auto IBLPasses::SetScene( Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto IBLPasses::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterSkybox( graph );
        RegisterBRDFLut( graph );

        //RegisterPrefilter( graph );
        //RegisterIrradiance( graph );
        RegisterShading( graph );
    }

    auto IBLPasses::SetClearColor( const Vec4F &color ) -> void {
        m_ClearColor = color;
    }

    auto IBLPasses::SetResolution( RenderResolution resolution ) -> void {
        m_Resolution = resolution;

        // Update passes
    }

    auto IBLPasses::RegisterIrradiance( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "IrradiancePass",

                [&]( FramePassBuilder &b) {
                    MKT_BEGIN_PROFILER_NAMED();

                    // Create target
                    m_IrradianceMipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( m_IrradianceMipLevels ) ) ) + 1;
                    b.Create<TextureCube>( "IrradiancePass_ColorTarget", m_IrradianceDimensions, TextureFormat::RGBA32_FLOAT, m_IrradianceMipLevels );

                    // Create pipeline
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Vert.sprv", ShaderStage::VERTEX )
                        .UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Frag.sprv", ShaderStage::FRAGMENT )
                        .Create<Pipeline>( "IrradiancePass_Pipeline", GraphicsPipelineDescription{ .VertexAttributesSpec{} } );

                    b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget_Color )
                        .Write( "IrradiancePass_CameraInfo", FrameResourceState::ShaderResource_Read )
                        .Write( "IrradiancePass_Parameters", FrameResourceState::ShaderResource_Read );

                    b.Read( "SkyboxPass_TextureCube", FrameResourceState::ShaderResource_Read );
                },

                [&]( CommandContext &ctx, FrameGraphBlackboard & ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    ctx.BindPipeline( "IrradiancePass_Pipeline" );
                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    // The skybox is bound once
                    ctx.BindImage( m_CubeMap, m_CubeMapSampler, 1 );

                    for (UInt32 mip = 0; mip < m_IrradianceDimensions; ++mip) {
                        for (UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face) {
                            ctx.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                            // Update face data

                            ctx.BeginRender();

                            // Draw

                            ctx.EndRender();
                        }
                    }

                    ctx.EndPass();
                } );

        graph.SetNodeExecutionPolicy( "BRDFLut", FramePassExecutionPolicy::ON_CHANGE );
    }

    auto IBLPasses::RegisterPrefilter( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "PrefilterPass",

                [&]( FramePassBuilder &b) {
                    MKT_BEGIN_PROFILER_NAMED();

                    m_PrefilterMipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( m_PrefilterDimensions ) ) ) + 1;
                    b.Create<TextureCube>( "IrradiancePass_ColorTarget", m_PrefilterDimensions, TextureFormat::RGBA32_FLOAT, m_PrefilterMipLevels );

                    // Create pipeline
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Prefilter_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Prefilter_Frag.sprv", ShaderStage::FRAGMENT );
                    b.Create<Pipeline>( "Prefilter_Pipeline", GraphicsPipelineDescription{ .VertexAttributesSpec{} } );

                    b.Write( "Prefilter_ColorTarget", FrameResourceState::ShaderResource_Read );
                    b.Read( "SkyboxPass_TextureCube", FrameResourceState::ShaderResource_Read );
                },

                [&]( CommandContext &ctx, FrameGraphBlackboard & ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    ctx.BindPipeline( "IrradiancePass_Pipeline" );
                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    // The skybox is bound once
                    ctx.BindImage( m_CubeMap, m_CubeMapSampler, 1 );

                    for (UInt32 mip = 0; mip < m_IrradianceMipLevels; ++mip) {
                        for (UInt32 face = 0; face < MAX_CUBE_MAP_FACES; ++face) {
                            ctx.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                            // Update face data

                            ctx.BeginRender();

                            // Draw

                            ctx.EndRender();
                        }
                    }

                    ctx.EndPass();
                } );

        graph.SetNodeExecutionPolicy( "BRDFLut", FramePassExecutionPolicy::ON_CHANGE );
    }

    auto IBLPasses::RegisterBRDFLut( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "BRDFLut",
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    // R16G16 is supported commonly
                    b.Create<Texture>( "BRDFLutPass_ColorTarget", m_Resolution, TextureFormat::RG16_FLOAT, TextureUsage::COLOR );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/BRDFLut_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/BRDFLut_Frag.sprv", ShaderStage::FRAGMENT );

                    b.Create<Pipeline>( "BRDFLutPass_Pipeline", GraphicsPipelineDescription{
                                            .VertexAttributesSpec{},
                                            .ColorAttachmentFormats{ TextureFormat::RG16_FLOAT }
                                        } );

                    b.Write( "BRDFLutPass_ColorTarget", FrameResourceState::ShaderResource_Read );
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    ctx.BindPipeline( "BRDFLutPass_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.SetClearColor( { 0.0f, 0.0f, 0.0f, 1.0f } );

                    ctx.SetColorRenderTarget( "BRDFLutPass_ColorTarget" );

                    ctx.BeginRender();

                    ctx.Draw( 3 );

                    ctx.EndRender();
                } );

        // After a call to RegisterPass the node is guaranteed to exist already
        // BRDFLut is quite expensive to run every frame so we limit it to a single execution
        graph.SetNodeExecutionPolicy( "BRDFLut", FramePassExecutionPolicy::ONCE );
    }

    auto IBLPasses::EnableSkybox( bool enable ) -> void {
        m_UseSkybox = enable;
    }

    auto IBLPasses::SetCubeMap( TextureHandle cubeMap ) -> void {
        m_CubeMap = cubeMap;
    }

    auto IBLPasses::SetExposure( float value ) -> void {
        m_SkyboxUBO.Exposure = value;
    }

    auto IBLPasses::SetGamma( float value ) -> void {
        m_SkyboxUBO.Gamma = value;
    }

    auto IBLPasses::RegisterSkybox( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "Skybox",
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Texture>( "FinalShadingPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "FinalShadingPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    b.Create<Buffer>( "SkyboxPass_CameraInfo", BufferUsage::UNIFORM, sizeof( SkyboxUBO ), 1 );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/Skybox_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Skybox_Frag.sprv", ShaderStage::FRAGMENT );

                    GraphicsPipelineDescription graphicsDesc{
                        .DepthTest{ false },
                        .DepthWrite{ false },
                        .AlphaBlending{ false },
                        .PipelineCullMode{ CullMode::NONE },
                        .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                    };
                    b.Create<Pipeline>( "SkyboxPass_Pipeline", graphicsDesc );

                    b.Write( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "FinalShadingPass_DepthTarget", FrameResourceState::ShaderResource_Read );
                    b.Write( "SkyboxPass_CameraInfo", FrameResourceState::Transfer_Dst );

                    b.Use( SRGType::SRG_PerPass, "SkyboxPass_CameraInfo", 0 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    if (!m_UseSkybox || m_CubeMap.IsEmpty()) { return; }

                    if (m_CubeMapSampler.IsEmpty()) {
                        SamplerDescription samplerDescription{ .CubeSampler{ true } };
                        m_CubeMapSampler = ctx.CreateSampler( samplerDescription );
                    }

                    ctx.BindPipeline( "SkyboxPass_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    ctx.BindImage( m_CubeMap, m_CubeMapSampler, 1 );
                    ctx.UploadBuffer<SkyboxUBO>( "SkyboxPass_CameraInfo", m_SkyboxUBO );

                    ctx.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );
                    ctx.SetColorRenderTarget( "FinalShadingPass_ColorTarget" );
                    ctx.SetDepthRenderTarget( "FinalShadingPass_DepthTarget" );

                    ctx.BeginRender();

                    ctx.Draw( 36 );

                    ctx.EndRender();
                } );
    }

    auto IBLPasses::SetCamera( const Camera *camera ) -> void {
        m_FrameUBO.View = camera->GetViewMatrix();
        m_FrameUBO.Projection = camera->GetProjection();
        m_FrameUBO.CameraPosition = Vec4F{ camera->GetPosition(), 1.0f };

        m_SkyboxUBO.View = m_FrameUBO.View;
        m_SkyboxUBO.Projection = m_FrameUBO.Projection;
    }

    auto IBLPasses::RegisterShading( FrameGraph &graph ) -> void {
        // Prepare buffer of meshes
        m_Meshes.resize( MAX_RENDERABLE_ENTITIES );

        graph.RegisterPass(
                "FinalShading",
                []( FramePassBuilder &b ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "FinalCompositionPass_CameraInfo", BufferUsage::UNIFORM, sizeof( ShaderCameraParams ), 1 )
                        .Create<Buffer>( "FinalCompositionPass_MeshInfo", BufferUsage::SSBO, sizeof( ShaderMaterialParams ), MAX_RENDERABLE_ENTITIES );

                    GraphicsPipelineDescription graphicsDesc{
                        .DepthTest{ true },
                        .DepthWrite{ true },
                        .AlphaBlending{ true },
                        .PipelineCullMode{ CullMode::CULL_BACK },
                    };

                    b.UseShader( "Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX )
                        .UseShader( "Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT )
                        .Create<Pipeline>( "FinalCompositionPass_Pipeline", graphicsDesc );

                    b.Read( "AABBGenComp_CameraUBO" )
                        .Read( "AABBGenComp_Clusters" )
                        .Read( "LightCullingComp_LightsBuffer" )
                        .Read( "FinalCompositionPass_CameraInfo" )
                        .Read( "FinalShadingPass_DepthTarget" )
                        .Read( "FinalShadingPass_ColorTarget" );

                    b.Write( "FinalCompositionPass_MeshInfo" );

                    b.Use( SRGType::SRG_PerPass, "FinalCompositionPass_CameraInfo", 0 )
                        .Use( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 1 )
                        .Use( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 2 )
                        .Use( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 3 )
                        .Use( SRGType::SRG_PerPass, "FinalCompositionPass_MeshInfo", 4 )
                        .Use( SRGType::SRG_Textures );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    ctx.BindPipeline( "FinalCompositionPass_Pipeline" );

                    ctx.SetClearColor( { 0.5f, 0.2f, 0.3f, 1.0f } );
                    ctx.SetColorRenderTarget( "FinalShadingPass_ColorTarget" );
                    ctx.SetDepthRenderTarget( "FinalShadingPass_DepthTarget" );

                    LoadOp colorTargetLoadOP{ LoadOp::LOAD };
                    if (!m_UseSkybox) {
                        colorTargetLoadOP = LoadOp::CLEAR;
                        ctx.SetClearColor( m_ClearColor );
                    }

                    PassRenderInfo renderInfo{
                        .ColorLoadOp{ colorTargetLoadOP },
                    };

                    ctx.UploadBuffer<ShaderCameraParams>( "FinalCompositionPass_CameraInfo", m_FrameUBO );

                    ctx.BeginRender( renderInfo );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    TraverseMeshList( ctx );
                    UploadInstanceData( ctx );

                    ctx.EndRender();
                } );
    }

    auto IBLPasses::UploadInstanceData( CommandContext &context ) -> void {
        Size meshIndex{};
        Size firstInstance{};

        for (auto &[meshNode, instanceInfo]: m_MeshDrawState) {
            DrawIndexedState &drawState{ instanceInfo.InstanceDrawState };

            drawState.IndexBuffer = meshNode->GetIndexBuffer();

            if (drawState.VertexBuffers.empty()) {
                drawState.VertexBuffers.emplace_back( meshNode->GetVertexBuffer(), 0 );
            }

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
        context.UploadBufferData( "FinalCompositionPass_MeshInfo", m_Meshes.data(), sizeof( ShaderMaterialParams ), firstInstance );
    }

    auto IBLPasses::TraverseMeshList( CommandContext &context ) -> void {
        auto &registry{ m_Scene->GetRegistry() };
        auto renderables{ registry.view<TagComponent, TransformComponent, MaterialComponent, MeshComponent>() };

        for (auto &entity: renderables) {
            auto &tag{ registry.get<TagComponent>( entity ) };
            auto &transform{ registry.get<TransformComponent>( entity ) };
            auto &meshComponent{ registry.get<MeshComponent>( entity ) };
            auto &materialComp{ registry.get<MaterialComponent>( entity ) };

            if (meshComponent.HasMesh() && materialComp.HasMaterial()) {
                MeshNode *meshNode{ meshComponent.GetMesh() };
                PBRMaterial *pbrMat{ materialComp.GetMaterial().Dynamic<PBRMaterial>() };

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
                    ubo.Factors.z = pbrMat->GetAoFactor();

                    ubo.AlbedoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ALBEDO_TEXTURE ) );
                    ubo.NormalIndex = context.PushTexture( pbrMat->GetTextureType( MapType::NORMAL_TEXTURE ) );
                    ubo.MetallicIndex = context.PushTexture( pbrMat->GetTextureType( MapType::METALLIC_TEXTURE ) );
                    ubo.RoughnessIndex = context.PushTexture( pbrMat->GetTextureType( MapType::ROUGHNESS_TEXTURE ) );
                    ubo.AoIndex = context.PushTexture( pbrMat->GetTextureType( MapType::AMBIENT_OCCLUSION_TEXTURE ) );
                }
            }
        }
    }
} // namespace Mikoto