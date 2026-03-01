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

        // These would ideally render and update the shadow maps
        // for dynamic shadow casters in a shadow texture atlas
        // TODO: investigate. Start by first integrating shadows for one dir light and proceed with atlas integration
        //https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/

        // Shadow mapping
        RegisterDirShadowMap( graph );
        RegisterSpotShadowMap( graph );
        RegisterPointShadowMap( graph );

        // IBL
        RegisterSkybox( graph );
        RegisterBRDFLut( graph );

        RegisterPrefilter( graph );
        RegisterIrradiance( graph );
        RegisterSkyboxRender( graph );

        RegisterShading( graph );

        //RegisterMetalRoughnessPBR( graph );

        // This pass will force transitions into
        // states we can display in the editor
        RegisterDebugViewsPass( graph );
    }

    auto IBLPasses::UseConvolutedCube( bool enable ) -> void {
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
        if (m_Equirectangular == texture2D) {
            return;
        }

        m_Equirectangular = texture2D;
        m_RequestUpdateSkybox = true;
    }

    auto IBLPasses::UseCubeMap( bool value ) -> void {
        m_UseCubeMap = value;
    }

    auto IBLPasses::IsUsingCubeMap() const -> bool {
        return m_UseCubeMap;
    }

    auto IBLPasses::RegisterIrradiance( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass<IrradiancePassData>(
            "IrradiancePass",

            [this]( FramePassBuilder &b, IrradiancePassData& p ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "IrradiancePass_ColorTargetCUBE", m_IrradianceDimensions, TextureFormat::RGBA32_FLOAT, m_IrradianceMipLevels );
                b.CreateTexture( "IrradiancePass_ColorTarget", m_IrradianceDimensions, m_IrradianceDimensions, TextureFormat::RGBA32_FLOAT, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/Irradiance_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/Irradiance_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( "IrradiancePass_Pipeline",
                    GraphicsPipelineDescription{
                        .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                        .ColorAttachmentFormats{ TextureFormat::RGBA32_FLOAT } } );

                b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "IrradiancePass_ColorTargetCUBE", FrameResourceState::TransferDst );

                b.Read( "SkyboxRender_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard &blackboard ) {
                MKT_BEGIN_PROFILER_NAMED();

                auto& irradianceFlags{ blackboard.Get<IrradiancePassData>() };
                if (!irradianceFlags.Update) {
                    return;
                }

                 if ( m_UseCubeMap && !m_CubeMap.IsEmpty() ) {
                    ctx.BindImageSampler( ResourceGroup::DynamicSamplers, m_CubeMap, m_CubeMapSampler, ResourceSlot::Slot_0 );
                } else {
                     ctx.BindImageSampler( ResourceGroup::DynamicSamplers, "SkyboxRender_ColorTargetCUBE", m_CubeMapSampler, ResourceSlot::Slot_0 );
                }

                for ( Size mipLevel{}; mipLevel < m_IrradianceMipLevels; mipLevel++ ) {
                    for ( UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face ) {
                        float viewPortWidth{  static_cast<float>(m_IrradianceDimensions * std::pow(0.5f, mipLevel)) };
                        float viewPortHeight{  static_cast<float>(m_IrradianceDimensions * std::pow(0.5f, mipLevel)) };

                        constexpr bool flip{ false };
                        ctx.SetViewport( 0, 0, viewPortWidth, viewPortHeight, flip );
                        ctx.SetScissor( 0, 0, viewPortWidth, viewPortHeight );

                        m_IrradianceParameters.DeltaTheta = 0.5f * Math::Constants::PI / 64.0;
                        m_IrradianceParameters.DeltaPhi = 2.0f * Math::Constants::PI / 180.0f;
                        m_IrradianceParameters.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * s_Matrices[face];

                        ctx.SetColorRenderTarget( "IrradiancePass_ColorTarget" );

                        ctx.BeginRender();
                        ctx.BindPipeline( "IrradiancePass_Pipeline" );

                        ctx.PushConstants( std::addressof( m_IrradianceParameters ), sizeof( m_IrradianceParameters ) );

                        ctx.DrawIndexed( m_DrawBoxIndexedState );

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
            [this]( FramePassBuilder &b, PrefilterPassData& ) {
                MKT_BEGIN_PROFILER_NAMED();

                m_PrefilterMipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( m_PrefilterDimensions ) ) ) + 1;
                b.CreateTexture( "PrefilterPass_ColorTargetCUBE", m_PrefilterDimensions, TextureFormat::RGBA16_FLOAT, m_PrefilterMipLevels );
                b.CreateTexture( "PrefilterPass_ColorTarget", m_PrefilterDimensions, m_PrefilterDimensions, TextureFormat::RGBA16_FLOAT, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/Prefilter_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/Prefilter_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( "Prefilter_Pipeline", 
                    GraphicsPipelineDescription{
                        .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                        .ColorAttachmentFormats{ TextureFormat::RGBA16_FLOAT } } );

                b.Write( "PrefilterPass_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "PrefilterPass_ColorTargetCUBE", FrameResourceState::TransferDst );

                b.Read( "SkyboxRender_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
            },

            [this]( CommandContext &ctx, FrameGraphBlackboard & blackboard ) {
                MKT_BEGIN_PROFILER_NAMED();

                auto& prefilterInfo{ blackboard.Get<PrefilterPassData>() };
                if (!prefilterInfo.Update) {
                    return;
                }

                if (m_UseCubeMap && !m_CubeMap.IsEmpty()) {
                    ctx.BindImageSampler( ResourceGroup::DynamicSamplers, m_CubeMap, m_CubeMapSampler, ResourceSlot::Slot_0 );
                } else {
                    ctx.BindImageSampler( ResourceGroup::DynamicSamplers, "SkyboxRender_ColorTargetCUBE", m_CubeMapSampler, ResourceSlot::Slot_0 );
                }

                // Tweak
                m_PrefilterParameters.NumSamples = 1024;

                for ( UInt32 mipLevel{}; mipLevel < m_PrefilterMipLevels; ++mipLevel ) {
                    m_PrefilterParameters.Roughness = static_cast<float>( mipLevel ) / static_cast<float>( m_PrefilterMipLevels - 1 );

                    for ( UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face ) {
                        float viewPortWidth{  static_cast<float>(m_PrefilterDimensions * std::pow(0.5f, mipLevel)) };
                        float viewPortHeight{   static_cast<float>(m_PrefilterDimensions * std::pow(0.5f, mipLevel)) };

                        constexpr bool flip{ false };
                        ctx.SetViewport( 0, 0, viewPortWidth, viewPortHeight, flip );
                        ctx.SetScissor( 0, 0, viewPortWidth, viewPortHeight );

                        m_PrefilterParameters.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * s_Matrices[face];

                        ctx.SetColorRenderTarget( "PrefilterPass_ColorTarget" );

                        ctx.BeginRender();
                        ctx.BindPipeline( "Prefilter_Pipeline" );

                        ctx.PushConstants( std::addressof( m_PrefilterParameters ), sizeof( m_PrefilterParameters ) );

                        ctx.DrawIndexed( m_DrawBoxIndexedState );

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

                b.CreateTexture( "BRDFLutPass_ColorTarget", 512, 512 , TextureFormat::RG16_FLOAT, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/BRDFLut_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/BRDFLut_Frag.slang", ShaderStage::FRAGMENT );

                b.CreatePipeline( "BRDFLutPass_Pipeline", 
                    GraphicsPipelineDescription{
                    .VertexAttributesSpec{},
                    .ColorAttachmentFormats{ TextureFormat::RG16_FLOAT }
                } );

                b.Write( "BRDFLutPass_ColorTarget", FrameResourceState::RenderTarget );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                ctx.SetViewport( 0, 0, 512, 512 );
                ctx.SetScissor( 0, 0, 512, 512 );

                if (m_BRDFLutSampler.IsEmpty()) {
                    m_BRDFLutSampler = ctx.CreateSampler( SamplerDescription{} );
                }

                ctx.SetColorRenderTarget( "BRDFLutPass_ColorTarget" );

                ctx.BeginRender();

                ctx.BindPipeline( "BRDFLutPass_Pipeline" );
                
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

    auto IBLPasses::SetCubeMap( TextureHandle cubeMap ) -> void {
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

            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();
                b.CreateTexture( "SkyboxRender_ColorTargetCUBE", 2540, TextureFormat::RGBA32_FLOAT, 1 );
                b.CreateTexture( "SkyboxRender_ColorTarget", 2540, 2540, TextureFormat::RGBA32_FLOAT, TextureUsage::COLOR );

                b.UseShader( "Resources/Shaders/slang/SkyboxGen_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/SkyboxGen_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( "SkyboxRender_Pipeline",
                    GraphicsPipelineDescription{
                    .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                    .ColorAttachmentFormats{ TextureFormat::RGBA32_FLOAT 
                } } );

                b.Write( "SkyboxRender_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "SkyboxRender_ColorTargetCUBE", FrameResourceState::TransferDst );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & blackboard ) {
                MKT_BEGIN_PROFILER_NAMED();

                if (!m_RequestUpdateSkybox) {
                    return;
                }

                if (m_Equirectangular.IsEmpty()) {
                    return;
                }

                if ( m_Skybox2DSampler.IsEmpty() ) {
                    m_Skybox2DSampler = ctx.CreateSampler( SamplerDescription{ } );
                }

                ctx.BindImageSampler( ResourceGroup::DynamicSamplers, m_Equirectangular, m_Skybox2DSampler, ResourceSlot::Slot_0 );

                for ( Size mipLevel{}; mipLevel < 1; mipLevel++ ) {
                    for ( UInt32 face{}; face < MAX_CUBE_MAP_FACES; ++face ) {
                        constexpr bool flip{ false };
                        ctx.SetViewport( 0, 0, 2540, 2540, flip );
                        ctx.SetScissor( 0, 0, 2540, 2540 );

                        ctx.SetColorRenderTarget( "SkyboxRender_ColorTarget" );

                        ctx.BeginRender();
                        ctx.BindPipeline( "SkyboxRender_Pipeline" );

                        m_SkyboxRenderParameters.MVP = glm::perspective( static_cast<float>( Math::Constants::PI / 2.0 ), 1.0f, 0.1f, 512.0f ) * s_Matrices[face];
                        ctx.PushConstants( std::addressof( m_SkyboxRenderParameters ), sizeof( m_SkyboxRenderParameters ) );

                        ctx.DrawIndexed( m_DrawBoxIndexedState );

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

        m_BoxModel = AssetsService::Get()->LoadAsset<Model>( "Resources/Models/1 - Box texture/Box.gltf" );

        graph.RegisterPass(
            "Skybox",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.UseShader( "Resources/Shaders/slang/Skybox_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/Skybox_Frag.slang", ShaderStage::FRAGMENT );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ false },
                    .DepthWrite{ false },
                    .AlphaBlending{ false },
                    .PipelineCullMode{ CullMode::NONE },
                    .PrimitiveTopology{ Topology::TRIANGLE_LIST },
                };
                b.CreatePipeline( "SkyboxPass_Pipeline", graphicsDesc );

                b.Write( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "FinalShadingPass_DepthTarget", FrameResourceState::DepthWrite );

                b.Read( "SkyboxRender_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "PrefilterPass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );

                // The first mesh of the box model should be the only one,
                // we get the index and vertex buffers from it
                // the draw state is cached as it doesn't change between frames,
                // and can be reused in other passes like irradiance and prefilter
                MeshNode &boxMeshNode { m_BoxModel->GetMeshNode( 0 ) };

                m_DrawBoxIndexedState.IndexBuffer = boxMeshNode.GetIndexBuffer();
                m_DrawBoxIndexedState.VertexBuffers.emplace_back( boxMeshNode.GetVertexBuffer(), 0 );

                m_DrawBoxIndexedState.IndicesCount = boxMeshNode.GetIndexBuffer()->GetCount();
                m_DrawBoxIndexedState.InstancesCount = 1;
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                if (!m_UseSkybox) {
                    return;
                }

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                constexpr UInt32 bindSlot{ 1 };

                // Use the prefiltered map for convoluted background, in the shader you can specify the mip level as third parameter of texture() function
                // play around to see what fits best for the scene, remember to pass the max mip level as a push constant to avoid sampling beyond the available mip levels in the shader
                if ( m_UseConvolutedCubeMap ) {
                    m_IBLParameters.MaxMipLevel = m_PrefilterMipLevels;
                    ctx.BindImageSampler( ResourceGroup::DynamicSamplers, "PrefilterPass_ColorTargetCUBE", m_CubeMapSampler, ResourceSlot::Slot_0 );
                } else {
                    if ( m_UseCubeMap && !m_CubeMap.IsEmpty()) {
                        ctx.BindImageSampler( ResourceGroup::DynamicSamplers, m_CubeMap, m_CubeMapSampler, ResourceSlot::Slot_0 );
                    } else {
                        ctx.BindImageSampler( ResourceGroup::DynamicSamplers, "SkyboxRender_ColorTargetCUBE", m_CubeMapSampler, ResourceSlot::Slot_0 );
                    }
                }

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.PushConstants( std::addressof( m_IBLParameters ), sizeof( m_IBLParameters ) );

                ctx.SetClearColor( { 0.3f, 0.4f, 0.8f, 1.0f } );
                ctx.SetColorRenderTarget( "FinalShadingPass_ColorTarget" );
                ctx.SetDepthRenderTarget( "FinalShadingPass_DepthTarget" );

                ctx.BeginRender();

                ctx.BindPipeline( "SkyboxPass_Pipeline" );

                ctx.DrawIndexed( m_DrawBoxIndexedState );

                ctx.EndRender();
            } );
    }

    auto IBLPasses::SetCamera( const Camera *camera ) -> void {

    }
    
    auto IBLPasses::RegisterDirShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "DirectionalShadowMapPass",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();
                
                b.CreateTexture( "DirectionalShadowMapPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "DirectionalShadowMapPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT, TextureUsage::DEPTH );

                b.CreateBuffer( "DirectionalShadowMapPass_CameraInfo", BufferUsage::UNIFORM, MKT_SIZEOF( LightCameraInfo ), 1 );

                b.UseShader( "Resources/Shaders/slang/DirLighShadows_Frag.slang", ShaderStage::FRAGMENT );
                b.UseShader( "Resources/Shaders/slang/DirLighShadows_Vert.slang", ShaderStage::VERTEX );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .DepthAttachmentFormat{ TextureFormat::D32_FLOAT }
                };

                b.CreatePipeline( "DirectionalShadowMapPass_Pipeline", graphicsDesc );

                b.Write( "DirectionalShadowMapPass_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "DirectionalShadowMapPass_DepthTarget", FrameResourceState::DepthWrite );

                b.Read( "MeshCulling_GeometryInfo", FrameResourceState::UnorderedAccessView );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                MKT_ASSERT( m_MeshCullingPass, "Mesh culling must be valid" );

                auto &registry{ m_Scene->GetRegistry() };
                auto lights{ registry.view<TransformComponent, LightComponent>() };

                if (lights.size_hint() == 0) {
                    return;
                }

                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_GeometryInfo", ResourceSlot::Slot_0 );

                for (const auto &light : lights) {
                    auto &lightComp{ registry.get<LightComponent>( light ) };

                    if (lightComp.IsTypeActive( LightType::DIRECTIONAL_LIGHT_TYPE )) {
                        auto &transform{ registry.get<TransformComponent>( light ) };
                        
                        ctx.SetColorRenderTarget( "DirectionalShadowMapPass_ColorTarget" );
                        ctx.SetDepthRenderTarget( "DirectionalShadowMapPass_DepthTarget" );
                        
                        ctx.BeginRender();

                        constexpr float zNear{ 0.01f };
                        constexpr float zFar{ 2000.0f };
                        constexpr float degreesFOV{ 45.0f };

                        m_DirectionalShadowMapCameraInfo.LightView = glm::lookAt(transform.GetTranslation(), glm::vec3(0.0f), glm::vec3(0, 1, 0));
                        m_DirectionalShadowMapCameraInfo.LightProjection = glm::perspective(glm::radians(degreesFOV), 1.0f, zNear, zFar);

                        ctx.PushConstants( std::addressof( m_DirectionalShadowMapCameraInfo ), sizeof(m_DirectionalShadowMapCameraInfo ) );

                        const auto dimensions{ InferDimensions( m_Resolution ) };
                        ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                        ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                        ctx.BindPipeline( "DirectionalShadowMapPass_Pipeline" );
                        m_MeshCullingPass->DrawInstances( ctx );
                        
                        ctx.EndRender();
                    }
                }
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

    auto IBLPasses::RegisterShading( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        
        graph.RegisterPass<EnvironmentConstants>(
            "FinalShading",
            [this]( FramePassBuilder &b, EnvironmentConstants& ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "FinalShadingPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, Multisampling::MSAA_X1, TextureUsage::COLOR );
                b.CreateTexture( "FinalShadingPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, Multisampling::MSAA_X1, TextureUsage::DEPTH );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .AlphaBlending{ true },
                    .EnableSampleRateShading{ true },
                    .MSAA{ Multisampling::MSAA_X1 },
                    .PipelineCullMode{ CullMode::NONE }, // We probably need to organize models by material some 
                                                         // models like the just_a_girl require cull_back to be properly visulized
                };
                
                b.UseShader( "Resources/Shaders/slang/PBR_Basic_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/PBR_Basic_Frag.slang", ShaderStage::FRAGMENT );
                b.CreatePipeline( "FinalCompositionPass_Pipeline", graphicsDesc );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );

                b.Read( "MeshCulling_GeometryInfo", FrameResourceState::UnorderedAccessView );
                b.Read( "MeshCulling_MaterialsInfo", FrameResourceState::UnorderedAccessView );
                b.Read( "MeshCulling_SkinningInfo", FrameResourceState::UnorderedAccessView );

                b.Read( "AABBGenComp_Clusters", FrameResourceState::UnorderedAccessView );
                b.Read( "LightCullingComp_LightsBuffer", FrameResourceState::UnorderedAccessView );

                b.Read( "FinalShadingPass_DepthTarget", FrameResourceState::DepthWrite );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::RenderTarget );

                b.Read( "BRDFLutPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "PrefilterPass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "IrradiancePass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                MKT_ASSERT( m_MeshCullingPass, "Mesh culling must be valid" );

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

                ctx.BeginRender( renderInfo );

                auto& data{ blackboard.Get<EnvironmentConstants>() };
                data.MaxReflectionLOD = m_IBLParameters.MaxReflectionLOD;
                data.Exposure = m_IBLParameters.Exposure;
                data.Gamma = m_IBLParameters.Gamma;
                data.IsSkyboxActive = m_IBLParameters.IsSkyboxActive;

                // Bind resources
                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_GeometryInfo", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_MaterialsInfo", ResourceSlot::Slot_1 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_SkinningInfo", ResourceSlot::Slot_2 );

                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "AABBGenComp_Clusters", ResourceSlot::Slot_3 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "LightCullingComp_LightsBuffer", ResourceSlot::Slot_4 );

                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "BRDFLutPass_ColorTarget", m_BRDFLutSampler, ResourceSlot::Slot_0 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "PrefilterPass_ColorTargetCUBE", m_CubeMapSampler, ResourceSlot::Slot_1 );
                ctx.BindImageSampler( ResourceGroup::StaticSamplers, "IrradiancePass_ColorTargetCUBE", m_CubeMapSampler, ResourceSlot::Slot_2 );

                ctx.BindGroup( ResourceGroup::UnboundedImageViews, "Texture2D_List" );

                ctx.PushConstants( std::addressof( data ), sizeof( data ) );

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindPipeline( "FinalCompositionPass_Pipeline" );
                m_MeshCullingPass->DrawInstances( ctx );

                ctx.EndRender();
            } );
    }

    auto IBLPasses::RegisterMetalRoughnessPBR( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Resources:
        // https://github.khronos.org/Vulkan-Site/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/04_lighting_implementation.html

        graph.RegisterPass(
            "MetalRoughnessPBR",
            [this]( FramePassBuilder &b ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                b.Create<Texture>( "MetalRoughnessPBR_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, Multisampling::MSAA_X1, TextureUsage::COLOR );
                b.Create<Texture>( "MetalRoughnessPBR_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, Multisampling::MSAA_X1, TextureUsage::DEPTH );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .AlphaBlending{ true },
                    .EnableSampleRateShading{ true },
                    .MSAA{ Multisampling::MSAA_X1 },
                    .PipelineCullMode{ CullMode::CULL_BACK },
                };

                b.UseShader( "Resources/Shaders/slang/PBR_MetallicRoughness_Vert.slang", ShaderStage::VERTEX )
                    .UseShader( "Resources/Shaders/slang/PBR_MetallicRoughness_Frag.slang", ShaderStage::FRAGMENT )
                    .Create<Pipeline>( "MetalRoughnessPBR_Pipeline", graphicsDesc );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UnorderedAccessView )
                    .Read( "AABBGenComp_Clusters", FrameResourceState::UnorderedAccessView )
                    .Read( "LightCullingComp_LightsBuffer", FrameResourceState::UnorderedAccessView )
                    .Read( "MetalRoughnessPBR_ColorTarget", FrameResourceState::RenderTarget)
                    .Read( "MetalRoughnessPBR_DepthTarget", FrameResourceState::RenderTarget )
                    .Read( "ClusteredShading_Parameters", FrameResourceState::UniformBuffer );

                b.Read( "PrefilterPass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "IrradiancePass_ColorTargetCUBE", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "BRDFLutPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                b.Read( "MeshCulling_MeshInfo", FrameResourceState::UnorderedAccessView );
                b.Read( "MeshCulling_MaterialInfo", FrameResourceState::UniformBuffer );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                MKT_ASSERT( m_MeshCullingPass, "Mesh culling must be valid" );

                ctx.BindPipeline( "MetalRoughnessPBR" );

                ctx.SetColorRenderTarget( "MetalRoughnessPBR_ColorTarget" );
                ctx.SetDepthRenderTarget( "MetalRoughnessPBR_DepthTarget" );

                LoadOp colorTargetLoadOP{ LoadOp::LOAD };
                if (!m_UseSkybox) {
                    colorTargetLoadOP = LoadOp::CLEAR;
                    ctx.SetClearColor( m_ClearColor );
                }

                PassRenderInfo renderInfo{
                    .ColorLoadOp{ colorTargetLoadOP },
                };

                ctx.BindImage( "PrefilterPass_ColorTargetCUBE", m_CubeMapSampler, 7 );
                ctx.BindImage( "IrradiancePass_ColorTargetCUBE", m_CubeMapSampler, 8 );
                ctx.BindImage( "BRDFLutPass_ColorTarget", m_BRDFLutSampler, 6 );

                ctx.BeginRender( renderInfo );

                const auto dimension{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimension.first, dimension.second );
                ctx.SetScissor( 0, 0, dimension.first, dimension.second );

                m_MeshCullingPass->DrawInstances( ctx );

                ctx.EndRender();
            } );
    }

    auto IBLPasses::RegisterDebugViewsPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Transition resources for presentation
        graph.RegisterPass(
            "DebugViewsPass",
            []( FramePassBuilder &b ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                // Force it go after the final composition
                //b.Read( "3DRenderTextEdge", FrameResourceState::UniformBuffer );

                // To have this pas transition the final convert the final image into a layout imgui likes
                //b.Read( "FinalShading_Params", FrameResourceState::ShaderRead_GraphicsPipeline );
                b.Read( "FinalShadingPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                //b.Read( "HelloTriangle_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
                //b.Read( "HelloTexture_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );

                //b.Read( "GBuffer_Position", FrameResourceState::ShaderRead_GraphicsPipeline );
                //b.Read( "GBuffer_Normal", FrameResourceState::ShaderRead_GraphicsPipeline );
                //b.Read( "GBuffer_Color", FrameResourceState::ShaderRead_GraphicsPipeline );
            },
            []( CommandContext &, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );
    }
}