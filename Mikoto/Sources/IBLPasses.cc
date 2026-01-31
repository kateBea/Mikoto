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

#include <memory>

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

        RegisterDirShadowMap( graph );

        RegisterIrradiance( graph );
        RegisterPrefilter( graph );
        RegisterShading( graph );

        // This pass will force transitions into
        // states we can display in the editor
        RegisterDebugViewsPass( graph );
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
                    b.Create<Buffer>( "IrradiancePass_CameraInfo", BufferUsage::UNIFORM, sizeof( IrradianceCamInfo ), 1 );
                    b.Create<Buffer>( "IrradiancePass_Parameters", BufferUsage::UNIFORM, sizeof( IrradianceParameters ), 1 );

                    // Create target
                    m_IrradianceMipLevels = 1;
                    b.Create<TextureCube>( "IrradiancePass_ColorTarget", m_IrradianceDimensions, TextureFormat::RGBA16_FLOAT, m_IrradianceMipLevels );

                    // Create pipeline
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Vert.sprv", ShaderStage::VERTEX )
                        .UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Frag.sprv", ShaderStage::FRAGMENT )
                        .Create<Pipeline>( "IrradiancePass_Pipeline", GraphicsPipelineDescription{
                            .VertexAttributesSpec{},
                            .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT }
                        } );

                    b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget )
                        .Write( "IrradiancePass_CameraInfo", FrameResourceState::UniformBuffer)
                        .Write( "IrradiancePass_Parameters", FrameResourceState::UniformBuffer);

                    b.Use( SRGType::SRG_PerPass, "IrradiancePass_CameraInfo", 0 )
                        .Use( SRGType::SRG_PerPass, "IrradiancePass_Parameters", 1 );
                },

                [&]( CommandContext &ctx, FrameGraphBlackboard & ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    // The skybox is bound once
                    ctx.BindImage( m_CubeMap, m_CubeMapSampler, 2 );

                    m_IrradianceParameters.DeltaTheta = 0.5f * Math::Constants::PI / 64.0;
                    m_IrradianceParameters.DeltaPhi = 2.0f * Math::Constants::PI / 180.0f;

                    for ( UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face ) {
                        ctx.SetViewport( 0, 0, m_IrradianceDimensions, m_IrradianceDimensions );
                        ctx.SetScissor( 0, 0, m_IrradianceDimensions, m_IrradianceDimensions );

                        ctx.UploadBuffer<IrradianceParameters>( "IrradiancePass_CameraInfo", m_IrradianceParameters );

                        m_IrradianceCameraInfo.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ),
                                                                       1.0f, 0.1f, 512.0f ) *
                                                     s_Matrices[face];
                        ctx.UploadBuffer<IrradianceCamInfo>( "IrradiancePass_CameraInfo", m_IrradianceCameraInfo );

                        ctx.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                        ctx.BeginRender();
                        ctx.BindPipeline( "IrradiancePass_Pipeline" );
                        //ctx.Draw( 36 ); // Crashes
                        ctx.EndRender();
                    }

                    ctx.EndPass();
                } );

        graph.SetNodeExecutionPolicy( "IrradiancePass", FramePassExecutionPolicy::ONCE );
    }

    auto IBLPasses::RegisterPrefilter( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "PrefilterPass",

                [&]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "PrefilterPass_CameraInfo", BufferUsage::UNIFORM, sizeof( PrefilterCamInfo ), 1 );
                    b.Create<Buffer>( "PrefilterPass_Parameters", BufferUsage::UNIFORM, sizeof( PrefilterParameters ), 1 );

                    m_PrefilterMipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( m_PrefilterDimensions ) ) ) + 1;
                    b.Create<TextureCube>( "Prefilter_ColorTarget", m_PrefilterDimensions, TextureFormat::RGBA32_FLOAT, m_PrefilterMipLevels );

                    // Create pipeline
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Prefilter_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Prefilter_Frag.sprv", ShaderStage::FRAGMENT );
                    b.Create<Pipeline>( "Prefilter_Pipeline", GraphicsPipelineDescription{
                        .VertexAttributesSpec{},
                        .ColorAttachmentFormats{ TextureFormat::RGBA32_FLOAT }
                    } );

                    b.Write( "Prefilter_ColorTarget", FrameResourceState::RenderTarget )
                       .Write( "PrefilterPass_CameraInfo", FrameResourceState::UniformBuffer)
                       .Write( "PrefilterPass_Parameters", FrameResourceState::UniformBuffer);

                   b.Use( SRGType::SRG_PerPass, "PrefilterPass_CameraInfo", 0 )
                       .Use( SRGType::SRG_PerPass, "PrefilterPass_Parameters", 1 );
                },

                [&]( CommandContext &ctx, FrameGraphBlackboard & ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    // The skybox is bound once
                    ctx.BindImage( m_CubeMap, m_CubeMapSampler, 2 );

                    for ( UInt32 mip = 0; mip < m_PrefilterMipLevels; ++mip ) {
                        for ( UInt32 face = 0; face < MAX_CUBE_MAP_FACES; ++face ) {
                            ctx.SetViewport( 0, 0, m_PrefilterDimensions, m_PrefilterDimensions );
                            ctx.SetScissor( 0, 0, m_PrefilterDimensions, m_PrefilterDimensions );

                            ctx.BindPipeline( "Prefilter_Pipeline" );

                            ctx.SetColorRenderTarget( "Prefilter_ColorTarget" );

                            ctx.BeginRender();

                            // Draw

                            ctx.EndRender();
                        }
                    }

                    ctx.EndPass();
                } );

        graph.SetNodeExecutionPolicy( "PrefilterPass", FramePassExecutionPolicy::ONCE );
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

                    b.Write( "BRDFLutPass_ColorTarget", FrameResourceState::RenderTarget );
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

    auto IBLPasses::SetMeshCulling( MeshCulling& cullingPass ) -> void {
        m_MeshCullingPass = std::addressof( cullingPass );
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

                    b.Write( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget );
                    b.Write( "FinalShadingPass_DepthTarget", FrameResourceState::DepthWrite );
                    b.Write( "SkyboxPass_CameraInfo", FrameResourceState::UniformBuffer );

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

    auto IBLPasses::RegisterDirShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "DirectionalShadowMapPass",
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Texture>( "DirectionalShadowMapPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "DirectionalShadowMapPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT, TextureUsage::DEPTH );

                    b.Create<Buffer>( "DirectionalShadowMapPass_CameraInfo", BufferUsage::UNIFORM, sizeof( DirectionalShadowMapCameraInfo ), 1 );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/DirectionalShadowMap_Frag.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/DirectionalShadowMap_Vert.sprv", ShaderStage::FRAGMENT );

                    GraphicsPipelineDescription graphicsDesc{
                        .DepthTest{ true },
                        .DepthWrite{ true },
                        .DepthAttachmentFormat{ TextureFormat::D32_FLOAT }
                    };
                    b.Create<Pipeline>( "DirectionalShadowMapPass_Pipeline", graphicsDesc );

                    b.Write( "DirectionalShadowMapPass_ColorTarget", FrameResourceState::RenderTarget );
                    b.Write( "DirectionalShadowMapPass_DepthTarget", FrameResourceState::DepthWrite );

                    b.Use( SRGType::SRG_PerPass, "DirectionalShadowMapPass_CameraInfo", 0 );
                    b.Use( SRGType::SRG_PerPass, "FinalCompositionPass_MeshInfo", 1 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    MKT_ASSERT( m_MeshCullingPass, "Mesh culling must be valid" );

                    const auto dimensions{ InferDimensions( m_Resolution ) };

                    ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                    ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                    ctx.SetColorRenderTarget( "DirectionalShadowMapPass_ColorTarget" );
                    ctx.SetDepthRenderTarget( "DirectionalShadowMapPass_DepthTarget" );

                    ctx.BeginRender();

                    ctx.BindPipeline( "DirectionalShadowMapPass_Pipeline" );

                    auto &registry{ m_Scene->GetRegistry() };
                    auto lights{ registry.view<TransformComponent, LightComponent>() };

                    for (const auto &light : lights) {
                        auto &lightComp{ registry.get<LightComponent>( light ) };

                        if (lightComp.IsTypeActive( LightType::DIRECTIONAL_LIGHT_TYPE )) {
                            auto &transform{ registry.get<TransformComponent>( light ) };

                            constexpr float zNear{ 0.01f };
                            constexpr float zFar{ 2000.0f };
                            constexpr float degreesFOV{ 45.0f };

                            m_DirectionalShadowMapCameraInfo.LightView = glm::lookAt(transform.GetTranslation(), glm::vec3(0.0f), glm::vec3(0, 1, 0));
                            m_DirectionalShadowMapCameraInfo.LightProjection = glm::perspective(glm::radians(degreesFOV), 1.0f, zNear, zFar);

                            ctx.UploadBuffer<DirectionalShadowMapCameraInfo>( "DirectionalShadowMapPass_CameraInfo", m_DirectionalShadowMapCameraInfo );

                            m_MeshCullingPass->DrawInstances( ctx );
                        }
                    }

                    ctx.EndRender();
                } );
    }

    auto IBLPasses::RegisterDebugViewsPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // This has to be the very last pass

        graph.RegisterPass(
                "DebugViewsPass",
                []( FramePassBuilder &b ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                    b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "DirectionalShadowMapPass_DepthTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "FinalCompositionPass_MeshInfo", FrameResourceState::UnorderedAccess );
                    b.Read( "FinalCompositionPass_CameraInfo", FrameResourceState::UniformBuffer );
                    b.Read( "FinalCompositionPass_CameraInfo", FrameResourceState::UniformBuffer );

                    b.Read( "TextRenderPass_FontParams", FrameResourceState::UniformBuffer );
                    b.Read( "TextRenderPass_TextRenderParams", FrameResourceState::UniformBuffer );
                },
                []( CommandContext &, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                });
    }

    auto IBLPasses::RegisterShading( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "FinalShading",
                []( FramePassBuilder &b ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "FinalCompositionPass_CameraInfo", BufferUsage::UNIFORM, sizeof( ShaderCameraParams ), 1 );

                    GraphicsPipelineDescription graphicsDesc{
                        .DepthTest{ true },
                        .DepthWrite{ true },
                        .AlphaBlending{ true },
                        .PipelineCullMode{ CullMode::CULL_BACK },
                    };

                    b.UseShader( "Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX )
                        .UseShader( "Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT )
                        .Create<Pipeline>( "FinalCompositionPass_Pipeline", graphicsDesc );

                    b.Read( "AABBGenComp_CameraUBO", FrameResourceState::UnorderedAccess )
                        .Read( "AABBGenComp_Clusters", FrameResourceState::UnorderedAccess )
                        .Read( "LightCullingComp_LightsBuffer", FrameResourceState::UnorderedAccess )
                        .Read( "FinalShadingPass_DepthTarget", FrameResourceState::DepthWrite)
                        .Read( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget );

                    b.Read( "FinalCompositionPass_MeshInfo", FrameResourceState::UnorderedAccess );
                    b.Write( "FinalCompositionPass_CameraInfo", FrameResourceState::UniformBuffer );

                    b.Use( SRGType::SRG_PerPass, "FinalCompositionPass_CameraInfo", 0 )
                        .Use( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 1 )
                        .Use( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 2 )
                        .Use( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 3 )
                        .Use( SRGType::SRG_PerPass, "FinalCompositionPass_MeshInfo", 4 )
                        .Use( SRGType::SRG_Textures );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    MKT_ASSERT( m_MeshCullingPass, "Mesh culling must be valid" );

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

                    m_MeshCullingPass->DrawInstances( ctx );

                    ctx.EndRender();
                } );
    }
} // namespace Mikoto