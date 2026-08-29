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

    auto GeometryShadingModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        mGeometryManagement = MKT_ADDRESSOF( geom );
    }

    auto GeometryShadingModule::SetMergeWireframeToFinalOutput( bool merge ) -> void {

    }

    auto GeometryShadingModule::SetClearColor( const Color& color ) -> void {
        // Not sure if this is correct but otherwise clear colors look weird when tone-mapped.
        // Clear color is assumed to be in LDR we just conver it to an HDR value.
        mClearColor = color / ( 1.0f + color );
    }

    auto GeometryShadingModule::SetResolution( RenderResolution resolution ) -> void {
        mResolution = resolution;
    }

    auto GeometryShadingModule::SetSkyboxMaterial( material::MaterialHandle material ) -> void {
        // Release first descriptor table indices from old material textures

        mSkyboxMaterial = material;
    }

    auto GeometryShadingModule::SetRenderBackground( SceneBackgroundType bg ) -> void {
        mBackgroundType = bg;
    }

    auto GeometryShadingModule::SetEnableSsao( bool enable ) -> void {
        mEnableSsao = enable;
    }

    auto GeometryShadingModule::SetSsaoIntensity( f32 value ) -> void {
        mSsaoIntensity = value;
    }

    auto GeometryShadingModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Prepare the box model, etc
        PrepareGeometryShadingAssets( graph );

        // Prepare external resources
        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto iblSampler{ FGSamplerDescription{}
            .SetName( "IBLCubeSampler_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge )
            .SetBorderColor( kColorBlack ) };
        info.mSkyboxCubeSampler = graph.Create( iblSampler );

        // Create shared resources
        const auto dimensions{ InferDimensions( mResolution ) };
        auto colorImage{ FGTextureDescription{}
            .SetName( "FinalShading_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) }; // Will support HDR values
        info.mColorImage = graph.Create( colorImage );

        auto samplerDesc{ FGSamplerDescription{}
            .SetName( "SkyboxProjection_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eRepeat )
            .SetBorderColor( kColorWhite ) };
        info.mDefaultSampler = graph.Create( samplerDesc );

        auto skyboxCube{ FGTextureDescription{}
            .SetName( "SkyboxProjection_CubeRT" )
            .SetWidth( as<i32>( 2540 ) )
            .SetHeight( as<i32>( 2540 ) )
            .SetDimensions( TextureDimension::eTextureCube )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA32_FLOAT ) }; // For HDR values
        info.mSkyboxCubeRT = graph.Create( skyboxCube );

        RegisterBrdfLut( graph );
        RegisterSkyboxProjection( graph );

        RegisterPrefilter( graph );
        RegisterIrradiance( graph );

        RegisterSkyboxRender( graph );

        RegisterShading( graph );

        RegisterWireframe( graph );
    }

    auto GeometryShadingModule::RegisterIrradiance( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "Irradiance_CubeRT" )
            .SetWidth( as<i32>( kIrradianceDimensions ) )
            .SetHeight( as<i32>( kIrradianceDimensions ) )
            .SetMipCount( as<i32>( kIrradianceMipLevels ) )
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
            .PushShader( "Irradiance_Frag.slang", FGStageType::ePixel ) };
        info.mIrradiancePipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "IrradiancePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, GeomShadingModuleInfo& data ) {
                b.UseResource( data.mIrradianceCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mSkyboxCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            []( CommandContext& ctx, Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    float4x4 mMvp{};
                    f32 mDeltaTheta{};
                    f32 mDeltaPhi{};

                    u32 mSkyboxCubeID{};
                    u32 mBasicSamplerID{};

                    u32 mVertexBufferID{};
                    u32 mIndexBufferID{};
                } params{
                    .mSkyboxCubeID = ctx.PushTexture_SRV( data.mSkyboxCubeRT ),
                    .mBasicSamplerID = ctx.PushSampler( data.mSkyboxCubeSampler ),

                    .mVertexBufferID = ctx.PushBuffer_SRV(data.mBoxVertexBuffer),
                    .mIndexBufferID = ctx.PushBuffer_SRV(data.mBoxIndexBuffer) };

                for ( u32 mipLevel{}; mipLevel < kIrradianceMipLevels; mipLevel++ ) {
                    for ( u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        f32 width{ as<f32>(kIrradianceDimensions * std::pow(0.5f, mipLevel)) };
                        f32 heigh{ as<f32>(kIrradianceDimensions * std::pow(0.5f, mipLevel)) };

                        params.mDeltaTheta = 0.5f * math::constants::kPi / 64.0;
                        params.mDeltaPhi = 2.0f * math::constants::kPi / 180.0f;
                        params.mMvp = glm::perspective( as<f32>( math::constants::kPi / 2.0 ),
                            1.0f, 0.1f, 512.0f ) * kMatrices[face];

                        ctx.PushConstants( params );

                        const auto graphicsState{ ContextRenderState{}
                            .SetRenderArea( Rect{ as<i32>(width), as<i32>(heigh) } )
                            .AddRenderTarget( data.mIrradianceCubeRT, kColorCyan, LoadOp::eClear, face, mipLevel ) };
                        ctx.BeginRender( graphicsState );

                        ctx.SetViewportState( ViewportState{}
                            .AddViewportAndScissorRect( Viewport( width, heigh ) ) );

                        ctx.BindPipeline( data.mIrradiancePipeline );
                        ctx.Draw( data.mBoxIndicesCount );

                        ctx.EndRender();
                    }
                }
            } );
    }

    auto GeometryShadingModule::PrepareGeometryShadingAssets( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        mBoxModel = AssetsService::Get()->LoadAsset<Model>( "Resources/Prefabs/box/Box.gltf" );

        info.mBoxVerticesCount = mBoxModel->GetMeshNode( 0 ).GetVertexBuffer()->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription );
        info.mBoxIndicesCount = mBoxModel->GetMeshNode( 0 ).GetIndexBuffer()->GetSizeBytes() / MKT_SIZEOF( u32 );

        auto vertexDesc{ FGBufferDescription{}
            .SetName( "GeometryShadingBox_Vertices" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( info.mBoxVerticesCount, MKT_SIZEOF( asset::VertexDescription ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        info.mBoxVertexBuffer = graph.Create( vertexDesc );

        auto indexDesc{ FGBufferDescription{}
            .SetName( "GeometryShadingBox_Indices" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( info.mBoxIndicesCount, MKT_SIZEOF( u32 ) )
            .SetHeapType( HeapType::eDeviceLocal )};
        info.mBoxIndexBuffer = graph.Create( indexDesc );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "GeometryShadingBox_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, GeomShadingModuleInfo &data ) {
                b.UseResource( data.mBoxVertexBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mBoxIndexBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<GeomShadingModuleInfo>() };

                // The model only has one mesh which is at index 0
                FGBufferHandle vertices{ ctx.ImportBuffer( mBoxModel->GetMeshNode( 0 ).GetVertexBuffer() ) };
                FGBufferHandle indices{ ctx.ImportBuffer( mBoxModel->GetMeshNode( 0 ).GetIndexBuffer() ) };

                ctx.CopyBuffer( data.mBoxVertexBuffer, vertices );
                ctx.CopyBuffer( data.mBoxIndexBuffer, indices );
            } );

        graph.SetExecutionPolicy( "GeometryShadingBox_Upload", FGExecutionPolicy::eOnce );
    }

    auto GeometryShadingModule::RegisterPrefilter( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "Prefilter_CubeRT" )
            .SetWidth( as<i32>( kPrefilterDimensions ) )
            .SetHeight( as<i32>( kPrefilterDimensions ) )
            .SetDimensions( TextureDimension::eTextureCube )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetMipCount( mPrefilterMipLevels )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) }; // For HDR values
        info.mPrefilterCubeRT = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Prefilter_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "Prefilter_Vert.slang", FGStageType::eVertex )
            .PushShader( "Prefilter_Frag.slang", FGStageType::ePixel ) };
        info.mPrefilterPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "PrefilterPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder&b, GeomShadingModuleInfo& info ) {
                b.UseResource( info.mPrefilterCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( info.mSkyboxCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    float4x4 mMvp{};
                    f32 mRoughness{};
                    u32 mNumSamples{};

                    u32 mSkyboxCubeID{};
                    u32 mBasicSamplerID{};

                    u32 mVertexBufferID{};
                    u32 mIndexBufferID{};
                } params{
                    .mNumSamples = 1024, // Tweak
                    .mSkyboxCubeID = ctx.PushTexture_SRV( data.mSkyboxCubeRT ),
                    .mBasicSamplerID = ctx.PushSampler( data.mSkyboxCubeSampler ),

                    .mVertexBufferID = ctx.PushBuffer_SRV(data.mBoxVertexBuffer),
                    .mIndexBufferID = ctx.PushBuffer_SRV(data.mBoxIndexBuffer) };

                for ( u32 mipLevel{}; mipLevel < mPrefilterMipLevels; mipLevel++ ) {
                    params.mRoughness = as<f32>( mipLevel ) / as<f32>( mPrefilterMipLevels - 1 );

                    for ( u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        f32 width{ as<f32>(kPrefilterDimensions * glm::pow(0.5f, mipLevel)) };
                        f32 heigh{ as<f32>(kPrefilterDimensions * glm::pow(0.5f, mipLevel)) };

                        params.mMvp = glm::perspective( as<f32>( math::constants::kPi / 2.0 ),
                            1.0f, 0.1f, 512.0f ) * kMatrices[face];

                        ctx.PushConstants( params );

                        const auto graphicsState{ ContextRenderState{}
                            .SetRenderArea( Rect{ as<i32>(width), as<i32>(heigh) } )
                            .AddRenderTarget( data.mPrefilterCubeRT, kColorCyan, LoadOp::eClear, face, mipLevel ) };
                        ctx.BeginRender( graphicsState );

                        ctx.SetViewportState( ViewportState{}
                            .AddViewportAndScissorRect( Viewport( width, heigh ) ) );

                        ctx.BindPipeline( data.mPrefilterPipeline );
                        ctx.Draw( data.mBoxIndicesCount );

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
            .PushShader( "BRDFLut_Frag.slang", FGStageType::ePixel ) };
        info.mBrdfPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "BRDFLut",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, GeomShadingModuleInfo& info ) {
                b.UseResource( info.mBrdfColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            []( CommandContext &ctx, Blackboard& b ) -> void {
                const auto& info{ b.Get<GeomShadingModuleInfo>() };
                const auto graphicsState{ ContextRenderState{}
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

    auto GeometryShadingModule::SetAmbientScale( f32 ambient ) -> void {
        mAbientScale = ambient;
    }

    auto GeometryShadingModule::SetGamma( float value ) -> void {
        mGamma = value;
    }

    auto GeometryShadingModule::RegisterSkyboxProjection( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SkyboxProjection_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA32_FLOAT )
            .PushShader( "SkyboxProjection_Vert.slang", FGStageType::eVertex )
            .PushShader( "SkyboxProjection_Frag.slang", FGStageType::ePixel ) };
        info.mSkyboxProjectionPipeline_FlatImage = graph.Create( pipelineBuilder );

        // Takes an equirectangular map and projects it onto a Cube texture
        graph.RegisterPass<GeomShadingModuleInfo>(
            "SkyboxProjection",
            FGPassType::eGraphics,
            [](FGNodeBuilder& b, GeomShadingModuleInfo& info) {
                b.UseResource( info.mSkyboxCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                b.UseResource( info.mBoxVertexBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( info.mBoxIndexBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            [this](CommandContext& ctx, Blackboard& blackboard){
                if (mSkyboxMaterial.IsEmpty()) {
                    return;
                }

                auto* material{ checked_cast<SkyboxMaterial*>( mSkyboxMaterial.GetRaw() ) };
                if (!material->IsType( SkyboxType::eEquirectangular ) || material->GetEquirectangular().IsEmpty()) {
                    return;
                }

                auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    float4x4 mMvp{};
                    u32 mBasicSamplerID{};
                    u32 mEquirectangularID{};

                    u32 mVertexBufferID{};
                    u32 mIndexBufferID{};
                } params{
                    .mMvp = math::constants::Identity<float4x4>(),
                    .mBasicSamplerID = ctx.PushSampler(data.mDefaultSampler),
                    .mEquirectangularID = ctx.PushTexture_SRV(ctx.ImportTexture( material->GetEquirectangular() )),

                    .mVertexBufferID = ctx.PushBuffer_SRV(data.mBoxVertexBuffer),
                    .mIndexBufferID = ctx.PushBuffer_SRV(data.mBoxIndexBuffer) };

                // Project onto the skybox cube image if the material uses an
                // equirectangular image, otherwise if material consists of 6 flat images
                // we copy them instead

                for (u32 mipLevel{}; mipLevel < 1; mipLevel++) {
                    for (u32 face{}; face < rhi::kMaxCubeFaces; ++face) {
                        params.mMvp = glm::perspective(as<f32>(math::constants::kPi / 2.0),
                                                       1.0f, 0.1f, 512.0f) * kMatrices[face];
                        ctx.PushConstants(params);

                        auto graphicsState{ ContextRenderState{}
                            .SetRenderArea(Rect{2540, 2540})
                            .AddRenderTarget(data.mSkyboxCubeRT, kFaceColors[face], LoadOp::eClear, face, mipLevel) };
                        ctx.BeginRender(graphicsState);

                        ctx.SetViewportState(ViewportState{}
                            .AddViewportAndScissorRect(Viewport(2540, 2540)));

                        ctx.BindPipeline(data.mSkyboxProjectionPipeline_FlatImage);
                        ctx.Draw(data.mBoxIndicesCount ); // For vertex pulling indices is the draw vertex count

                        ctx.EndRender();
                    }
                }
            });

        // When we provide the skybox material as a list of 6 2D images
        graph.RegisterPass<GeomShadingModuleInfo>(
            "SkyboxProjection_Compute",
            FGPassType::eCompute,
            []( FGNodeBuilder&b, GeomShadingModuleInfo& info ) {
                b.UseResource( info.mSkyboxCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard) {
                if (mSkyboxMaterial.IsEmpty()) {
                    return;
                }

                auto* material{ checked_cast<SkyboxMaterial*>( mSkyboxMaterial.GetRaw() ) };
                if (!material->IsType( SkyboxType::eCubeFaces )) {
                    return;
                }

                auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

            } );

        // This pass is done for testing purposes,
        // to see if copy yields faster results than compute
        graph.RegisterPass<GeomShadingModuleInfo>(
            "SkyboxProjection_Transfer",
            FGPassType::eTransfer,
            []( FGNodeBuilder& b, GeomShadingModuleInfo& info ) {
                b.UseResource( info.mSkyboxCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard) {
                if (mSkyboxMaterial.IsEmpty()) {
                    return;
                }

                auto* material{ checked_cast<SkyboxMaterial*>( mSkyboxMaterial.GetRaw() ) };
                if (!material->IsType( SkyboxType::eCubeFaces )) {
                    return;
                }

                auto& data{ blackboard.Get<GeomShadingModuleInfo>() };
            } );

        // This pass is done for testing purposes,
        // to see if copy yields faster results than compute

        auto pipelineProjGraphicsBuilder{ FGPipelineDescription{}
            .SetName( "SkyboxProjectionGraphics_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA32_FLOAT )
            .PushShader( "SkyboxProjectionGraphics_Vert.slang", FGStageType::eVertex )
            .PushShader( "SkyboxProjectionGraphics_Frag.slang", FGStageType::ePixel ) };
        info.mSkyboxProjectionPipeline_Graphics = graph.Create( pipelineProjGraphicsBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "SkyboxProjection_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, GeomShadingModuleInfo& info ) {
                b.UseResource( info.mSkyboxCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard) {
                if (mSkyboxMaterial.IsEmpty()) {
                    return;
                }

                auto* material{ checked_cast<SkyboxMaterial*>( mSkyboxMaterial.GetRaw() ) };
                if (!material->IsType( SkyboxType::eCubeFaces )) {
                    return;
                }

                // Prepare textures
                const auto& textures{ material->GetFaceTextures() };
                for (const auto& [face, texture] : textures) {
                    mSkyboxFaces[face] = ctx.ImportTexture( texture );
                }

                // See kMatrices definition
                eastl::fixed_hash_map<u32, SkyboxFace, 6> faceIndex{};

                faceIndex[1] = SkyboxFace::eRight; // POSITIVE_X
                faceIndex[0] = SkyboxFace::eLeft;  // NEGATIVE_X
                faceIndex[3] = SkyboxFace::eTop;   // POSITIVE_Y
                faceIndex[2] = SkyboxFace::eBottom;// NEGATIVE_Y
                faceIndex[4] = SkyboxFace::eBack;  // POSITIVE_Z
                faceIndex[5] = SkyboxFace::eFront; // NEGATIVE_Z

                auto& data{ blackboard.Get<GeomShadingModuleInfo>() };
                for (u32 mipLevel{}; mipLevel < 1; mipLevel++) {
                    for (u32 face{}; face < rhi::kMaxCubeFaces; ++face) {
                        struct DrawParams {
                            float4x4 mMvp{};
                            u32 mBasicSamplerID{};
                            u32 mSkyboxFaceID{};
                        } params{
                            .mMvp = math::constants::Identity<float4x4>(),
                            .mBasicSamplerID = ctx.PushSampler( data.mDefaultSampler ),
                            .mSkyboxFaceID = ctx.PushTexture_SRV( mSkyboxFaces[faceIndex[face]] ) };
                        params.mMvp = glm::perspective(as<f32>(math::constants::kPi / 2.0),
                                                       1.0f, 0.1f, 512.0f) * kMatrices[face];
                        ctx.PushConstants(params);

                        const auto graphicsState{ ContextRenderState{}
                            .SetRenderArea(Rect{2540, 2540})
                            .AddRenderTarget(data.mSkyboxCubeRT, kFaceColors[face], LoadOp::eClear, face, mipLevel) };
                        ctx.BeginRender(graphicsState);

                        ctx.SetViewportState(ViewportState{}
                            .AddViewportAndScissorRect(Viewport(2540, 2540)));

                        ctx.BindPipeline(data.mSkyboxProjectionPipeline_Graphics);
                        ctx.Draw(3 );

                        ctx.EndRender();
                    }
                }
            } );
    }

    auto GeometryShadingModule::RegisterSkyboxRender( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SkyboxRender_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "Skybox_Vert.slang", FGStageType::eVertex )
            .PushShader( "Skybox_Frag.slang", FGStageType::ePixel ) };
        info.mSkyboxRenderPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "SkyboxRender",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                CameraModuleInfo& cameraData{ blackboard.Get<CameraModuleInfo>() };
                PrepassModuleInfo& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                GeomShadingModuleInfo& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.UseResource( finalCompData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prePassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );

                builder.UseResource(finalCompData.mBoxVertexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead);
                builder.UseResource(finalCompData.mBoxIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead);

                // We can either render this guy or the prefilter image
                builder.UseResource( finalCompData.mSkyboxCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mPrefilterCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( cameraData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                const auto& cameraData{ b.Get<CameraModuleInfo>() };
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& finalCompData{ b.Get<GeomShadingModuleInfo>() };

                // NOTE: the material has either a rectangular image or 6 cube images
                // instead of expecting an actual cube image we can just use the 6 faces in the
                // skybox render pass and avoid this pass as we do not need to project anything

                // Just need to prepare this pass to accept 6 images in case we happen to have 6 faces available instead

                struct DrawParams {
                    u32 mCameraBufferID{};
                    u32 mBasicSamplerID{};

                    u32 mVertexBufferID{};
                    u32 mIndexBufferID{};

                    u32 mSkyboxCubeRtID{};
                    u32 mPrefilterCubeRtID{};

                    u32 mBackGroundType{}; // Prefilter map, Original skybox, clear color

                    u32 mMaxMipLevel{};
                    f32 mExposure{ 1.0f };
                    f32 mGamma{ 2.0f };
                } params{
                    .mCameraBufferID = ctx.PushBuffer_SRV( cameraData.mCameraData ),
                    .mBasicSamplerID = ctx.PushSampler( finalCompData.mSkyboxCubeSampler ),

                    .mVertexBufferID = ctx.PushBuffer_SRV(finalCompData.mBoxVertexBuffer),
                    .mIndexBufferID = ctx.PushBuffer_SRV(finalCompData.mBoxIndexBuffer),

                    .mSkyboxCubeRtID = ctx.PushTexture_SRV( finalCompData.mSkyboxCubeRT ),
                    .mPrefilterCubeRtID = ctx.PushTexture_SRV( finalCompData.mPrefilterCubeRT ),

                    .mBackGroundType = as<u32>( mBackgroundType ),

                    .mMaxMipLevel = mPrefilterMipLevels };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>(dimensions.first), as<i32>(dimensions.second) } )
                    .AddDepthTarget( prePassData.mPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( finalCompData.mColorImage, kColorCyan, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>(dimensions.first), as<f32>(dimensions.second) ) ) );

                // For no clear color use background cube
                // otherwise just clear the image with specified solid color
                if (mBackgroundType != SceneBackgroundType::eClearColor) {
                    ctx.BindPipeline( finalCompData.mSkyboxRenderPipeline );
                    ctx.Draw( finalCompData.mBoxIndicesCount );
                }

                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::RegisterShading( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Resources:
        // https://github.khronos.org/Vulkan-Site/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/04_lighting_implementation.html

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "PBR_MetallicRoughness_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .SetCullMode( CullMode::eCullBack )
            .PushShader( "PBR_MetallicRoughness_Vert.slang", FGStageType::eVertex )
            .PushShader( "PBR_MetallicRoughness_Frag.slang", FGStageType::ePixel ) };
        info.mShadingPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "PBR_Radiance",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard & blackboard ) -> void {
                const auto& shadowMapData{ blackboard.Get<ShadowMapInfo>() };
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.UseResource( cameraData.mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mMaterialsBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( prePassData.mClusterBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( prePassData.mLightsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( finalCompData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prePassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );

                builder.UseResource( finalCompData.mBrdfColorTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mPrefilterCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mIrradianceCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );

                // SSAO stuff here

                // Shadow mapping stuff here
                for (const auto& map : shadowMapData.mDirShadowMaps) {
                    if (map.mHandle != 0) {
                        builder.UseResource( map, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                }
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) -> void {
                const auto& shadowMapData{ blackboard.Get<ShadowMapInfo>() };
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                auto& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };

                finalCompData.mExposure = mExposure;
                finalCompData.mGamma = mGamma;

                struct DrawParams {
                    SPointer mGeometryDescBuffer{};
                    SPointer mSkinningBuffer{};
                    SPointer mGeometryBuffer{};
                    SPointer mCameraBuffer{};

                    SPointer mMaterialsBuffer{};

                    SPointer mClusterBuffer{};
                    SPointer mLightsBuffer{};

                    // Split because push constant use st430
                    // float4 is messed up
                    float2 mGridSize1{};
                    float2 mGridSize2{};

                    u32 mEnableSsao{};

                    u32 mBrdfColorTargetID{};
                    u32 mPrefilterCubeRtID{};
                    u32 mIrradianceCubeRtID{};

                    u32 mSamplerCubeID{};
                    u32 mSamplerBasicID{};

                    u32 mActiveLightCount{};

                    u32 mPrefilteredCubeMipLevels{};
                    f32 mScaleIblAmbient{ 1.0f };

                    u32 mIsSkyboxActive{ MKT_SHADER_FALSE };

                    u32 mDirectionalShadowsInfoBufferID{};
                    u32 mDirectionalShadowCastersCount{};
                } params{
                    .mGeometryDescBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mSkinningBuffer = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),
                    .mGeometryBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mCameraBuffer = ctx.GetDeviceBufferAddress( cameraData.mCameraData ),

                    .mMaterialsBuffer = ctx.GetDeviceBufferAddress( geometryData.mMaterialsBuffer ),

                    .mClusterBuffer = ctx.GetDeviceBufferAddress( prePassData.mClusterBuffer ),
                    .mLightsBuffer = ctx.GetDeviceBufferAddress( prePassData.mLightsBuffer ),

                    .mGridSize1 = { prePassData.mGridSize[0], prePassData.mGridSize[1] },
                    .mGridSize2 = { prePassData.mGridSize[2], prePassData.mGridSize[3] },

                    .mEnableSsao = mEnableSsao ? MKT_SHADER_TRUE : MKT_SHADER_FALSE,

                    .mBrdfColorTargetID = ctx.PushTexture_SRV( finalCompData.mBrdfColorTarget ),
                    .mPrefilterCubeRtID = ctx.PushTexture_SRV( finalCompData.mPrefilterCubeRT ),
                    .mIrradianceCubeRtID = ctx.PushTexture_SRV( finalCompData.mIrradianceCubeRT ),

                    .mSamplerCubeID = ctx.PushSampler( finalCompData.mSkyboxCubeSampler ),
                    .mSamplerBasicID = ctx.PushSampler( finalCompData.mDefaultSampler ),

                    .mActiveLightCount = prePassData.mActiveLightCount,
                    .mPrefilteredCubeMipLevels = mPrefilterMipLevels,
                    .mScaleIblAmbient = mAbientScale,
                    .mIsSkyboxActive = mBackgroundType != SceneBackgroundType::eClearColor ? MKT_SHADER_TRUE : MKT_SHADER_FALSE,
                    .mDirectionalShadowsInfoBufferID = ctx.PushBuffer_SRV( shadowMapData.mDirShadowsBuffer ),
                    .mDirectionalShadowCastersCount = shadowMapData.mDirShadowCasterCount };
                ctx.PushConstants( params );

                LoadOp colorLoadOp{ LoadOp::eClear };
                if (mBackgroundType != SceneBackgroundType::eClearColor) {
                    colorLoadOp = LoadOp::eLoad;
                }

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>(dimensions.first), as<i32>(dimensions.second) } )
                    .AddDepthTarget( prePassData.mPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( finalCompData.mColorImage, mClearColor, colorLoadOp ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>(dimensions.first), as<f32>(dimensions.second) ) ) );

                ctx.BindPipeline( finalCompData.mShadingPipeline );

                mGeometryManagement->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::RegisterWireframe(FrameGraph& graph) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<WireframeData>() };

        auto dimensions{ InferDimensions( mResolution ) };

        // Color attachment for debugging
        auto colorImage{ FGTextureDescription{}
            .SetName( "Wireframe_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        info.mColorImage = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Wireframe_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetPolygonMode( PolygonMode::eLines )
            .SetDepthFormat( Format::eD32 )
            .SetCullMode( CullMode::eNone )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .PushShader( "Wireframe_Vert.slang", FGStageType::eVertex )
            .PushShader( "Wireframe_Frag.slang", FGStageType::ePixel ) };
        info.mPipeline = graph.Create( pipelineBuilder );

        // Hardware wireframe
        graph.RegisterPass(
            "WireframePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder&builder, Blackboard& blackboard ) {
                const auto& wireframeData{ blackboard.Get<WireframeData>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& cameraPassData{ blackboard.Get<CameraModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.UseResource( wireframeData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                builder.UseResource( finalCompData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                builder.UseResource( prePassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );

                builder.UseResource( cameraPassData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, Blackboard& b ) -> void {
                const auto& wireframeData{ b.Get<WireframeData>() };
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& cameraPassData{ b.Get<CameraModuleInfo>() };
                const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };

                struct DrawParams {
                    SPointer mGeometryBuffer{};
                    SPointer mSkinningBuffer{};
                    SPointer mGeometryAllocationBufferID{};

                    SPointer mCameraBuffer{};
                } params{
                    .mGeometryBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mSkinningBuffer = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),

                    .mGeometryAllocationBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mCameraBuffer = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData ) };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prePassData.mPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( wireframeData.mColorImage, kColorWhite, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.SetPolygonLineWidth( 3.0f );

                ctx.BindPipeline( wireframeData.mPipeline );

                mGeometryManagement->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );
    }
}
