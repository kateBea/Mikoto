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

    auto IBLPasses::RegisterPasses( FrameGraph &graph, GpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Prepare external resources
        if ( m_CubeMapSampler.IsEmpty() ) {
            SamplerDescription samplerDescription{ .CubeSampler{ true } };
            m_CubeMapSampler = device->CreateSampler( samplerDescription );
        }

        RegisterSkybox( graph );
        RegisterBRDFLut( graph );

        // These would ideally render and update the shadow maps
        // for dynamic shadow casters in a shadow texture atlas
        // TODO: investigate. Start by first integrating shadows for one dir light and proceed with atlas integration
        //https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/

        RegisterDirShadowMap( graph );
        RegisterSpotShadowMap( graph );
        RegisterPointShadowMap( graph );

        RegisterSkyboxRender( graph );

        RegisterIrradiance( graph );
        RegisterPrefilter( graph );
        RegisterShading( graph );

        // This pass will force transitions into
        // states we can display in the editor
        RegisterDebugViewsPass( graph );
    }

    auto IBLPasses::SetUseConvolutedCube( bool enable ) -> void {
        m_UseConvolutedCubeMap = enable;
    }

    auto IBLPasses::IsUsingConvolutedCube() const -> bool {
        return m_UseConvolutedCubeMap;
    }

    auto IBLPasses::SetClearColor( const Vec4F &color ) -> void {
        m_ClearColor = color;
    }

    auto IBLPasses::SetResolution( RenderResolution resolution ) -> void {
        m_Resolution = resolution;

        // Update passes
    }

    auto IBLPasses::SetEquirectangularMap( TextureHandle texture2D ) -> void {
        if (m_Skybox2D == texture2D) {
            return;
        }

        m_Skybox2D = texture2D;
        m_RequestUpdateSkybox = true;
    }

    auto IBLPasses::UseLDRCubeMap( bool value ) -> void {
        m_UsePrecomputedLDRCubeMap = value;
    }

    auto IBLPasses::IsUsingPrecomputedLDRCubeMap() const -> bool {
        return m_UsePrecomputedLDRCubeMap;
    }

    auto IBLPasses::RegisterIrradiance( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass<IrradiancePassData>(
                "IrradiancePass",

                [&]( FramePassBuilder &b, IrradiancePassData& p ) {
                    MKT_BEGIN_PROFILER_NAMED();
                    b.Create<TextureCube>( "IrradiancePass_ColorTargetCUBE", m_IrradianceDimensions, TextureFormat::RGBA32_FLOAT, m_IrradianceMipLevels );
                    b.Create<Texture>( "IrradiancePass_ColorTarget", m_IrradianceDimensions, m_IrradianceDimensions, TextureFormat::RGBA32_FLOAT, TextureUsage::COLOR );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Irradiance_Frag.sprv", ShaderStage::FRAGMENT );
                    b.Create<Pipeline>( "IrradiancePass_Pipeline",
                                        GraphicsPipelineDescription{
                                                .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                                                .VertexAttributesSpec{},
                                                .ColorAttachmentFormats{ TextureFormat::RGBA32_FLOAT } } );

                    b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget );
                    b.Write( "IrradiancePass_ColorTargetCUBE", FrameResourceState::TransferDst );

                    b.Write( "SkyboxRender_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                },
                [&]( CommandContext &ctx, FrameGraphBlackboard &blackboard ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    auto& irradianceFlags{ blackboard.Get<IrradiancePassData>() };
                    if (!irradianceFlags.Update) {
                        return;
                    }

                    if (m_UsePrecomputedLDRCubeMap && !m_CubeMap.IsEmpty()) {
                        ctx.BindImage( m_CubeMap, m_CubeMapSampler, 0 );
                    } else {
                        ctx.BindImage( "SkyboxRender_ColorTargetCUBE", m_CubeMapSampler, 0 );
                    }

                    for ( Size mipLevel{}; mipLevel < m_IrradianceMipLevels; mipLevel++ ) {
                        for ( UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face ) {
                            float viewPortWidth{  static_cast<float>(m_IrradianceDimensions * std::pow(0.5f, mipLevel)) };
                            float viewPortHeight{  static_cast<float>(m_IrradianceDimensions * std::pow(0.5f, mipLevel)) };

                            ctx.SetViewport( 0, 0, viewPortWidth, viewPortHeight );
                            ctx.SetScissor( 0, 0, viewPortWidth, viewPortHeight );

                            m_IrradianceParameters.DeltaTheta = 0.5f * Math::Constants::PI / 64.0;
                            m_IrradianceParameters.DeltaPhi = 2.0f * Math::Constants::PI / 180.0f;
                            m_IrradianceParameters.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * s_Matrices[face];

                            ctx.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                            ctx.BeginRender();
                            ctx.BindPipeline( "IrradiancePass_Pipeline" );

                            ctx.PushConstants( std::addressof( m_IrradianceParameters ), sizeof( m_IrradianceParameters ) );

                            ctx.Draw( 36 );

                            ctx.EndRender();

                            ctx.CopyToCube( "IrradiancePass_ColorTarget", "IrradiancePass_ColorTargetCUBE", mipLevel, face);
                        }
                    }

                    irradianceFlags.Update = false;
                } );
    }

    auto IBLPasses::RegisterPrefilter( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass<PrefilterPassData>(
                "PrefilterPass",

                [&]( FramePassBuilder &b, PrefilterPassData& ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    m_PrefilterMipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( m_PrefilterDimensions ) ) ) + 1;
                    b.Create<TextureCube>( "PrefilterPass_ColorTargetCUBE", m_PrefilterDimensions, TextureFormat::RGBA16_FLOAT, m_PrefilterMipLevels );
                    b.Create<Texture>( "PrefilterPass_ColorTarget", m_PrefilterDimensions, m_PrefilterDimensions, TextureFormat::RGBA16_FLOAT, TextureUsage::COLOR );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/Prefilter_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/Prefilter_Frag.sprv", ShaderStage::FRAGMENT );
                    b.Create<Pipeline>( "Prefilter_Pipeline", GraphicsPipelineDescription{
                                                                      .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                                                                      .VertexAttributesSpec{},
                                                                      .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT } } );

                    b.Write( "PrefilterPass_ColorTarget", FrameResourceState::RenderTarget );
                    b.Write( "PrefilterPass_ColorTargetCUBE", FrameResourceState::TransferDst );

                    b.Write( "SkyboxRender_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                },

                [&]( CommandContext &ctx, FrameGraphBlackboard & blackboard ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    auto& prefilterInfo{ blackboard.Get<PrefilterPassData>() };
                    if (!prefilterInfo.Update) {
                        return;
                    }

                    if (m_UsePrecomputedLDRCubeMap && !m_CubeMap.IsEmpty()) {
                        ctx.BindImage( m_CubeMap, m_CubeMapSampler, 0 );
                    } else {
                        ctx.BindImage( "SkyboxRender_ColorTargetCUBE", m_CubeMapSampler, 0 );
                    }

                    // Tweak
                    m_PrefilterParameters.NumSamples = 32;

                    for ( UInt32 mipLevel{}; mipLevel < m_PrefilterMipLevels; ++mipLevel ) {
                        m_PrefilterParameters.Roughness = static_cast<float>( mipLevel ) / static_cast<float>( m_PrefilterMipLevels - 1 );

                        for ( UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face ) {
                            float viewPortWidth{  static_cast<float>(m_PrefilterDimensions * std::pow(0.5f, mipLevel)) };
                            float viewPortHeight{   static_cast<float>(m_PrefilterDimensions * std::pow(0.5f, mipLevel)) };

                            ctx.SetViewport( 0, 0, viewPortWidth, viewPortHeight );
                            ctx.SetScissor( 0, 0, viewPortWidth, viewPortHeight );

                            m_PrefilterParameters.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * s_Matrices[face];

                            ctx.SetColorRenderTarget( "PrefilterPass_ColorTarget" );

                            ctx.BeginRender();
                            ctx.BindPipeline( "Prefilter_Pipeline" );

                            ctx.PushConstants( std::addressof( m_PrefilterParameters ), sizeof( m_PrefilterParameters ) );

                            ctx.Draw( 36 );

                            ctx.EndRender();

                            ctx.CopyToCube( "PrefilterPass_ColorTarget", "PrefilterPass_ColorTargetCUBE", mipLevel, face );
                        }
                    }

                    prefilterInfo.Update = false;
                } );
    }

    auto IBLPasses::RegisterBRDFLut( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "BRDFLut",
                []( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    // R16G16 is supported commonly
                    b.Create<Texture>( "BRDFLutPass_ColorTarget", 512, 512 , TextureFormat::RG16_FLOAT, TextureUsage::COLOR );

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

                    ctx.SetViewport( 0, 0, 512, 512 );
                    ctx.SetScissor( 0, 0, 512, 512 );

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

        // Update for shader contents
        m_IBLParameters.IsSkyboxActive = enable ? MKT_SHADER_TRUE : MKT_SHADER_FALSE;
    }

    auto IBLPasses::SetMaxReflectionLOD( float value ) -> void {
        m_IBLParameters.MaxReflectionLOD = value;
    }

    auto IBLPasses::SetMeshCulling( MeshCulling& cullingPass ) -> void {
        m_MeshCullingPass = std::addressof( cullingPass );
    }

    auto IBLPasses::SetLDRCubeMap( TextureHandle cubeMap ) -> void {
        if (m_CubeMap == cubeMap) {
            return;
        }

        m_CubeMap = cubeMap;
        m_RequestUpdateSkybox = true;
    }

    auto IBLPasses::SetExposure( float value ) -> void {
        m_IBLParameters.Exposure = value;
    }

    auto IBLPasses::SetGamma( float value ) -> void {
        m_IBLParameters.Gamma = value;
    }

    auto IBLPasses::RegisterSkyboxRender( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "SkyboxRender",

                [&]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();
                    b.Create<TextureCube>( "SkyboxRender_ColorTargetCUBE", 1024, TextureFormat::RGBA32_FLOAT, 1 );
                    b.Create<Texture>( "SkyboxRender_ColorTarget", 1024, 1024, TextureFormat::RGBA32_FLOAT, TextureUsage::COLOR );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/SkyboxRender_Vert.sprv", ShaderStage::VERTEX );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/SkyboxRender_Frag.sprv", ShaderStage::FRAGMENT );
                    b.Create<Pipeline>( "SkyboxRender_Pipeline",
                                        GraphicsPipelineDescription{
                                                .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                                                .VertexAttributesSpec{},
                                                .ColorAttachmentFormats{ TextureFormat::RGBA32_FLOAT } } );

                    b.Write( "SkyboxRender_ColorTarget", FrameResourceState::RenderTarget );
                    b.Write( "SkyboxRender_ColorTargetCUBE", FrameResourceState::TransferDst );
                },
                [&]( CommandContext &ctx, FrameGraphBlackboard & blackboard ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    if (!m_RequestUpdateSkybox) {
                        return;
                    }

                    if (m_Skybox2D.IsEmpty()) {
                        return;
                    }

                    if ( m_Skybox2DSampler.IsEmpty() ) {
                        m_Skybox2DSampler = ctx.CreateSampler( SamplerDescription{ } );
                    }

                    ctx.BindImage( m_Skybox2D, m_Skybox2DSampler, 0 );

                    for ( Size mipLevel{}; mipLevel < 1; mipLevel++ ) {
                        for ( UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face ) {
                            ctx.SetViewport( 0, 0, 1024, 1024 );
                            ctx.SetScissor( 0, 0, 1024, 1024 );

                            ctx.SetColorRenderTarget( "SkyboxRender_ColorTarget" );

                            ctx.BeginRender();
                            ctx.BindPipeline( "SkyboxRender_Pipeline" );

                            m_SkyboxRenderParameters.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * s_Matrices[face];
                            ctx.PushConstants( std::addressof( m_SkyboxRenderParameters ), sizeof( m_SkyboxRenderParameters ) );

                            ctx.Draw( 36 );

                            ctx.EndRender();

                            ctx.CopyToCube( "SkyboxRender_ColorTarget", "SkyboxRender_ColorTargetCUBE", mipLevel, face);
                        }
                    }

                    m_RequestUpdateSkybox = false;

                    // Request update for irradiance and prefilter cubes
                    auto& irradianceFlags{ blackboard.Get<IrradiancePassData>() };
                    auto& prefilterInfo{ blackboard.Get<PrefilterPassData>() };

                    irradianceFlags.Update = true;
                    prefilterInfo.Update = true;
                } );
    }

    auto IBLPasses::RegisterSkybox( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "Skybox",
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Texture>( "FinalShadingPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                    b.Create<Texture>( "FinalShadingPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                    b.Create<Buffer>( "IBL_Parameters", BufferUsage::UNIFORM, sizeof( IBLParameters ), 1 );

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
                    b.Write( "IBL_Parameters", FrameResourceState::UniformBuffer );

                    b.Read( "SkyboxRender_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "IrradiancePass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );

                    b.Use( SRGType::SRG_PerPass, "IBL_Parameters", 0 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    // Since other passes depend on this data we upload it so it is available in the shaders
                    ctx.UploadBuffer<IBLParameters>( "IBL_Parameters", m_IBLParameters );

                    if (!m_UseSkybox) {
                        return;
                    }

                    ctx.BindPipeline( "SkyboxPass_Pipeline" );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    if ( m_UseConvolutedCubeMap ) {
                        ctx.BindImage( "IrradiancePass_ColorTargetCUBE", m_CubeMapSampler, 1 );
                    } else {
                        if ( m_UsePrecomputedLDRCubeMap && !m_CubeMap.IsEmpty()) {
                            ctx.BindImage( m_CubeMap, m_CubeMapSampler, 1 );
                        } else {
                            ctx.BindImage( "SkyboxRender_ColorTargetCUBE", m_CubeMapSampler, 1 );
                        }
                    }

                    // Update contents
                    ctx.UploadBuffer<IBLParameters>( "IBL_Parameters", m_IBLParameters );

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

        m_IBLParameters.View = m_FrameUBO.View;
        m_IBLParameters.Projection = m_FrameUBO.Projection;
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

                    b.UseShader( "Resources/Shaders/vulkan-spirv/DirectionalShadowMap_Frag.sprv", ShaderStage::FRAGMENT );
                    b.UseShader( "Resources/Shaders/vulkan-spirv/DirectionalShadowMap_Vert.sprv", ShaderStage::VERTEX );

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

    auto IBLPasses::RegisterPointShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "PointLightShadowMapPass",
                []( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                } );
    }

    auto IBLPasses::RegisterSpotShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
                "SpotLightShadowMapPass",
                []( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
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

                    b.Read( "InfiniteGrid_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                    b.Read( "GBuffer_Position", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "GBuffer_Normal", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "GBuffer_Color", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "Wireframe_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
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
                        .PipelineCullMode{ CullMode::NONE },
                    };

                    b.UseShader( "Resources/Shaders/vulkan-spirv/PBR_Instanced_Vert.sprv", ShaderStage::VERTEX )
                        .UseShader( "Resources/Shaders/vulkan-spirv/PBR_Instanced_Frag.sprv", ShaderStage::FRAGMENT )
                        .Create<Pipeline>( "FinalCompositionPass_Pipeline", graphicsDesc );

                    b.Read( "AABBGenComp_CameraUBO", FrameResourceState::UnorderedAccess )
                        .Read( "AABBGenComp_Clusters", FrameResourceState::UnorderedAccess )
                        .Read( "LightCullingComp_LightsBuffer", FrameResourceState::UnorderedAccess )
                        .Read( "FinalShadingPass_DepthTarget", FrameResourceState::DepthWrite)
                        .Read( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget );

                    b.Read( "PrefilterPass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "IrradiancePass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                    b.Read( "BRDFLutPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                    b.Read( "FinalCompositionPass_MeshInfo", FrameResourceState::UnorderedAccess );
                    b.Write( "FinalCompositionPass_CameraInfo", FrameResourceState::UniformBuffer );

                    b.Read( "IBL_Parameters", FrameResourceState::UniformBuffer );

                    b.Use( SRGType::SRG_PerPass, "FinalCompositionPass_CameraInfo", 0 )
                        .Use( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 1 )
                        .Use( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 2 )
                        .Use( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 3 )
                        .Use( SRGType::SRG_PerPass, "FinalCompositionPass_MeshInfo", 4 )
                        .Use( SRGType::SRG_Textures );

                    b.Use( SRGType::SRG_PerPass, "IBL_Parameters", 5 );
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

                    if (m_BRDFLutSampler.IsEmpty()) {
                        m_BRDFLutSampler = ctx.CreateSampler( SamplerDescription{} );
                    }

                    ctx.BindImage( "PrefilterPass_ColorTargetCUBE", m_CubeMapSampler, 7 );
                    ctx.BindImage( "IrradiancePass_ColorTargetCUBE", m_CubeMapSampler, 8 );
                    ctx.BindImage( "BRDFLutPass_ColorTarget", m_BRDFLutSampler, 6 );

                    ctx.UploadBuffer<ShaderCameraParams>( "FinalCompositionPass_CameraInfo", m_FrameUBO );

                    ctx.BeginRender( renderInfo );

                    ctx.SetViewport( 0, 0, 1920, 1080 );
                    ctx.SetScissor( 0, 0, 1920, 1080 );

                    m_MeshCullingPass->DrawInstances( ctx );

                    ctx.EndRender();
                } );

        //graph.SetNodeExecutionPolicy( "FinalShading", FramePassExecutionPolicy::ONCE );
    }
} // namespace Mikoto