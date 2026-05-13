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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Blackboard.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Math/Math.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/ShadowMappingModule.hh>
#include <Renderer/Passes/GeometryShadingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::scene;

    GeometryShadingModule::GeometryShadingModule( RenderResolution resolution )
        : mResolution{ resolution } {}

    auto GeometryShadingModule::SetScene( const Scene *scene ) -> void {
        mScene = scene;
    }

    auto GeometryShadingModule::SetCamera( const Camera *camera ) -> void {
        mCamera = camera;
    }

    auto GeometryShadingModule::SetGeometryManagement( GeometryCullModule &geom ) -> void {
        mGeometryManagement = MKT_ADDRESSOF( geom );
    }

    auto GeometryShadingModule::SetClearColor( const Color& color ) -> void {
        // Not sure if this is correct but otherwise clear colors look weird when tone-mapped.
        // Clear color is assumed to be in LDR we just conver it to an HDR value.
        mClearColor = color / ( 1.0f + color );
    }

    auto GeometryShadingModule::SetResolution( RenderResolution resolution ) -> void {
        mResolution = resolution;
    }

    auto GeometryShadingModule::SetEquirectangular( TextureHandle texture2D ) -> void {

    }

    auto GeometryShadingModule::SetRenderBackground( SceneBackgroundType bg ) -> void {
        mBackgroundType = bg;
    }

    auto GeometryShadingModule::SetEnableSsao( bool enable ) -> void {
        mEnableSsao = enable;
    }

    auto GeometryShadingModule::SetSsaoIntensity( float value ) -> void {
        mSsaoIntensity = value;
    }

    auto GeometryShadingModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Prepare external resources
        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "IBLCubeSampler_Sampler01" )
            .SetBorderColor( kColorBlack ) };

        info.mIBLCubeSampler = graph.Create( samplerDes );

        // Create shared resources
        const auto dimensions{ InferDimensions( mResolution ) };
        auto colorImage{ FGTextureDescription{}
            .SetName( "FinalShading_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) }; // Will support HDR values

        info.mShadingColorImage = graph.Create( colorImage );

        auto skyboxCube{ FGTextureDescription{}
            .SetName( "SkyboxProjection_CubeRT" )
            .SetWidth( as<i32>( 2540 ) )
            .SetHeight( as<i32>( 2540 ) )
            .SetDimensions( TextureDimension::eTextureCube )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA32_FLOAT ) }; // For HDR values

        info.mSkyboxCubeRT = graph.Create( skyboxCube );

        RegisterSkyboxProjection( graph );

        RegisterBrdfLut( graph );
        RegisterPrefilter( graph );
        RegisterIrradiance( graph );

        RegisterSkyboxRender( graph );

        RegisterShading( graph );
    }

    auto GeometryShadingModule::RegisterIrradiance( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "Irradiance_CubeRT" )
            .SetWidth( as<i32>( kIrradianceDimensions ) )
            .SetHeight( as<i32>( kIrradianceDimensions ) )
            .SetDimensions( TextureDimension::eTextureCube )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA32_FLOAT ) }; // For HDR values

        info.mIrradianceCubeRT = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Irradiance_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA32_FLOAT )
            .PushShader( "Irradiance_Vert.slang", FGStageType::eVertex )
            .PushShader( "Irradiance_Frag.slang", FGStageType::eFragment ) };

        info.mIrradiancePipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "IrradiancePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, GeomShadingModuleInfo& data ) {
                b.Write( data.mIrradianceCubeRT, FGResourceState::eRenderTarget );
                b.Read( data.mSkyboxCubeRT, FGResourceState::eShaderResource );
            },
            []( CommandContext& ctx, Blackboard& blackboard ) {
                auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    float4x4 mMvp{};
                    f32 mDeltaTheta{};
                    u32 mDeltaPhi{};
                    u32 mSkyboxCubeID{};
                    u32 mBasicSamplerID{};
                } params{
                    .mSkyboxCubeID = ctx.PushTexture_SRV( data.mSkyboxCubeRT ),
                    .mBasicSamplerID = ctx.PushSampler( data.mBasicSampler ),
                };

                for ( u32 mipLevel{}; mipLevel < kIrradianceMipLevels; mipLevel++ ) {
                    for ( u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        f32 width{ as<f32>(kIrradianceDimensions * std::pow(0.5f, mipLevel)) };
                        f32 heigh{ as<f32>(kIrradianceDimensions * std::pow(0.5f, mipLevel)) };

                        params.mDeltaTheta = 0.5f * math::constants::kPi / 64.0;
                        params.mDeltaPhi = 2.0f * math::constants::kPi / 180.0f;
                        params.mMvp = glm::perspective( as<f32>( math::constants::kPi / 2.0 ),
                            1.0f, 0.1f, 512.0f ) * kMatrices[face];

                        ctx.PushConstants( params );

                        auto graphicsState{ ContextRenderState{}
                            .SetRenderArea( Rect{ as<i32>(width), as<i32>(heigh) } )
                            .AddRenderTarget( data.mIrradianceCubeRT, kColorCyan, LoadOp::eClear, face, mipLevel ) };
                        ctx.BeginRender( graphicsState );

                        ctx.SetViewportState( ViewportState{}
                            .AddViewportAndScissorRect( Viewport( width, heigh ) ) );

                        ctx.BindPipeline( data.mIrradiancePipeline );
                        ctx.Draw( 36 );

                        ctx.EndRender();
                    }
                }
            } );
    }

    auto GeometryShadingModule::RegisterPrefilter( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "Prefilter_CubeRT" )
            .SetWidth( as<i32>( kPrefilterDimensions ) )
            .SetHeight( as<i32>( kPrefilterDimensions ) )
            .SetDimensions( TextureDimension::eTextureCube )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) }; // For HDR values

        info.mPrefilterCubeRT = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Prefilter_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "Prefilter_Vert.slang", FGStageType::eVertex )
            .PushShader( "Prefilter_Frag.slang", FGStageType::eFragment ) };

        info.mPrefilterPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "PrefilterPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder&b, GeomShadingModuleInfo& info ) {
                b.Write( info.mPrefilterCubeRT, FGResourceState::eRenderTarget );
                b.Read( info.mSkyboxCubeRT, FGResourceState::eShaderResource );
            },

            [this]( CommandContext &ctx, Blackboard& blackboard ) {
                auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    float4x4 mMvp{};
                    f32 mRoughness{};
                    u32 mNumSamples{};
                    u32 mSkyboxCubeID{};
                    u32 mBasicSamplerID{};

                } params{
                    .mNumSamples = 1024, // Tweak
                    .mSkyboxCubeID = ctx.PushTexture_SRV( data.mSkyboxCubeRT ),
                    .mBasicSamplerID = ctx.PushSampler( data.mBasicSampler ),
                };

                for ( u32 mipLevel{}; mipLevel < mPrefilterMipLevels; mipLevel++ ) {
                    params.mRoughness = as<f32>( mipLevel ) / as<f32>( mPrefilterMipLevels - 1 );

                    for ( u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        f32 width{ as<f32>(kPrefilterDimensions * glm::pow(0.5f, mipLevel)) };
                        f32 heigh{ as<f32>(kPrefilterDimensions * glm::pow(0.5f, mipLevel)) };

                        params.mMvp = glm::perspective( as<f32>( math::constants::kPi / 2.0 ),
                            1.0f, 0.1f, 512.0f ) * kMatrices[face];

                        ctx.PushConstants( params );

                        auto graphicsState{ ContextRenderState{}
                            .SetRenderArea( Rect{ as<i32>(width), as<i32>(heigh) } )
                            .AddRenderTarget( data.mPrefilterCubeRT, kColorCyan, LoadOp::eClear, face, mipLevel ) };
                        ctx.BeginRender( graphicsState );

                        ctx.SetViewportState( ViewportState{}
                            .AddViewportAndScissorRect( Viewport( width, heigh ) ) );

                        ctx.BindPipeline( data.mPrefilterPipeline );
                        ctx.Draw( 36 );

                        ctx.EndRender();
                    }
                }
            } );
    }

    auto GeometryShadingModule::RegisterBrdfLut( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "BRDFLut_ColorImage01" )
            .SetWidth( as<i32>( 512 ) )
            .SetHeight( as<i32>( 512 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRG16_FLOAT ) };

        info.mBrdfColorTarget = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "BRDFLut_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eRG16_FLOAT )
            .PushShader( "BRDFLut_Vert.slang", FGStageType::eVertex )
            .PushShader( "BRDFLut_Frag.slang", FGStageType::eFragment ) };

        info.mBrdfPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "BRDFLut",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, GeomShadingModuleInfo& info ) {
                b.Write( info.mBrdfColorTarget, FGResourceState::eRenderTarget );
            },
            []( CommandContext &ctx, Blackboard& b ) -> void {
                GeomShadingModuleInfo& info{ b.Get<GeomShadingModuleInfo>() };

                auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ 512, 512 } )
                    .AddRenderTarget( info.mBrdfColorTarget, kColorCyan, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( 512, 512 ) ) );

                ctx.BindPipeline( info.mBrdfPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::SetExposure( float value ) -> void {
        mExposure = value;
    }

    auto GeometryShadingModule::SetGamma( float value ) -> void {
        mGamma = value;
    }

    auto GeometryShadingModule::RegisterSkyboxProjection( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SkyboxProjection_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA32_FLOAT )
            .PushShader( "SkyboxProjection_Vert.slang", FGStageType::eVertex )
            .PushShader( "SkyboxProjection_Frag.slang", FGStageType::eFragment ) };

        info.mSkyboxProjectionPipeline = graph.Create( pipelineBuilder );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "SkyboxProjection_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };

        info.mBasicSampler = graph.Create( samplerDes );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "SkyboxProjection",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, GeomShadingModuleInfo& info ) {
                b.Write( info.mSkyboxCubeRT, FGResourceState::eRenderTarget );
            },
    []( CommandContext &ctx, Blackboard &blackboard ) {
                GeomShadingModuleInfo& info{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    float4x4 mMvp{};
                    u32 mBasicSamplerID{};
                    u32 mEquirectangularID{};
                } params{
                    .mMvp = math::constants::Identity<float4x4>(),
                    .mBasicSamplerID = ctx.PushSampler( info.mBasicSampler ),
                    .mEquirectangularID = ctx.PushTexture_SRV( info.mImportedEquirectangular ),
                };

                for ( u32 mipLevel{}; mipLevel < 1; mipLevel++ ) {
                    for ( u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        params.mMvp = glm::perspective( as<f32>( math::constants::kPi / 2.0 ),
                            1.0f, 0.1f, 512.0f ) * kMatrices[face];

                        ctx.PushConstants( params );

                        auto graphicsState{ ContextRenderState{}
                            .SetRenderArea( Rect{ 2540, 2540 } )
                            .AddRenderTarget( info.mSkyboxCubeRT, kColorCyan, LoadOp::eClear, face, mipLevel ) };
                        ctx.BeginRender( graphicsState );

                        ctx.SetViewportState( ViewportState{}
                            .AddViewportAndScissorRect( Viewport( 2540, 2540 ) ) );

                        ctx.BindPipeline( info.mSkyboxProjectionPipeline );
                        ctx.Draw( 36 );

                        ctx.EndRender();
                    }
                }
        } );
    }

    auto GeometryShadingModule::RegisterSkyboxRender( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "FinalShading_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) }; // Will support HDR values

        info.mShadingColorImage = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SkyboxRender_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "Skybox_Vert.slang", FGStageType::eVertex )
            .PushShader( "Skybox_Frag.slang", FGStageType::eFragment ) };

        info.mSkyboxRenderPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "SkyboxRender",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                CameraModuleInfo& camInfo{ blackboard.Get<CameraModuleInfo>() };
                PrepassModuleInfo& prepass{ blackboard.Get<PrepassModuleInfo>() };
                GeomShadingModuleInfo& finalImageInfo{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.Write( finalImageInfo.mShadingColorImage, FGResourceState::eRenderTarget );
                builder.Read( prepass.mDepthPrepassDepthTarget, FGResourceState::eDepthRead );

                // We can either render this guy or the prefilter image
                builder.Read( finalImageInfo.mSkyboxCubeRT, FGResourceState::eShaderResource );
                builder.Read( finalImageInfo.mPrefilterCubeRT, FGResourceState::eShaderResource );
                builder.Read( camInfo.mCameraData, FGResourceState::eShaderResource );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                CameraModuleInfo& camInfo{ b.Get<CameraModuleInfo>() };
                PrepassModuleInfo& prepass{ b.Get<PrepassModuleInfo>() };
                GeomShadingModuleInfo& finalImageInfo{ b.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    u32 mCameraBufferID{};
                    u32 mBasicSamplerID{};

                    u32 mSkyboxCubeRtID{};
                    u32 mPrefilterCubeRtID{};

                    u32 mBackGroundType{}; // Prefilter map, Original skybox, clear color

                    u32 mMaxMipLevel{};
                    f32 mExposure{ 1.0f };
                    f32 mGamma{ 2.0f };
                } params{
                    .mCameraBufferID = ctx.PushBuffer_SRV( camInfo.mCameraData ),
                    .mBasicSamplerID = ctx.PushSampler( finalImageInfo.mBasicSampler ),

                    .mSkyboxCubeRtID = ctx.PushTexture_SRV( finalImageInfo.mSkyboxCubeRT ),
                    .mPrefilterCubeRtID = ctx.PushTexture_SRV( finalImageInfo.mPrefilterCubeRT ),

                    .mBackGroundType = as<u32>( mBackgroundType ),

                    .mMaxMipLevel = mPrefilterMipLevels,
                };

                const auto dimensions{ InferDimensions( mResolution ) };
                ctx.PushConstants( params );

                auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>(dimensions.first), as<i32>(dimensions.second) } )
                    .AddDepthTarget( prepass.mDepthPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( finalImageInfo.mShadingColorImage, kColorCyan, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<i32>(dimensions.first), as<i32>(dimensions.second) ) ) );

                // For no clear color use background cube
                // otherwise just clear the image with specified solid color
                if (mBackgroundType != SceneBackgroundType::eClearColor) {
                    ctx.BindPipeline( finalImageInfo.mSkyboxRenderPipeline );
                    ctx.Draw( 36 );
                }

                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::RegisterShading( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Resources:
        // https://github.khronos.org/Vulkan-Site/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/04_lighting_implementation.html

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "PBR_MetallicRoughness_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .SetCullMode( CullMode::eCullBack )
            .PushShader( "PBR_MetallicRoughness_Vert.slang", FGStageType::eVertex )
            .PushShader( "PBR_MetallicRoughness_Frag.slang", FGStageType::eFragment ) };

        info.mShadingPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "PBR_Radiance",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard & blackboard ) -> void {
                CameraModuleInfo& cam{ blackboard.Get<CameraModuleInfo>() };
                PrepassModuleInfo& prepass{ blackboard.Get<PrepassModuleInfo>() };
                GeometryManagementModuleInfo& geom{ blackboard.Get<GeometryManagementModuleInfo>() };
                GeomShadingModuleInfo& finalImage{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.Read( cam.mCameraData, FGResourceState::eShaderResource );

                builder.Read( geom.mVerticesBuffer, FGResourceState::eShaderResource );
                builder.Read( geom.mIndicesBuffer, FGResourceState::eShaderResource );

                builder.Read( geom.mGeometryBuffer, FGResourceState::eShaderResource );
                builder.Read( geom.mMaterialsBuffer, FGResourceState::eShaderResource );
                builder.Read( geom.mSkinningBuffer, FGResourceState::eShaderResource );

                builder.Read( prepass.mClusterBuffer, FGResourceState::eShaderResource );
                builder.Read( prepass.mLightCullingBuffer, FGResourceState::eShaderResource );

                builder.Write( finalImage.mShadingColorImage, FGResourceState::eRenderTarget );
                builder.Read( prepass.mDepthPrepassDepthTarget, FGResourceState::eDepthRead );

                builder.Read( finalImage.mBrdfColorTarget, FGResourceState::eShaderResource );
                builder.Read( finalImage.mPrefilterCubeRT, FGResourceState::eShaderResource );
                builder.Read( finalImage.mIrradianceCubeRT, FGResourceState::eShaderResource );

                // SSAO stuff here

                // Shadow mapping stuff here
                const auto& shadow{ blackboard.Get<ShadowMapInfo>() };

                for (const auto& map : shadow.mSpotShadowMaps) {
                    if (map.mHandle != 0) {
                        builder.Read( map, FGResourceState::eDepthRead );
                    }
                }
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) -> void {
                CameraModuleInfo& cam{ blackboard.Get<CameraModuleInfo>() };
                PrepassModuleInfo& prepass{ blackboard.Get<PrepassModuleInfo>() };
                GeometryManagementModuleInfo& geom{ blackboard.Get<GeometryManagementModuleInfo>() };
                GeomShadingModuleInfo& finalImage{ blackboard.Get<GeomShadingModuleInfo>() };

                finalImage.mExposure = mExposure;
                finalImage.mGamma = mGamma;

                struct DrawParams {
                    u32 mCameraDataID{};
                    u32 mVerticesBufferID{};
                    u32 mIndicesBufferID{};

                    u32 mGeometryBufferID{};
                    u32 mMaterialsBufferID{};
                    u32 mSkinningBufferID{};
                    u32 mClusterBufferID{};

                    u32 mLightCullingBufferID{};
                    u32 mBrdfColorTargetID{};
                    u32 mPrefilterCubeRtID{};
                    u32 mIrradianceCubeRtID{};

                    u32 mSamplerCubeID{};

                    float4 mGridSize{};
                    f32 mExposure{ 1.0f };
                    f32 mGamma{ 2.0f };

                    u32 mActiveLightCount{};

                    u32 mEnableSsao{ MKT_SHADER_FALSE };
                    f32 mSsaoIntensity{ 1.f };

                    u32 mPrefilteredCubeMipLevels{};
                    f32 mScaleIblAmbient{ 1.0f };

                    u32 mIsSkyboxActive{ MKT_SHADER_FALSE };
                } params{
                    .mCameraDataID = ctx.PushBuffer_SRV( cam.mCameraData ),

                    .mVerticesBufferID = ctx.PushBuffer_SRV( geom.mVerticesBuffer ),
                    .mIndicesBufferID = ctx.PushBuffer_SRV( geom.mIndicesBuffer ),

                    .mGeometryBufferID = ctx.PushBuffer_SRV( geom.mGeometryBuffer ),
                    .mMaterialsBufferID = ctx.PushBuffer_SRV( geom.mMaterialsBuffer ),
                    .mSkinningBufferID = ctx.PushBuffer_SRV( geom.mSkinningBuffer ),

                    .mClusterBufferID = ctx.PushBuffer_SRV( prepass.mClusterBuffer ),
                    .mLightCullingBufferID = ctx.PushBuffer_SRV( prepass.mLightCullingBuffer ),

                    .mBrdfColorTargetID = ctx.PushTexture_SRV( finalImage.mBrdfColorTarget ),
                    .mPrefilterCubeRtID = ctx.PushTexture_SRV( finalImage.mPrefilterCubeRT ),
                    .mIrradianceCubeRtID = ctx.PushTexture_SRV( finalImage.mIrradianceCubeRT ),

                    .mSamplerCubeID = ctx.PushSampler( finalImage.mBasicSampler ),

                    .mGridSize = prepass.mGridSize,
                    .mExposure = mExposure,
                    .mGamma = mGamma,
                    .mActiveLightCount = prepass.mActiveLightCount,
                    .mEnableSsao = mEnableSsao ? MKT_SHADER_TRUE : MKT_SHADER_FALSE,
                    .mSsaoIntensity = mSsaoIntensity,
                    .mPrefilteredCubeMipLevels = mPrefilterMipLevels,
                    .mScaleIblAmbient = 1.0f,
                    .mIsSkyboxActive = mBackgroundType != SceneBackgroundType::eClearColor ? MKT_SHADER_TRUE : MKT_SHADER_FALSE,
                };

                ctx.PushConstants( params );

                LoadOp colorLoadOp{ LoadOp::eClear };
                if (mBackgroundType != SceneBackgroundType::eClearColor) {
                    colorLoadOp = LoadOp::eLoad;
                }

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>(dimensions.first), as<i32>(dimensions.second) } )
                    .AddDepthTarget( prepass.mDepthPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( finalImage.mShadingColorImage, mClearColor, colorLoadOp ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<i32>(dimensions.first), as<i32>(dimensions.second) ) ) );

                ctx.BindPipeline( finalImage.mShadingPipeline );

                mGeometryManagement->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );
    }
}