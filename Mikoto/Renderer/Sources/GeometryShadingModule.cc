//    Copyright 2026 ケイト
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

#include <EASTL/utility.h>
#include <EASTL/bit.h>
#include <EASTL/array.h>
#include <EASTL/fixed_hash_map.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Blackboard.hh>

#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/VolumeModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/SubsurfaceScatteringModule.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

namespace mikoto::renderer {

    using namespace mikoto::scene;

    GeometryShadingModule::GeometryShadingModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto GeometryShadingModule::SetEnvironmentAllocated( bool allocated ) -> void {
        // A 1024 HDR cube retains detail without the old 2540-square allocation.
        // Prefiltered reflections need less resolution than the visible sky.
        mSkyboxDimensions = allocated ? 2048 : 1u;
        mPrefilterDimensions = allocated ? 512 : 1u;

        // std::bit_width(n) returns how many bits are needed to
        // represent a positive integer each mip level halves the
        // dimension, and halving an integer is equivalent to
        // shifting its bits right once.
        mSkyboxMipLevels = eastl::bit_width( mSkyboxDimensions );
        mPrefilterMipLevels = eastl::bit_width( mPrefilterDimensions );
    }

    auto GeometryShadingModule::SetScene( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScene = scene;
    }

    auto GeometryShadingModule::SetCamera( const scene::Camera* camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto GeometryShadingModule::SetGeometryManager( GeometryCullModule& geom ) -> void {
        MKT_BEGIN_PROFILER();
        mGeometryManagement = MKT_ADDRESSOF( geom );
    }

    auto GeometryShadingModule::SetMultisampling( rhi::Multisampling samples ) -> void {
        mMultisampling = samples;
    }

    auto GeometryShadingModule::SetMergeWireframeToFinalOutput( bool merge ) -> void {
        MKT_BEGIN_PROFILER();
        mMergeWireframeToFinalImage = merge;
    }

    auto GeometryShadingModule::SetClearColor( const renderer::rhi::Color& color ) -> void {
        MKT_BEGIN_PROFILER();
        mClearColor = color;
    }

    auto GeometryShadingModule::SetResolution( renderer::rhi::RenderResolution resolution ) -> void {
        MKT_BEGIN_PROFILER();
        mResolution = resolution;
    }

    auto GeometryShadingModule::SetSkyboxMaterial( material::MaterialHandle material ) -> void {
        MKT_BEGIN_PROFILER();
        mSkyboxMaterial = material;
    }

    auto GeometryShadingModule::SetRenderBackground( SceneBackgroundType bg ) -> void {
        MKT_BEGIN_PROFILER();
        mBackgroundType = bg;
    }

    auto GeometryShadingModule::SetEnvironmentLightingEnabled( bool enabled ) -> void {
        MKT_BEGIN_PROFILER();
        mEnvironmentLightingEnabled = enabled;
    }

    auto GeometryShadingModule::SetAtmosphereEnabled( bool enabled ) -> void {
        mAtmosphereEnabled = enabled;
    }

    auto GeometryShadingModule::SetEnableSsao( bool enable ) -> void {
        MKT_BEGIN_PROFILER();
        mEnableSsao = enable;
    }

    auto GeometryShadingModule::IsSsaoEnabled() const -> bool {
        return mEnableSsao;
    }

    auto GeometryShadingModule::SetSsaoIntensity( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();
        mSsaoIntensity = value;
    }

    auto GeometryShadingModule::GetSsaoIntensity() const -> core::f32 {
        return mSsaoIntensity;
    }

    auto GeometryShadingModule::RegisterIrradiance( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "Irradiance_CubeRT" )
            .SetWidth( core::as<core::i32>( kIrradianceDimensions ) )
            .SetHeight( core::as<core::i32>( kIrradianceDimensions ) )
            .SetMipCount( core::as<core::i32>( kIrradianceMipLevels ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTextureCube )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA32_FLOAT ) };// For HDR values
        info.mIrradianceCubeRT = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Irradiance_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .PushShader( "Irradiance_Vert.slang", FGStageType::eVertex )
            .PushShader( "Irradiance_Frag.slang", FGStageType::ePixel ) };
        info.mIrradiancePipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "IrradiancePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, GeomShadingModuleInfo& data ) {
                b.UseResource( data.mIrradianceCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mSkyboxCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( data.mAtmosphereCube, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( data.mBoxVertexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                b.UseResource( data.mBoxIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    core::float4x4 mMvp{};
                    core::f32 mDeltaPhi{};
                    core::f32 mDeltaTheta{};

                    core::u32 mSkyboxCubeID{};
                    core::u32 mBasicSamplerID{};

                    core::u32 mVertexBufferID{};
                    core::u32 mIndexBufferID{};
                } params{
                    .mSkyboxCubeID = ctx.PushTexture_SRV( mAtmosphereEnabled ? data.mAtmosphereCube : data.mSkyboxCubeRT ),
                    .mBasicSamplerID = ctx.PushSampler( data.mSkyboxCubeSampler ),

                    .mVertexBufferID = ctx.PushBuffer_SRV( data.mBoxVertexBuffer ),
                    .mIndexBufferID = ctx.PushBuffer_SRV( data.mBoxIndexBuffer )
                };

                for ( core::u32 mipLevel{}; mipLevel < kIrradianceMipLevels; mipLevel++ ) {
                    for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        core::f32 width{ core::as<core::f32>( kIrradianceDimensions * glm::pow( 0.5f, core::as<core::f32>( mipLevel ) ) ) };
                        core::f32 heigh{ core::as<core::f32>( kIrradianceDimensions * glm::pow( 0.5f, core::as<core::f32>( mipLevel ) ) ) };

                        params.mDeltaTheta = 0.5f * math::constants::kPi / ( mAtmosphereEnabled ? 16.0f : 64.0f );
                        params.mDeltaPhi = 2.0f * math::constants::kPi / ( mAtmosphereEnabled ? 32.0f : 180.0f );
                        params.mMvp = glm::perspective( core::as<core::f32>( math::constants::kPi / 2.0 ),
                                          1.0f, 0.1f, 512.0f ) *
                                      kMatrices[face];

                        ctx.PushConstants( params );

                        auto graphicsState{ RenderBeginDescription{}
                            .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( heigh ) } )
                            .AddRenderTarget( data.mIrradianceCubeRT, renderer::rhi::kColorCyan, renderer::rhi::LoadOp::eClear, face, mipLevel ) };
                        ctx.BeginRender( graphicsState );

                        ctx.SetViewportState( renderer::rhi::ViewportState{}
                            .AddViewportAndScissorRect( renderer::rhi::Viewport( width, heigh ) ) );

                        ctx.BindPipeline( data.mIrradiancePipeline );
                        ctx.Draw( data.mBoxIndicesCount );

                        ctx.EndRender();
                    }
                }
            } );
    }

    auto GeometryShadingModule::RegisterSkyboxGeometryUpload( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();
        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        mBoxModel = asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( "Resources/Prefabs/box/Box.gltf" );

        info.mBoxVerticesCount = mBoxModel->GetMeshNode( 0 ).GetVertexBuffer()->GetSizeBytes() / MKT_SIZEOF( asset::VertexDescription_Std430Alignment );
        info.mBoxIndicesCount = mBoxModel->GetMeshNode( 0 ).GetIndexBuffer()->GetSizeBytes() / MKT_SIZEOF( core::u32 );

        auto vertexDesc{ FGBufferDescription{}
            .SetName( "GeometryShadingBox_Vertices" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( info.mBoxVerticesCount, MKT_SIZEOF( asset::VertexDescription_Std430Alignment ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        info.mBoxVertexBuffer = graph.Create( vertexDesc );

        auto indexDesc{ FGBufferDescription{}
            .SetName( "GeometryShadingBox_Indices" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( info.mBoxIndicesCount, MKT_SIZEOF( core::u32 ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        info.mBoxIndexBuffer = graph.Create( indexDesc );

        // Declare both sides of the copy so externally uploaded model buffers
        // participate in the graph's transfer dependencies too.
        const auto vertices{ graph.ImportBuffer( mBoxModel->GetMeshNode( 0 ).GetVertexBuffer() ) };
        const auto indices{ graph.ImportBuffer( mBoxModel->GetMeshNode( 0 ).GetIndexBuffer() ) };
        graph.RegisterPass<GeomShadingModuleInfo>(
            "GeometryShadingBox_Upload",
            FGPassType::eTransfer,
            [vertices, indices]( FGNodeBuilder& b, GeomShadingModuleInfo& data ) {
                b.UseResource( vertices, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                b.UseResource( indices, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                b.UseResource( data.mBoxVertexBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mBoxIndexBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [vertices, indices]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                ctx.CopyBuffer( data.mBoxVertexBuffer, vertices );
                ctx.CopyBuffer( data.mBoxIndexBuffer, indices );
            } );

        graph.SetExecutionPolicy( "GeometryShadingBox_Upload", FGExecutionPolicy::eOnce );
    }

    auto GeometryShadingModule::RegisterPrefilter( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto iblSampler{ FGSamplerDescription{}
            .SetName( "IBLCubeSampler_Sampler01" )
            .SetMipLevels( core::as<core::f32>( glm::max( mSkyboxMipLevels, mPrefilterMipLevels ) - 1 ) )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToEdge )
            .SetBorderColor( renderer::rhi::kColorBlack ) };
        info.mSkyboxCubeSampler = graph.Create( iblSampler );

        auto colorImage{ FGTextureDescription{}
            .SetName( "Prefilter_CubeRT" )
            .SetWidth( core::as<core::i32>( mPrefilterDimensions ) )
            .SetHeight( core::as<core::i32>( mPrefilterDimensions ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTextureCube )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetMipCount( mPrefilterMipLevels )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT ) };// For HDR values
        info.mPrefilterCubeRT = graph.Create( colorImage );

        // A smooth, disk-free atmosphere does not need the panorama's 1024
        // resolution or 1024 samples. Keep a smaller complete mip chain for
        // interactive sun edits without changing imported-environment quality.
        colorImage
            .SetName( "Atmosphere_PrefilterCube" )
            .SetWidth( 128 )
            .SetHeight( 128 )
            .SetMipCount( 8 );
        info.mAtmospherePrefilter = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "Prefilter_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "Prefilter_Vert.slang", FGStageType::eVertex )
            .PushShader( "Prefilter_Frag.slang", FGStageType::ePixel ) };
        info.mPrefilterPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "PrefilterPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, GeomShadingModuleInfo& info ) {
                b.UseResource( info.mPrefilterCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( info.mAtmospherePrefilter, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( info.mSkyboxCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( info.mAtmosphereCube, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                b.UseResource( info.mBoxVertexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                b.UseResource( info.mBoxIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                struct DrawParams {
                    core::float4x4 mMvp{};
                    core::f32 mRoughness{};
                    core::u32 mNumSamples{};

                    core::u32 mSkyboxCubeID{};
                    core::u32 mBasicSamplerID{};

                    core::u32 mVertexBufferID{};
                    core::u32 mIndexBufferID{};
                } params{
                    .mNumSamples = mAtmosphereEnabled ? 64u : 1024u,
                    .mSkyboxCubeID = ctx.PushTexture_SRV( mAtmosphereEnabled ? data.mAtmosphereCube : data.mSkyboxCubeRT ),
                    .mBasicSamplerID = ctx.PushSampler( data.mSkyboxCubeSampler ),

                    .mVertexBufferID = ctx.PushBuffer_SRV( data.mBoxVertexBuffer ),
                    .mIndexBufferID = ctx.PushBuffer_SRV( data.mBoxIndexBuffer )
                };

                const core::u32 mipCount{ mAtmosphereEnabled ? 8u : mPrefilterMipLevels };
                const core::u32 dimension{ mAtmosphereEnabled ? 128u : mPrefilterDimensions };

                for ( core::u32 mipLevel{}; mipLevel < mipCount; mipLevel++ ) {
                    params.mRoughness = core::as<core::f32>( mipLevel ) / core::as<core::f32>( eastl::max( 1u, mipCount - 1 ) );

                    for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        core::f32 width{ core::as<core::f32>( dimension * glm::pow( 0.5f, core::as<core::f32>( mipLevel ) ) ) };
                        core::f32 heigh{ core::as<core::f32>( dimension * glm::pow( 0.5f, core::as<core::f32>( mipLevel ) ) ) };

                        params.mMvp = glm::perspective( core::as<core::f32>( math::constants::kPi / 2.0 ), 1.0f, 0.1f, 512.0f ) * kMatrices[face];

                        ctx.PushConstants( params );

                        auto graphicsState{ RenderBeginDescription{}
                            .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( heigh ) } )
                            .AddRenderTarget( mAtmosphereEnabled ? data.mAtmospherePrefilter : data.mPrefilterCubeRT, renderer::rhi::kColorCyan, renderer::rhi::LoadOp::eClear, face, mipLevel ) };
                        ctx.BeginRender( graphicsState );

                        ctx.SetViewportState( renderer::rhi::ViewportState{}
                            .AddViewportAndScissorRect( renderer::rhi::Viewport( width, heigh ) ) );

                        ctx.BindPipeline( data.mPrefilterPipeline );
                        ctx.Draw( data.mBoxIndicesCount );

                        ctx.EndRender();
                    }
                }
            } );
    }

    auto GeometryShadingModule::RegisterBrdfLut( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        GeomShadingModuleInfo& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "BRDFLut_ColorImage01" )
            .SetWidth( core::as<core::i32>( 512 ) )
            .SetHeight( core::as<core::i32>( 512 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRG16_FLOAT ) };
        info.mBrdfColorTarget = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "BRDFLut_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( renderer::rhi::Format::eRG16_FLOAT )
            .PushShader( "BRDFLut_Vert.slang", FGStageType::eVertex )
            .PushShader( "BRDFLut_Frag.slang", FGStageType::ePixel ) };
        info.mBrdfPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "BRDFLut",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, GeomShadingModuleInfo& info ) {
                b.UseResource( info.mBrdfColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            []( CommandContext& ctx, core::Blackboard& b ) -> void {
                const auto& info{ b.Get<GeomShadingModuleInfo>() };
                auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ 512, 512 } )
                    .AddRenderTarget( info.mBrdfColorTarget, renderer::rhi::kColorCyan, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( 512, 512 ) ) );

                ctx.BindPipeline( info.mBrdfPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::SetExposure( float value ) -> void {
        MKT_BEGIN_PROFILER();
        mExposure = value;
    }

    auto GeometryShadingModule::SetAmbientScale( core::f32 ambient ) -> void {
        MKT_BEGIN_PROFILER();
        mAbientScale = ambient;
    }

    auto GeometryShadingModule::SetGamma( float value ) -> void {
        MKT_BEGIN_PROFILER();
        mGamma = value;
    }

    auto GeometryShadingModule::SetSkyboxBlur( core::f32 blur ) -> void {
        MKT_BEGIN_PROFILER();
        mSkyboxBlur = blur;
    }

    auto GeometryShadingModule::RegisterSkyboxProjection( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto samplerDesc{ FGSamplerDescription{}
            .SetName( "SkyboxProjection_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eRepeat )
            .SetBorderColor( renderer::rhi::kColorWhite ) };
        info.mDefaultSampler = graph.Create( samplerDesc );

        auto skyboxCube{ FGTextureDescription{}
            .SetName( "SkyboxProjection_CubeRT" )
            .SetWidth( mSkyboxDimensions )
            .SetHeight( mSkyboxDimensions )
            .SetMipCount( mSkyboxMipLevels )
            .SetDimensions( renderer::rhi::TextureDimension::eTextureCube )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::CopyDest )
            .SetFormat( renderer::rhi::Format::eRGBA32_FLOAT ) };// For HDR values
        info.mSkyboxCubeRT = graph.Create( skyboxCube );
        info.mAtmosphereCube = graph.GetOrCreate<AtmosphereModuleInfo>().mEnvironmentCube;

        info.mSkyboxProjectionPipeline_FlatImage = graph.Create( FGPipelineDescription{}
            .SetName( "SkyboxProjection_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .PushShader( "SkyboxProjection_Vert.slang", FGStageType::eVertex )
            .PushShader( "SkyboxProjection_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "SkyboxProjection",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, GeomShadingModuleInfo& data ) {
                builder.UseResource( data.mSkyboxCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( data.mBoxVertexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( data.mBoxIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                if ( mSkyboxMaterial.IsEmpty() ) {
                    return;
                }

                auto* material{ core::checked_cast<material::SkyboxMaterial*>( mSkyboxMaterial.GetPtr() ) };

                if ( !material->IsType( material::SkyboxType::eEquirectangular ) || material->GetEquirectangular().IsEmpty() ) {
                    return;
                }

                const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto panorama{ ctx.ImportTexture( material->GetEquirectangular() ) };
                struct DrawParams {
                    core::float4x4 mMvp{};
                    core::u32 mBasicSamplerID{};
                    core::u32 mEquirectangularID{};
                    core::u32 mVertexBufferID{};
                    core::u32 mIndexBufferID{};
                } params{
                    .mBasicSamplerID = ctx.PushSampler( data.mDefaultSampler ),
                    .mEquirectangularID = ctx.PushTexture_SRV( panorama ),
                    .mVertexBufferID = ctx.PushBuffer_SRV( data.mBoxVertexBuffer ),
                    .mIndexBufferID = ctx.PushBuffer_SRV( data.mBoxIndexBuffer )
                };

                for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                    params.mMvp = glm::perspective( core::as<core::f32>( math::constants::kPi / 2.0 ), 1.0f, 0.1f, 512.0f ) * kMatrices[face];
                    ctx.PushConstants( params );
                    ctx.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( mSkyboxDimensions ), core::as<core::i32>( mSkyboxDimensions ) } )
                        .AddRenderTarget( data.mSkyboxCubeRT, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear, face, 0 ) );

                    ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( mSkyboxDimensions ),
                                core::as<core::f32>( mSkyboxDimensions ) ) ) );

                    ctx.BindPipeline( data.mSkyboxProjectionPipeline_FlatImage );
                    ctx.Draw( data.mBoxIndicesCount );
                    ctx.EndRender();
                }
            } );

        info.mSkyboxProjectionPipeline_Graphics = graph.Create( FGPipelineDescription{}
            .SetName( "SkyboxProjectionGraphics_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .PushShader( "SkyboxProjectionGraphics_Vert.slang", FGStageType::eVertex )
            .PushShader( "SkyboxProjectionGraphics_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass<GeomShadingModuleInfo>(
            "SkyboxProjection_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, GeomShadingModuleInfo& data ) {
                builder.UseResource( data.mSkyboxCubeRT, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                if ( mSkyboxMaterial.IsEmpty() ) {
                    return;
                }

                auto* material{ core::checked_cast<material::SkyboxMaterial*>( mSkyboxMaterial.GetPtr() ) };

                if ( !material->IsType( material::SkyboxType::eCubeFaces ) ) {
                    return;
                }

                // Cubemap array order is +X, -X, +Y, -Y, +Z, -Z. Face images
                // use top-left UVs; Front is -Z in Mikoto's camera convention.
                constexpr material::SkyboxFace faces[]{
                    material::SkyboxFace::eRight,
                    material::SkyboxFace::eLeft,
                    material::SkyboxFace::eTop,
                    material::SkyboxFace::eBottom,
                    material::SkyboxFace::eBack,
                    material::SkyboxFace::eFront
                };

                eastl::array<FGTextureHandle, rhi::kMaxCubeFaces> inputs{};
                const auto& textures{ material->GetFaceTextures() };
                // Validate every named face before writing any output. Do not
                // retain handles from a previous material or insert missing keys.
                for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                    const auto found{ textures.find( faces[face] ) };
                    const auto resource{ found == textures.end() ? core::Ref<asset::Texture>{} : found->second.GetIfReady() };

                    if ( !resource ) {
                        MKT_CORE_LOGGER_WARN( "Skybox requires all six named faces" );
                        return;
                    }

                    inputs[face] = ctx.ImportTexture( resource->GetResource() );
                }

                const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };

                for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                    struct DrawParams {
                        core::u32 mSamplerID{};
                        core::u32 mTextureID{};
                        core::u32 mDimension{};
                    } params{
                        .mSamplerID = ctx.PushSampler( data.mSkyboxCubeSampler ),
                        .mTextureID = ctx.PushTexture_SRV( inputs[face] ),
                        .mDimension = mSkyboxDimensions
                    };

                    ctx.PushConstants( params );
                    ctx.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( mSkyboxDimensions ), core::as<core::i32>( mSkyboxDimensions ) } )
                        .AddRenderTarget( data.mSkyboxCubeRT, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear, face, 0 ) );

                    ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( mSkyboxDimensions ),
                                core::as<core::f32>( mSkyboxDimensions ) ) ) );

                    ctx.BindPipeline( data.mSkyboxProjectionPipeline_Graphics );
                    ctx.Draw( 3 );
                    ctx.EndRender();
                }
            } );
    }


    auto GeometryShadingModule::SetSkyboxPassesEnabled( FrameGraph& graph, bool enabled ) -> void {
        MKT_BEGIN_PROFILER();
        // Atmosphere mode requests this every frame. Do not recull the graph
        // for an already-disabled group; enabling always wakes cached passes.
        if ( !enabled && mSkyboxPassesDisabled ) {
            return;
        }

        mSkyboxPassesDisabled = !enabled;
        auto setEnabled{ [&]( eastl::string_view name ) {
            if ( enabled ) {
                graph.EnablePass( name );
            }
            else {
                graph.DisablePass( name );
            }
        } };
        setEnabled( "SkyboxProjection" );
        setEnabled( "SkyboxProjection_Graphics" );

        for ( core::u32 mip{ 1 }; mip < mSkyboxMipLevels; ++mip ) {
            setEnabled( string::Format( "SkyboxMipmaps_Mip{}_Render", mip ) );
            setEnabled( string::Format( "SkyboxMipmaps_Mip{}_Copy", mip ) );
        }
    }


    auto GeometryShadingModule::RegisterSkyboxMipmaps( FrameGraph& graph ) -> void {
        // Projection writes only the base level of the source cubemap. GGX
        // prefiltering chooses a source LOD from each importance sample's PDF and
        // solid-angle footprint; it needs averaged radiance at those source levels.
        //
        // Without this chain, a few samples can hit tiny, very bright HDR texels
        // without averaging their larger footprints. This can bake yellow/white
        // dots ("fireflies") into the prefiltered map and therefore its reflections.
        // Allocating mip levels alone does not populate them.
        //
        // These are ordinary SOURCE mips. RegisterPrefilter separately generates
        // the roughness-dependent OUTPUT mips; generating those does not replace
        // this step. Register after projection and before prefiltering/skybox reads.
        //
        // OpenGL can generate this source chain with glGenerateMipmap after the
        // base faces are populated; the driver manages the internal mip ordering.
        // https://learnopengl.com/PBR/IBL/Specular-IBL
        // Vulkan has no single equivalent call, but formats supporting filtered
        // blits can use vkCmdBlitImage2 directly between successive image mips,
        // with barriers between levels. This would remove scratch and copy passes,
        // but requires mip-level graph tracking to manage those transitions.
        // https://docs.vulkan.org/tutorial/latest/09_Generating_Mipmaps.html
        // The 22-pass count below comes from this implementation, not an API limit.
        //
        // Each mip has one render pass covering all six scratch faces, followed by
        // one copy pass publishing those faces to the source cube. Whole-texture
        // tracking requires separate source and destination textures; UseResource
        // handles the render/copy transitions between passes. The next mip samples
        // only after all six faces are copied. A 2048 cube therefore uses 22 passes
        // for its 11 generated mips, rather than 132 passes split by face.
        // The passes sleep after execution and wake when the environment changes.

        MKT_BEGIN_PROFILER();

        const auto cube{ graph.GetOrCreate<GeomShadingModuleInfo>().mSkyboxCubeRT };
        // Keep all six downsampled faces until the copy pass. Reuse a half-size,
        // single-level scratch cube; smaller mips use its upper-left region.
        const auto scratch{ graph.Create( FGTextureDescription{}
            .SetName( "SkyboxMip_Scratch" )
            .SetWidth( eastl::max( 1u, mSkyboxDimensions / 2 ) )
            .SetHeight( eastl::max( 1u, mSkyboxDimensions / 2 ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTextureCube )
            .SetFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::CopySource ) ) };

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "SkyboxMip_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .AddColorFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .PushShader( "SkyboxProjectionGraphics_Vert.slang", FGStageType::eVertex )
            .PushShader( "SkyboxMip_Frag.slang", FGStageType::ePixel ) ) };

        mSkyboxPassesDisabled = false;

        // Batch face operations inside each pass while keeping mip dependencies
        // and resource transitions visible to the graph.
        for ( core::u32 mip{ 1 }; mip < mSkyboxMipLevels; ++mip ) {
            const core::u32 dimension{ glm::max( 1u, mSkyboxDimensions >> mip ) };

            const auto renderName{ string::Format( "SkyboxMipmaps_Mip{}_Render", mip ) };
            graph.RegisterPass(
                renderName,
                FGPassType::eGraphics,
                [cube, scratch]( FGNodeBuilder& builder, core::Blackboard& ) {
                    builder.UseResource( cube, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( scratch, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                },
                [this, cube, scratch, pipeline, mip, dimension]( CommandContext& ctx, core::Blackboard& blackboard ) {
                    if ( mSkyboxMaterial.IsEmpty() || mAtmosphereEnabled ) {
                        return;
                    }

                    const auto& data{ blackboard.Get<GeomShadingModuleInfo>() };
                    struct DrawParams {
                        core::u32 mCubeID{};
                        core::u32 mSamplerID{};
                        core::u32 mSourceMip{};
                        core::u32 mFace{};
                        core::u32 mSourceDimension{};
                        core::u32 mDestinationDimension{};
                    } params{
                        .mCubeID = ctx.PushTexture_SRV( cube ),
                        .mSamplerID = ctx.PushSampler( data.mSkyboxCubeSampler ),
                        .mSourceMip = mip - 1,
                        .mSourceDimension = glm::max( 1u, mSkyboxDimensions >> ( mip - 1 ) ),
                        .mDestinationDimension = dimension
                    };

                    for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        params.mFace = face;
                        ctx.PushConstants( params );
                        ctx.BeginRender( RenderBeginDescription{}
                            .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimension ), core::as<core::i32>( dimension ) } )
                            .AddRenderTarget( scratch, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear, face ) );

                        ctx.SetViewportState( renderer::rhi::ViewportState{}
                            .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimension ),
                                    core::as<core::f32>( dimension ) ) ) );
                        ctx.BindPipeline( pipeline );
                        ctx.Draw( 3 );
                        ctx.EndRender();
                    }
                } );

            graph.SetExecutionPolicy( renderName, FGExecutionPolicy::eOnWake );

            const auto copyName{ string::Format( "SkyboxMipmaps_Mip{}_Copy", mip ) };
            graph.RegisterPass(
                copyName,
                FGPassType::eTransfer,
                [cube, scratch]( FGNodeBuilder& builder, core::Blackboard& ) {
                    builder.UseResource( scratch, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                    builder.UseResource( cube, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                },
                [this, cube, scratch, mip, dimension]( CommandContext& ctx, core::Blackboard& ) {
                    if ( mSkyboxMaterial.IsEmpty() || mAtmosphereEnabled ) {
                        return;
                    }

                    for ( core::u32 face{}; face < rhi::kMaxCubeFaces; ++face ) {
                        const renderer::rhi::TextureSlice source{ .mWidth = dimension, .mHeight = dimension, .mArrayLayer = face };
                        const renderer::rhi::TextureSlice destination{ .mWidth = dimension, .mHeight = dimension, .mMipLevel = mip, .mArrayLayer = face };

                        ctx.CopyTexture( scratch, source, cube, destination );
                    }
                } );

            graph.SetExecutionPolicy( copyName, FGExecutionPolicy::eOnWake );
        }
    }

    auto GeometryShadingModule::RegisterSkyboxRender( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        // Create shared resources
        const auto dimensions{ InferDimensions( mResolution ) };
        auto colorImage{ FGTextureDescription{}
            .SetName( "FinalShading_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget |
                           renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::CopyDest | renderer::rhi::TextureUsageFlagsBits::CopySource )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT ) };// Will support HDR values
        info.mColorImage = graph.Create( colorImage );

        info.mRasterColorImage = info.mColorImage;
        info.mRasterDepthImage = graph.GetData<PrepassModuleInfo>().mPrepassDepthTarget;

        if ( mMultisampling != renderer::rhi::Multisampling::eMsaaX1 ) {
            info.mRasterColorImage = graph.Create( FGTextureDescription{}
                .SetName( "Shading_MSAA_Color" )
                .SetWidth( dimensions.first )
                .SetHeight( dimensions.second )
                .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT )
                .SetMultisampling( mMultisampling )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::CopySource ) );

            info.mRasterDepthImage = graph.Create( FGTextureDescription{}
                .SetName( "Shading_MSAA_Depth" )
                .SetWidth( dimensions.first )
                .SetHeight( dimensions.second )
                .SetFormat( renderer::rhi::Format::eD32 )
                .SetMultisampling( mMultisampling )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget ) );
        }

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SkyboxRender_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetMultisampling( mMultisampling )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "Skybox_Vert.slang", FGStageType::eVertex )
            .PushShader( "Skybox_Frag.slang", FGStageType::ePixel ) };
        info.mSkyboxRenderPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "SkyboxRender",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                CameraModuleInfo& cameraData{ blackboard.Get<CameraModuleInfo>() };
                PrepassModuleInfo& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                GeomShadingModuleInfo& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };

                builder.UseResource( finalCompData.mRasterColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( finalCompData.mRasterDepthImage, FGPipelineStage::eDepthTarget, mMultisampling == renderer::rhi::Multisampling::eMsaaX1 ? FGResourceAccess::eRead : FGResourceAccess::eWrite );

                builder.UseResource( finalCompData.mBoxVertexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mBoxIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                // We can either render this guy or the prefilter image
                builder.UseResource( finalCompData.mSkyboxCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mPrefilterCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( cameraData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) -> void {
                const auto& cameraData{ b.Get<CameraModuleInfo>() };
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& finalCompData{ b.Get<GeomShadingModuleInfo>() };

                // NOTE: the material has either a rectangular image or 6 cube images
                // instead of expecting an actual cube image we can just use the 6 faces in the
                // skybox render pass and avoid this pass as we do not need to project anything

                // Just need to prepare this pass to accept 6 images in case we happen to have 6 faces available instead

                struct DrawParams {
                    core::u32 mCameraBufferID{};
                    core::u32 mBasicSamplerID{};

                    core::u32 mVertexBufferID{};
                    core::u32 mIndexBufferID{};

                    core::u32 mSkyboxCubeRtID{};
                    core::u32 mPrefilterCubeRtID{};

                    core::u32 mBackGroundType{};// Prefilter map, Original skybox, clear color

                    core::u32 mMaxMipLevel{};
                    core::f32 mExposure{ 1.0f };
                    core::f32 mGamma{ 2.0f };
                    core::f32 mSkyboxBlur{ .5f };
                } params{
                    .mCameraBufferID = ctx.PushBuffer_SRV( cameraData.mCameraData ),
                    .mBasicSamplerID = ctx.PushSampler( finalCompData.mSkyboxCubeSampler ),

                    .mVertexBufferID = ctx.PushBuffer_SRV( finalCompData.mBoxVertexBuffer ),
                    .mIndexBufferID = ctx.PushBuffer_SRV( finalCompData.mBoxIndexBuffer ),

                    .mSkyboxCubeRtID = ctx.PushTexture_SRV( finalCompData.mSkyboxCubeRT ),
                    .mPrefilterCubeRtID = ctx.PushTexture_SRV( finalCompData.mPrefilterCubeRT ),

                    .mBackGroundType = core::as<core::u32>( mBackgroundType ),

                    .mMaxMipLevel = mPrefilterMipLevels,
                    .mSkyboxBlur = mSkyboxBlur
                };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddDepthTarget( finalCompData.mRasterDepthImage, mMultisampling == renderer::rhi::Multisampling::eMsaaX1 ? renderer::rhi::LoadOp::eLoad : renderer::rhi::LoadOp::eClear )
                    .AddRenderTarget( finalCompData.mRasterColorImage, mClearColor, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                // For no clear color use background cube
                // otherwise just clear the image with specified solid color
                if ( !mAtmosphereEnabled && !mSkyboxMaterial.IsEmpty() && mBackgroundType != SceneBackgroundType::eClearColor ) {
                    ctx.BindPipeline( finalCompData.mSkyboxRenderPipeline );
                    ctx.Draw( finalCompData.mBoxIndicesCount );
                }

                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::RegisterTransparentPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();
        RegisterShading( graph, true );
    }

    auto GeometryShadingModule::RegisterSubsurfaceLighting( FrameGraph& graph ) -> void {

        const auto dimensions{ InferDimensions( mResolution ) };
        const core::u32 width{ core::as<core::u32>( dimensions.first ) };
        const core::u32 height{ core::as<core::u32>( dimensions.second ) };
        const auto texture{ [&]( core::cstr name, renderer::rhi::Format format ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( width )
                .SetHeight( height )
                .SetFormat( format )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget |
                               renderer::rhi::TextureUsageFlagsBits::ShaderResource |
                               renderer::rhi::TextureUsageFlagsBits::CopySource ) );
        } };

        // Float32 alpha preserves exact material/instance identities in large scenes.
        const auto original{ texture( "Subsurface_Diffuse", renderer::rhi::Format::eRGBA32_FLOAT ) };
        const auto metadata{ texture( "Subsurface_IdentityDepth", renderer::rhi::Format::eRGBA32_FLOAT ) };
        const auto specular{ texture( "Subsurface_Specular", renderer::rhi::Format::eRGBA16_FLOAT ) };
        const auto ambient{ texture( "Subsurface_Ambient", renderer::rhi::Format::eRGBA16_FLOAT ) };

        auto& info{ graph.GetOrCreate<SubsurfaceScatteringInfo>() };
        info.mDiffuse = original;
        info.mIdentityDepth = metadata;
        info.mSpecular = specular;
        info.mAmbient = ambient;

        RegisterShading( graph, false, true );
    }

    auto GeometryShadingModule::RegisterContours( FrameGraph& graph ) -> void {
        RegisterShading( graph, false, false, true );

        const auto& prepass{ graph.GetData<PrepassModuleInfo>() };
        const auto mask{ prepass.mGBufferContourTarget };
        const auto normal{ prepass.mGBufferNormalTarget };
        const auto position{ prepass.mGBufferPositionTarget };
        const auto target{ graph.GetData<GeomShadingModuleInfo>().mRasterColorImage };
        const auto camera{ graph.GetData<CameraModuleInfo>().mCameraData };
        const auto dimensions{ InferDimensions( mResolution ) };

        const core::u32 width{ core::as<core::u32>( dimensions.first ) };
        const core::u32 height{ core::as<core::u32>( dimensions.second ) };
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Stylized_Creases_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( true )
            .SetMultisampling( mMultisampling )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "ThumbnailFullscreen_Vert.slang", FGStageType::eVertex )
            .PushShader( "StylizedCrease_Frag.slang", FGStageType::ePixel ) ) };

        graph.RegisterPass(
            "Stylized_Creases",
            FGPassType::eGraphics,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                for ( auto texture: { mask, normal, position } ) {
                    builder.UseResource( texture, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                builder.UseResource( camera, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( target, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [=, this]( CommandContext& ctx, core::Blackboard& ) {
                if ( !mGeometryManagement->HasStylizedContours() ) {
                    return;
                }

                struct Parameters {
                    SPointer mCamera{};
                    core::u32 mMask{};
                    core::u32 mNormal{};
                    core::u32 mPosition{};
                    core::u32 mWidth{};
                    core::u32 mHeight{};
                    core::u32 mPadding{};
                } params{
                    .mCamera = ctx.GetDeviceBufferAddress( camera ),
                    .mMask = ctx.PushTexture_SRV( mask ),
                    .mNormal = ctx.PushTexture_SRV( normal ),
                    .mPosition = ctx.PushTexture_SRV( position ),
                    .mWidth = width,
                    .mHeight = height,
                    .mPadding = 0
                };

                ctx.PushConstants( params );
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ),
                            core::as<core::i32>( height ) } )
                    .AddRenderTarget( target, renderer::rhi::Color{ 0, 0, 0, 0 }, renderer::rhi::LoadOp::eLoad ) );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport{ core::as<core::f32>( width ), core::as<core::f32>( height ) } ) );

                ctx.BindPipeline( pipeline );
                ctx.Draw( 3 );
                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::RegisterShading( FrameGraph& graph, bool transparent, bool subsurface, bool contour ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Resources:
        // https://github.khronos.org/Vulkan-Site/tutorial/latest/Building_a_Simple_Engine/Lighting_Materials/04_lighting_implementation.html

        auto& info{ graph.GetOrCreate<GeomShadingModuleInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( contour ? "Stylized_Contours_Pipeline" : subsurface ? "Subsurface_Lighting_Pipeline"
                                                               : transparent  ? "PBR_Transparency_Pipeline"
                                                                              : "PBR_MetallicRoughness_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetBlendEnable( transparent )

                // Diffusion uses one unambiguous material/instance per pixel. Its
                // result is applied to MSAA geometry before the scene resolve.
            .SetMultisampling( subsurface ? renderer::rhi::Multisampling::eMsaaX1 : mMultisampling )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( subsurface ? renderer::rhi::Format::eRGBA32_FLOAT : renderer::rhi::Format::eRGBA16_FLOAT )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthWrite( !subsurface && !contour && !transparent && mMultisampling != renderer::rhi::Multisampling::eMsaaX1 )
            .SetDepthTest( true )
            .PushShader( contour ? "StylizedContour_Vert.slang" : "PBR_MetallicRoughness_Vert.slang", FGStageType::eVertex )
            .PushShader( contour ? "StylizedContour_Frag.slang" : subsurface ? "SubsurfaceLighting_Frag.slang"
                                                                                 : "PBR_MetallicRoughness_Frag.slang",
                    FGStageType::ePixel ) };

        if ( subsurface ) {
            pipelineBuilder.AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT ).AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT ).AddColorFormat( renderer::rhi::Format::eRGBA32_FLOAT );
        }

        const auto pipeline = graph.Create( pipelineBuilder );

        if ( !contour && !transparent && !subsurface ) {
            info.mShadingPipeline = pipeline;
        }

        graph.RegisterPass(
            contour ? "Stylized_Contours" : subsurface ? "Subsurface_Lighting"
                                        : transparent  ? "PBR_Transparency"
                                                       : "PBR_Radiance",
            FGPassType::eGraphics,
            [this, transparent, subsurface, contour]( FGNodeBuilder& builder, core::Blackboard& blackboard ) -> void {
                const auto& shadowMapData{ blackboard.Get<ShadowMapInfo>() };
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& finalCompData{ blackboard.Get<GeomShadingModuleInfo>() };

                // Fragment shading reads camera position and clustering data as
                // well as the vertex transform; synchronize both consumers.
                builder.UseResource( cameraData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( cameraData.mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mAttributeBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mDrawInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                if ( contour ) {
                    builder.UseResource( geometryData.mMaterialsBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                }

                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningCacheBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( prePassData.mClusterBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( prePassData.mLightsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                if ( const auto* atmosphere{ blackboard.TryGet<AtmosphereModuleInfo>() }; atmosphere && !contour ) {
                    builder.UseResource( atmosphere->mParameters, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( atmosphere->mTransmittance, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }

                builder.UseResource( subsurface ? blackboard.Get<SubsurfaceScatteringInfo>().mDiffuse : finalCompData.mRasterColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                if ( subsurface ) {
                    const auto& sss{ blackboard.Get<SubsurfaceScatteringInfo>() };

                    for ( auto target: { sss.mSpecular, sss.mAmbient, sss.mIdentityDepth } ) {
                        builder.UseResource( target, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                    }
                }

                builder.UseResource( subsurface ? prePassData.mPrepassDepthTarget : finalCompData.mRasterDepthImage,
                    FGPipelineStage::eDepthTarget, ( subsurface || contour || transparent || mMultisampling == renderer::rhi::Multisampling::eMsaaX1 ) ? FGResourceAccess::eRead : FGResourceAccess::eWrite );

                builder.UseResource( finalCompData.mBrdfColorTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mPrefilterCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mIrradianceCubeRT, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( finalCompData.mAtmospherePrefilter, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );

                builder.UseResource( blackboard.Get<PostProcessModuleInfo>().mSsaoBlurColorTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                if ( blackboard.Contains<RayTracingInfo>() ) {
                    const auto& ray{ blackboard.Get<RayTracingInfo>() };
                    builder.UseResource( ray.mDirect, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( ray.mIndirect, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( ray.mAmbientOcclusion, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                // Shadow mapping stuff here
                if ( const auto* volumes{ blackboard.TryGet<VolumeModuleInfo>() } ) {
                    builder.UseResource( volumes->mData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                if ( const auto* fog{ blackboard.TryGet<FogModuleInfo>() }; fog && transparent ) {
                    builder.UseResource( fog->mFrame, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( fog->mRadiance, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( fog->mTransmittance, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                builder.UseResource( shadowMapData.mRecordsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                for ( const auto& map: shadowMapData.mMaps ) {
                    if ( map.mHandle != 0 ) {
                        builder.UseResource( map, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                }
            },
            [this, pipeline, transparent, subsurface, contour]( CommandContext& ctx, core::Blackboard& blackboard ) -> void {
                if ( contour && !mGeometryManagement->HasStylizedContours() ) {
                    return;
                }

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
                    core::float2 mGridSize1{};
                    core::f32 mGridSizeZ{};
                    core::u32 mRayIndirect{ 0xffffffffu };

                    core::u32 mClusterBufferSize{};
                    core::f32 mSsaoIntensity{};

                    core::u32 mSsaoTextureID{};

                    core::u32 mBrdfColorTargetID{};
                    core::u32 mPrefilterCubeRtID{};
                    core::u32 mIrradianceCubeRtID{};

                    core::u32 mSamplerCubeID{};
                    core::u32 mSamplerBasicID{};

                    core::u32 mActiveLightCount{};

                    // Low 29 bits: mip count; bits 29/30: RT specular/diffuse;
                    // bit 31: environment lighting enabled. Keep 128-byte ABI.
                    core::u32 mPrefilterMipLevelsAndSkybox{};
                    core::f32 mScaleIblAmbient{ 1.0f };

                    core::u32 mRayDirect{ 0xffffffffu };

                    core::u32 mShadowInfoBufferID{};
                    core::u32 mShadowRecordCount{};
                } params{
                    .mGeometryDescBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mSkinningBuffer = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),
                    .mGeometryBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mCameraBuffer = ctx.GetDeviceBufferAddress( cameraData.mCameraData ),

                    .mMaterialsBuffer = ctx.GetDeviceBufferAddress( geometryData.mMaterialsBuffer ),

                    .mClusterBuffer = ctx.GetDeviceBufferAddress( prePassData.mClusterBuffer ),
                    .mLightsBuffer = ctx.GetDeviceBufferAddress( prePassData.mLightsBuffer ),

                    .mGridSize1 = { prePassData.mGridSize[0], prePassData.mGridSize[1] },
                    .mGridSizeZ = prePassData.mGridSize[2],

                    .mClusterBufferSize = prePassData.mClusterCount,
                    .mSsaoIntensity = mSsaoIntensity,

                    .mSsaoTextureID = mEnableSsao && !transparent ? ctx.PushTexture_SRV( blackboard.Get<PostProcessModuleInfo>().mSsaoBlurColorTarget ) : 0xFFFFFFFFu,

                    .mBrdfColorTargetID = ctx.PushTexture_SRV( finalCompData.mBrdfColorTarget ),
                    .mPrefilterCubeRtID = ctx.PushTexture_SRV( mAtmosphereEnabled ? finalCompData.mAtmospherePrefilter : finalCompData.mPrefilterCubeRT ),
                    .mIrradianceCubeRtID = ctx.PushTexture_SRV( finalCompData.mIrradianceCubeRT ),

                    .mSamplerCubeID = ctx.PushSampler( finalCompData.mSkyboxCubeSampler ),
                    .mSamplerBasicID = ctx.PushSampler( geometryData.mMaterialSampler ),

                    .mActiveLightCount = prePassData.mActiveLightCount,
                    .mPrefilterMipLevelsAndSkybox = ( mAtmosphereEnabled ? 8u : mPrefilterMipLevels ) |
                                                    ( mEnvironmentLightingEnabled && ( mAtmosphereEnabled || !mSkyboxMaterial.IsEmpty() ) ? 0x80000000u : 0u ),
                    .mScaleIblAmbient = mAbientScale,
                    // Camera clear flags control the visible background only.
                    // An environment still lights/reflects on objects over a solid color.
                    .mShadowInfoBufferID = ctx.PushBuffer_SRV( shadowMapData.mRecordsBuffer ),
                    .mShadowRecordCount = shadowMapData.mRecordCount
                };

                if ( !transparent && blackboard.Contains<RayTracingInfo>() ) {
                    const auto& ray{ blackboard.Get<RayTracingInfo>() };

                    if ( ray.mReady && ray.mAmbientOcclusionEnabled ) {
                        // PBRT's visibility estimate replaces SSAO for IBL; transport GI stays untouched.
                        params.mSsaoTextureID = ctx.PushTexture_SRV( ray.mAmbientOcclusion );
                        params.mSsaoIntensity = 1.0f;
                    }

                    if ( ray.mReady && ray.mShadows ) {
                        params.mRayDirect = ctx.PushTexture_SRV( ray.mDirect );
                    }

                    if ( ray.mReady && ( ray.mReflect || ray.mGlobalIllumination ) ) {
                        params.mRayIndirect = ctx.PushTexture_SRV( ray.mIndirect );
                        params.mPrefilterMipLevelsAndSkybox |= ( ray.mReflect ? 0x20000000u : 0u ) |
                                                               ( ray.mGlobalIllumination ? 0x40000000u : 0u );
                    }
                }

                // Match PBR_TRANSPARENT_SURFACE_PASS in Material_Helpers.
                // Opaque shading also visits BLEND meshes, retaining only
                // alpha-one pixels; the blend pass takes the remainder.
                constexpr core::u32 kTransparentSurfacePass{ 1u << 28 };

                if ( transparent ) {
                    params.mPrefilterMipLevelsAndSkybox |= kTransparentSurfacePass;
                }

                ctx.PushConstants( params );

                rhi::LoadOp colorLoadOp{ renderer::rhi::LoadOp::eClear };

                if ( !subsurface && ( contour || transparent || mBackgroundType != SceneBackgroundType::eClearColor ) ) {
                    colorLoadOp = renderer::rhi::LoadOp::eLoad;
                }

                const auto dimensions{ InferDimensions( mResolution ) };
                auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddDepthTarget( subsurface ? prePassData.mPrepassDepthTarget : finalCompData.mRasterDepthImage, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( subsurface ? blackboard.Get<SubsurfaceScatteringInfo>().mDiffuse : finalCompData.mRasterColorImage,
                            subsurface ? renderer::rhi::Color{ 0, 0, 0, 0 } : mClearColor, colorLoadOp ) };

                if ( subsurface ) {
                    const auto& sss{ blackboard.Get<SubsurfaceScatteringInfo>() };

                    for ( auto target: { sss.mSpecular, sss.mAmbient, sss.mIdentityDepth } ) {
                        graphicsState.AddRenderTarget( target, renderer::rhi::Color{ 0, 0, 0, 0 }, renderer::rhi::LoadOp::eClear );
                    }
                }

                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( pipeline );

                mGeometryManagement->DrawInstancesIndirect( ctx, transparent ? GeometryDrawGroup::eTransparent : GeometryDrawGroup::eAll );

                ctx.EndRender();
            } );
    }

    auto GeometryShadingModule::RegisterResolve( FrameGraph& graph, bool transparent ) -> void {
        if ( mMultisampling == renderer::rhi::Multisampling::eMsaaX1 ) {
            return;
        }

        graph.RegisterPass<GeomShadingModuleInfo>(
            transparent ? "Shading_Transparent_MSAA_Resolve" : "Shading_MSAA_Resolve",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, GeomShadingModuleInfo& info ) {
                builder.UseResource( info.mRasterColorImage, FGPipelineStage::eResolve, FGResourceAccess::eRead );
                builder.UseResource( info.mColorImage, FGPipelineStage::eResolve, FGResourceAccess::eWrite );
            },
            []( CommandContext& ctx, core::Blackboard& b ) {
                const auto& info{ b.Get<GeomShadingModuleInfo>() };
                ctx.ResolveTexture( info.mRasterColorImage, info.mColorImage );
            } );
    }

    auto GeometryShadingModule::RegisterWireframe( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<WireframeData>() };

        auto dimensions{ InferDimensions( mResolution ) };

        // Color attachment for debugging
        auto colorImage{ FGTextureDescription{}
            .SetName( "Wireframe_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        info.mColorImage = graph.Create( colorImage );

        auto defaultWireframePipelineBuilder{ FGPipelineDescription{}
            .SetName( "DefaultWireframe_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetPolygonMode( renderer::rhi::PolygonMode::eLines )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .SetDepthWrite( false )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .PushShader( "Wireframe_Vert.slang", FGStageType::eVertex )
            .PushShader( "Wireframe_Frag.slang", FGStageType::ePixel ) };
        info.mDefaultPipeline = graph.Create( defaultWireframePipelineBuilder );

        auto outputMergeWireframePipelineBuilder{ FGPipelineDescription{}
            .SetName( "OutputMergeWireframe_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetPolygonMode( renderer::rhi::PolygonMode::eLines )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .SetDepthWrite( false )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "Wireframe_Vert.slang", FGStageType::eVertex )
            .PushShader( "Wireframe_Frag.slang", FGStageType::ePixel ) };
        info.mOutputMergePipeline = graph.Create( outputMergeWireframePipelineBuilder );

        // Hardware wireframe
        graph.RegisterPass(
            "WireframePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
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
                builder.UseResource( geometryData.mInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mDrawInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningCacheBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mAttributeBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) -> void {
                const auto& wireframeData{ b.Get<WireframeData>() };
                const auto& finalCompData{ b.Get<GeomShadingModuleInfo>() };
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& cameraPassData{ b.Get<CameraModuleInfo>() };
                const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };

                struct DrawParams {
                    SPointer mGeometryBuffer{};
                    SPointer mSkinningBuffer{};
                    SPointer mGeometryAllocationBufferID{};

                    SPointer mCameraBuffer{};

                    core::float4 mLineColor{ .0f, .0f, .0f, 1.0f };
                } params{
                    .mGeometryBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mSkinningBuffer = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),

                    .mGeometryAllocationBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mCameraBuffer = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData )
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto drawWireframe{ [&]( FGTextureHandle target, FGPipelineHandle pipeline, renderer::rhi::LoadOp loadOp ) {
                    auto graphicsState{ RenderBeginDescription{}
                        .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                        .AddDepthTarget( prePassData.mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad )
                        .AddRenderTarget( target, renderer::rhi::kColorWhite, loadOp ) };
                    ctx.BeginRender( graphicsState );

                    ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ),
                                core::as<core::f32>( dimensions.second ) ) ) );

                    ctx.SetPolygonLineWidth( 1.0f );
                    ctx.BindPipeline( pipeline );

                    mGeometryManagement->DrawInstancesIndirect( ctx );

                    ctx.EndRender();
                } };

                // Keep the selectable wireframe image current even while the
                // polygon-complexity overlay also draws into the shaded image.
                drawWireframe( wireframeData.mColorImage, wireframeData.mDefaultPipeline, renderer::rhi::LoadOp::eClear );

                if ( mMergeWireframeToFinalImage ) {
                    drawWireframe( finalCompData.mColorImage, wireframeData.mOutputMergePipeline, renderer::rhi::LoadOp::eLoad );
                }
            } );
    }
}
