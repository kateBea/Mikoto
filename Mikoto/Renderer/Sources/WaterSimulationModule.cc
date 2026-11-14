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

#include <cmath>
#include <random>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Material/SkyboxMaterial.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/WaterSimulationModule.hh>

#include <Scene/Camera.hh>
#include <Scene/Component.hh>

namespace mikoto::renderer {
    // https://www.bealto.com/gpu-fft.html
    // https://arm-software.github.io/opengl-es-sdk-for-android/ocean_f_f_t.html
    // https://3dmodelizm.ru/ext/graphics.ucsd.edu/courses/rendering/2005/jdewall/tessendorf.pdf
    // https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-18-using-vertex-texture-displacement

    using namespace mikoto::core;

    static constexpr u32 kFftResolution{ 256 }; // Keep in sync with WaterFFT_Helpers.slang.
    static constexpr u32 kFftTileSize{ 8 };
    static constexpr u32 kFftSampleCount{ kFftResolution * kFftResolution };
    static constexpr f32 kWaterGravity{ 9.81f };
    static constexpr f64 kFftLoopSeconds{ 600.0 }; // Frequencies are snapped to this period.

    // Keep these records in sync with base/Water_Helpers.slang. Using float4
    // groups avoids float3 array-stride differences between shader backends.
    struct WaterWaveGPU {
        core::float4 mDirectionAmplitudeFrequency{};
        core::float4 mPhaseHorizontal{};
    };
    struct WaterSurfaceGPU {
        core::float4x4 mWorld{};
        core::float4 mColorRoughness{};
        core::float4 mAttenuation{};// absorption color, attenuation distance
        core::float4 mOptics{};     // IOR, transmission, thickness, foam amount
        core::uint4 mGeometry{};    // first vertex, first index, columns, overlay flags
        core::uint4 mSimulation{};  // first wave, count, simulation type, FFT sample offset
        core::float4 mDebugPreviewColor{};
        core::float4 mFft{};       // reciprocal patch length, remaining lanes reserved
    };

    struct WaterFftSpectrumParameters {
        core::u64 mInitialSpectrum{};
        core::u64 mOutput{};
        core::u32 mInitialOffset{};
        core::u32 mOutputOffset{};
        core::f32 mPatchLength{};
        core::f32 mTime{};
        core::f32 mChoppiness{};
    };

    struct WaterFftTransformParameters {
        core::u64 mInput{};
        core::u64 mOutput{};
        core::u32 mSampleOffset{};
        core::u32 mColumns{};
    };

    struct WaterFftGradientParameters {
        core::u64 mDisplacement{};
        core::u64 mGradients{};
        core::u32 mSampleOffset{};
        core::f32 mInverseDoubleSpacing{};
    };

    static auto CreateWaterFftBuffer( FrameGraph& graph, cstr name ) -> FGBufferHandle {
        return graph.Create( FGBufferDescription{}
            .SetName( name )
            .SetSizeBytes( sizeof( core::float4 ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured |
                rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );
    }

    // Tessendorf §3.4 builds h0 from a Phillips spectrum and independent Gaussian samples.
    // https://3dmodelizm.ru/ext/graphics.ucsd.edu/courses/rendering/2005/jdewall/tessendorf.pdf
    // Arm's sample likewise builds the initial spectrum on the CPU and evolves it on the GPU.
    // https://arm-software.github.io/opengl-es-sdk-for-android/ocean_f_f_t.html
    static auto GenerateWaterSpectrum( const scene::WaterSimulationComponent::FftSettings& settings,
        eastl::vector<core::float2>& spectrum ) -> void {
        constexpr f32 shortWaveDampingFraction{ 0.001f };
        constexpr f32 opposingWaveEnergy{ 0.07f }; // Weaker waves travelling against the wind.
        constexpr f32 phillipsScale{ 0.008f };    // Artist amplitude scales the resulting height.

        const f32 angle{ glm::radians( settings.mWindDirection ) };
        const core::float2 wind{ glm::cos( angle ), glm::sin( angle ) };
        const f32 largestWave{ settings.mWindSpeed * settings.mWindSpeed / kWaterGravity };
        const f32 dampingLength{ largestWave * shortWaveDampingFraction };
        const f32 waveNumberStep{ glm::two_pi<f32>() / settings.mPatchLength };

        // A fixed seed reproduces the same patch when the scene is reopened.
        std::mt19937 random{ settings.mSeed };
        std::normal_distribution<f32> gaussian{ 0.0f, 1.0f };
        spectrum.assign( kFftSampleCount, core::float2{} );

        if ( largestWave == 0.0f || settings.mAmplitude == 0.0f ) {
            return;
        }

        // Store frequencies in natural FFT order with zero at index zero.
        for ( u32 row{}; row < kFftResolution; ++row ) {
            for ( u32 column{}; column < kFftResolution; ++column ) {
                // Skip Nyquist lines. Their sign is ambiguous for horizontal displacement.
                constexpr u32 nyquist{ kFftResolution / 2 };
                if ( ( row == 0 && column == 0 ) || row == nyquist || column == nyquist ) {
                    continue;
                }

                const i32 x{ as<i32>( column ) - ( column > nyquist ? as<i32>( kFftResolution ) : 0 ) };
                const i32 z{ as<i32>( row ) - ( row > nyquist ? as<i32>( kFftResolution ) : 0 ) };
                const core::float2 waveNumber{ core::float2{ as<f32>( x ), as<f32>( z ) } * waveNumberStep };
                const f32 waveNumberSquared{ glm::dot( waveNumber, waveNumber ) };
                const f32 alignment{ glm::dot( waveNumber * glm::inversesqrt( waveNumberSquared ), wind ) };

                const f32 longWaveFalloff{ std::exp( -1.0f / ( waveNumberSquared * largestWave * largestWave ) ) };
                const f32 shortWaveFalloff{ std::exp( -waveNumberSquared * dampingLength * dampingLength ) };
                const f32 directionWeight{ alignment * alignment * ( alignment < 0.0f ? opposingWaveEnergy : 1.0f ) };
                const f32 power{ phillipsScale * longWaveFalloff * shortWaveFalloff * directionWeight /
                    ( waveNumberSquared * waveNumberSquared ) };

                // Delta k accounts for the frequency sample area. The IFFT evaluates the Fourier sum directly.
                const f32 scale{ settings.mAmplitude * waveNumberStep * std::sqrt( 0.5f * power ) };
                spectrum[row * kFftResolution + column] = core::float2{ gaussian( random ), gaussian( random ) } * scale;
            }
        }
    }

    WaterSimulationModule::WaterSimulationModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto WaterSimulationModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto WaterSimulationModule::SetScene( const scene::Scene* scene ) -> void {
        if ( mScene != scene ) {
            mPlanes.clear();
            mUploadedSpectra.clear();
            mFftPatches.clear();
        }

        mScene = scene;
    }

    auto WaterSimulationModule::RegisterWaterSimulation( FrameGraph& graph, bool hdr ) -> void {
        MKT_BEGIN_PROFILER();

        mPlanes.clear();
        mFftPatches.clear();
        mUploadedSpectra.clear();

        auto& info{ graph.GetOrCreate<WaterSimulationModuleInfo>() };
        info.mVertices = graph.Create( FGBufferDescription{}
            .SetName( "Water_Vertices" )
            .SetSizeBytes( MKT_SIZEOF( core::float3 ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured |
                           rhi::BufferUsageFlagsBits::CopySource |
                           rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mIndices = graph.Create( FGBufferDescription{}
            .SetName( "Water_Indices" )
            .SetSizeBytes( MKT_SIZEOF( core::u32 ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured |
                           rhi::BufferUsageFlagsBits::CopySource |
                           rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mWaves = graph.Create( FGBufferDescription{}
            .SetName( "Water_Waves" )
            .SetSizeBytes( sizeof( WaterWaveGPU ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | 
                rhi::BufferUsageFlagsBits::CopySource | 
                rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );

        info.mSurfaces = graph.Create( FGBufferDescription{}
            .SetName( "Water_Surfaces" )
            .SetSizeBytes( sizeof( WaterSurfaceGPU ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | 
                rhi::BufferUsageFlagsBits::CopySource | 
                rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) );
        
        info.mFftRows = CreateWaterFftBuffer( graph, "Water_FftRows" );
        info.mFftSpectrum = CreateWaterFftBuffer( graph, "Water_FftSpectrum" );
        info.mFftGradients = CreateWaterFftBuffer( graph, "Water_FftGradients" );
        info.mFftDisplacement = CreateWaterFftBuffer( graph, "Water_FftDisplacement" );
        info.mFftInitialSpectrum = CreateWaterFftBuffer( graph, "Water_FftInitialSpectrum" );

        const auto colorTarget{ hdr ? graph.GetData<GeomShadingModuleInfo>().mColorImage : graph.GetData<PresentationPassData>().mColorImage };

        if ( hdr ) {
            const auto [width, height]{ rhi::InferDimensions( mResolution ) };
            auto snapshot{ FGTextureDescription{}
                .SetWidth( width )
                .SetHeight( height )
                .SetDimensions( rhi::TextureDimension::eTexture2D )
                .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | 
                    rhi::TextureUsageFlagsBits::CopyDest ) };

            info.mSceneColor = graph.Create( snapshot
                .SetName( "Water_SceneColor" )
                .SetFormat( rhi::Format::eRGBA16_FLOAT ) );

            info.mSceneDepth = graph.Create( snapshot
                .SetName( "Water_SceneDepth" )
                .SetFormat( rhi::Format::eD32 ) );

            info.mSampler = graph.Create( FGSamplerDescription{}
                .SetName( "Water_Sampler" )
                .SetFilter( rhi::SamplerFilter::eLinear )
                .SetWrap( rhi::SamplerWrapMode::eClampToEdge ) );
        }

        info.mPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Water_Plane" )
            .SetPipelineType( rhi::PipelineType::eGraphics )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetDepthFormat( rhi::Format::eD32 )
            .SetBlendEnable( false )
            .AddColorFormat( hdr ? rhi::Format::eRGBA16_FLOAT : rhi::Format::eBGRA8_UNORM )
            .PushShader( "WaterPlane_Vert.slang", FGStageType::eVertex )
            .PushShader( "WaterPlane_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "WaterSimulation_Prepare",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& info{ b.Get<WaterSimulationModuleInfo>() };

                for ( const auto handle: { info.mVertices, info.mIndices, info.mWaves, info.mSurfaces } ) {
                    builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }

                builder.UseResource( info.mFftInitialSpectrum, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                mUploads.Clear();
                FilterWaterSimulators( ctx, b );
            } );

        graph.RegisterPass(
            "WaterSimulation_Filter",
            FGPassType::eTransfer,
            [hdr, colorTarget]( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& waterData{ b.Get<WaterSimulationModuleInfo>() };

                builder.UseResource( waterData.mVertices, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( waterData.mIndices, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( waterData.mWaves, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( waterData.mSurfaces, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( waterData.mFftInitialSpectrum, FGPipelineStage::eCopy, FGResourceAccess::eWrite );

                if ( hdr ) {
                    builder.UseResource( colorTarget, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                    builder.UseResource( waterData.mSceneColor, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                    builder.UseResource( waterData.mSceneDepth, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this, hdr, colorTarget]( CommandContext& ctx, core::Blackboard& b ) {
                mUploads.Submit( ctx );

                if ( hdr && !mPlanes.empty() ) {
                    const auto& info{ b.Get<WaterSimulationModuleInfo>() };
                    const auto [width, height]{ rhi::InferDimensions( mResolution ) };
                    const rhi::TextureSlice slice{ .mWidth = static_cast<core::u32>( width ), .mHeight = static_cast<core::u32>( height ) };
                    ctx.CopyTexture( colorTarget, slice, info.mSceneColor, slice );
                    ctx.CopyTexture( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, slice, info.mSceneDepth, slice );
                }
            } );

        // Wicked Engine separates spectrum evolution, inverse FFT, displacement and gradients.
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiOcean.cpp
        RegisterFftSpectrumPass( graph );
        RegisterFftRowsPass( graph );
        RegisterFftColumnsPass( graph );
        RegisterFftGradientsPass( graph );

        graph.RegisterPass(
            "WaterSimulation_Render",
            FGPassType::eGraphics,
            [hdr, colorTarget]( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& waterData{ b.Get<WaterSimulationModuleInfo>() };

                builder.UseResource( waterData.mVertices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( waterData.mIndices, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( waterData.mWaves, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( waterData.mWaves, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( waterData.mSurfaces, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( waterData.mSurfaces, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( waterData.mFftDisplacement, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( waterData.mFftGradients, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                builder.UseResource( colorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                if ( hdr ) {
                    builder.UseResource( waterData.mSceneColor, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    builder.UseResource( waterData.mSceneDepth, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    const auto& lighting{ b.Get<GeomShadingModuleInfo>() };
                    builder.UseResource( b.Get<PrepassModuleInfo>().mLightsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                    for ( const auto texture: { lighting.mPrefilterCubeRT, lighting.mAtmospherePrefilter, lighting.mIrradianceCubeRT, lighting.mBrdfColorTarget } ) {
                        builder.UseResource( texture, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                    }
                }
            },
            [this, hdr, colorTarget]( CommandContext& ctx, core::Blackboard& b ) {
                if ( !mScene || !mCamera || mPlanes.empty() ) {
                    return;
                }

                const auto& info{ b.Get<WaterSimulationModuleInfo>() };
                const auto [width, height]{ rhi::InferDimensions( mResolution ) };

                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( rhi::Rect{ static_cast<core::i32>( width ), static_cast<core::i32>( height ) } )
                    .AddRenderTarget( colorTarget, rhi::Color{}, rhi::LoadOp::eLoad )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, rhi::LoadOp::eLoad ) );

                ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect(
                    rhi::Viewport( static_cast<float>( width ), static_cast<float>( height ) ) ) );

                ctx.BindPipeline( info.mPipeline );

                struct Parameters {
                    SPointer mVertices{};
                    SPointer mIndices{};
                    SPointer mCamera{};
                    SPointer mSurfaces{};
                    SPointer mWaves{};
                    SPointer mLights{};

                    core::u32 mSurface{};
                    core::u32 mLightCount{};

                    core::u32 mEnvironment{ 0xffffffffu };
                    core::u32 mIrradiance{ 0xffffffffu };
                    core::u32 mBrdf{ 0xffffffffu };
                    core::u32 mSampler{};

                    core::f32 mEnvironmentScale{ 1.0f };
                    core::i32 mToneMap{ -1 };

                    core::u32 mSceneColor{ 0xffffffffu };
                    core::u32 mSceneDepth{ 0xffffffffu };
                    core::u32 mScreenSampler{};

                    SPointer mFftDisplacement{};
                    SPointer mFftGradients{};
                } params{};

                params.mVertices = ctx.GetDeviceBufferAddress( info.mVertices );
                params.mIndices = ctx.GetDeviceBufferAddress( info.mIndices );
                params.mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData );
                params.mSurfaces = ctx.GetDeviceBufferAddress( info.mSurfaces );
                params.mWaves = ctx.GetDeviceBufferAddress( info.mWaves );
                params.mFftDisplacement = ctx.GetDeviceBufferAddress( info.mFftDisplacement );
                params.mFftGradients = ctx.GetDeviceBufferAddress( info.mFftGradients );

                if ( hdr ) {
                    params.mSceneColor = ctx.PushTexture_SRV( info.mSceneColor );
                    params.mSceneDepth = ctx.PushTexture_SRV( info.mSceneDepth );
                    params.mScreenSampler = ctx.PushSampler( info.mSampler );
                    const auto& lighting{ b.Get<GeomShadingModuleInfo>() };
                    const auto& prepass{ b.Get<PrepassModuleInfo>() };
                    params.mLights = ctx.GetDeviceBufferAddress( prepass.mLightsBuffer );
                    params.mLightCount = prepass.mActiveLightCount;
                    const bool atmosphere{ mScene->GetEnvironmentLightingSource() == scene::EnvironmentLightingSource::eAtmosphere };
                    const auto environment{ mScene->GetEnvironmentMaterial() };

                    if ( atmosphere || ( mScene->GetEnvironmentLightingSource() == scene::EnvironmentLightingSource::eEnvironmentMap && environment ) ) {
                        params.mEnvironment = ctx.PushTexture_SRV( atmosphere ? lighting.mAtmospherePrefilter : lighting.mPrefilterCubeRT );
                        params.mIrradiance = ctx.PushTexture_SRV( lighting.mIrradianceCubeRT );
                        params.mBrdf = ctx.PushTexture_SRV( lighting.mBrdfColorTarget );
                        params.mSampler = ctx.PushSampler( lighting.mSkyboxCubeSampler );
                        params.mEnvironmentScale = atmosphere ? 1.0f : environment.As<material::SkyboxMaterial>()->GetAmbientScale();
                    }
                }
                else {
                    params.mToneMap = static_cast<core::i32>( mScene->GetToneMapping() );
                }

                for ( const auto& plane: mPlanes ) {
                    ctx.PushConstants( params );
                    ctx.Draw( plane.mIndexCount );
                    ++params.mSurface;
                }

                ctx.EndRender();
            } );
    }

    auto WaterSimulationModule::RegisterFftSpectrumPass( FrameGraph& graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Water_FftSpectrumPipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "WaterFFT_Spectrum_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "WaterSimulation_FftSpectrum",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& info{ blackboard.Get<WaterSimulationModuleInfo>() };
                builder.UseResource( info.mFftInitialSpectrum, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mFftSpectrum, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext& ctx, core::Blackboard& blackboard ) {
                if ( mFftPatches.empty() ) {
                    return;
                }

                const auto& info{ blackboard.Get<WaterSimulationModuleInfo>() };
                const auto initialSpectrum{ ctx.GetDeviceBufferAddress( info.mFftInitialSpectrum ) };
                const auto output{ ctx.GetDeviceBufferAddress( info.mFftSpectrum ) };

                ctx.BindPipeline( pipeline );

                for ( u32 index{}; index < mFftPatches.size(); ++index ) {
                    const auto& patch{ mFftPatches[index] };
                    const auto offset{ index * kFftSampleCount };
                    const WaterFftSpectrumParameters params{
                        .mInitialSpectrum = initialSpectrum,
                        .mOutput = output,
                        .mInitialOffset = offset,
                        .mOutputOffset = offset,
                        .mPatchLength = patch.mSettings.mPatchLength,
                        .mTime = as<f32>( std::fmod( patch.mTime, kFftLoopSeconds ) ),
                        .mChoppiness = patch.mSettings.mChoppiness
                    };

                    ctx.PushConstants( params );
                    ctx.Dispatch( kFftResolution / kFftTileSize, kFftResolution / kFftTileSize, 1 );
                }
            } );
    }

    auto WaterSimulationModule::RegisterFftRowsPass( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<WaterSimulationModuleInfo>() };
        info.mFftTransformPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "Water_FftTransformPipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "WaterFFT_Transform_Comp.slang", FGStageType::eCompute ) );

        RegisterFftTransformPass( graph, "WaterSimulation_FftRows", info.mFftTransformPipeline, info.mFftSpectrum, info.mFftRows, false );
    }

    auto WaterSimulationModule::RegisterFftColumnsPass( FrameGraph& graph ) -> void {
        const auto& info{ graph.GetData<WaterSimulationModuleInfo>() };
        RegisterFftTransformPass( graph, "WaterSimulation_FftColumns", info.mFftTransformPipeline, info.mFftRows, info.mFftDisplacement, true );
    }

    auto WaterSimulationModule::RegisterFftTransformPass( FrameGraph& graph, cstr name, FGPipelineHandle pipeline,
        FGBufferHandle input, FGBufferHandle output, bool columns ) -> void {
        graph.RegisterPass(
            name,
            FGPassType::eCompute,
            [input, output]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline, input, output, columns]( CommandContext& ctx, core::Blackboard& ) {
                if ( mFftPatches.empty() ) {
                    return;
                }

                const auto source{ ctx.GetDeviceBufferAddress( input ) };
                const auto destination{ ctx.GetDeviceBufferAddress( output ) };
                ctx.BindPipeline( pipeline );

                // A group transforms a full row or column. Separate passes synchronize the two axes.
                for ( u32 index{}; index < mFftPatches.size(); ++index ) {
                    const WaterFftTransformParameters params{
                        .mInput = source,
                        .mOutput = destination,
                        .mSampleOffset = index * kFftSampleCount,
                        .mColumns = as<u32>( columns )
                    };

                    ctx.PushConstants( params );
                    ctx.Dispatch( kFftResolution, 1, 1 );
                }
            } );
    }

    auto WaterSimulationModule::RegisterFftGradientsPass( FrameGraph& graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Water_FftGradientsPipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "WaterFFT_Gradients_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "WaterSimulation_FftGradients",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& info{ blackboard.Get<WaterSimulationModuleInfo>() };
                builder.UseResource( info.mFftDisplacement, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mFftGradients, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext& ctx, core::Blackboard& blackboard ) {
                if ( mFftPatches.empty() ) {
                    return;
                }

                const auto& info{ blackboard.Get<WaterSimulationModuleInfo>() };
                const auto displacement{ ctx.GetDeviceBufferAddress( info.mFftDisplacement ) };
                const auto gradients{ ctx.GetDeviceBufferAddress( info.mFftGradients ) };
                ctx.BindPipeline( pipeline );

                for ( u32 index{}; index < mFftPatches.size(); ++index ) {
                    const WaterFftGradientParameters params{
                        .mDisplacement = displacement,
                        .mGradients = gradients,
                        .mSampleOffset = index * kFftSampleCount,
                        .mInverseDoubleSpacing = kFftResolution / ( 2.0f * mFftPatches[index].mSettings.mPatchLength )
                    };

                    ctx.PushConstants( params );
                    ctx.Dispatch( kFftResolution / kFftTileSize, kFftResolution / kFftTileSize, 1 );
                }
            } );
    }

    auto WaterSimulationModule::PrepareFftInputs( CommandContext& ctx, const WaterSimulationModuleInfo& info ) -> void {
        const auto samples{ mFftPatches.size() * kFftSampleCount };

        if ( samples == 0 ) {
            return;
        }

        // Growing a graph buffer replaces its backing allocation and discards its old contents.
        const bool spectrumReplaced{ ctx.EnsureBufferCapacity( info.mFftInitialSpectrum, samples * sizeof( core::float2 ) ) };
        ctx.EnsureBufferCapacity( info.mFftSpectrum, samples * sizeof( core::float4 ) );
        ctx.EnsureBufferCapacity( info.mFftRows, samples * sizeof( core::float4 ) );
        ctx.EnsureBufferCapacity( info.mFftDisplacement, samples * sizeof( core::float4 ) );
        ctx.EnsureBufferCapacity( info.mFftGradients, samples * 2 * sizeof( core::float4 ) );

        const auto cachedCount{ spectrumReplaced ? 0 : mUploadedSpectra.size() };
        mUploadedSpectra.resize( mFftPatches.size() );

        eastl::vector<core::float2> spectrum{};

        for ( usize index{}; index < mFftPatches.size(); ++index ) {
            const auto& settings{ mFftPatches[index].mSettings };
            auto& uploaded{ mUploadedSpectra[index] };

            // Choppiness and time only affect spectrum evolution. Reuse h0 across frames.
            if ( index < cachedCount && settings.mPatchLength == uploaded.mPatchLength &&
                 settings.mWindSpeed == uploaded.mWindSpeed && settings.mWindDirection == uploaded.mWindDirection &&
                 settings.mAmplitude == uploaded.mAmplitude && settings.mSeed == uploaded.mSeed ) {
                continue;
            }

            GenerateWaterSpectrum( settings, spectrum );
            const auto bytes{ spectrum.size() * sizeof( core::float2 ) };
            mUploads.CopyBuffer( info.mFftInitialSpectrum, index * bytes, spectrum.data(), bytes );
            uploaded = settings;
        }
    }

    auto WaterSimulationModule::FilterWaterSimulators( CommandContext& ctx, core::Blackboard& b ) -> void {
        MKT_BEGIN_PROFILER();

        mFftPatches.clear();

        if ( !mScene ) {
            mPlanes.clear();
            return;
        }

        bool dirty{};
        eastl::vector<Plane> planes{};

        for ( const auto [entity, water, tag, transform]:
            mScene->GetRegistry().view<scene::WaterRenderComponent, scene::TagComponent, scene::TransformComponent>().each() ) {
            if ( !tag.IsActive() || water.GetShape() != scene::WaterShape::ePlane ) {
                continue;
            }

            const auto index{ planes.size() };
            // Revisions also cover appearance edits. Rebuild the rest mesh only
            // when its dimensions/resolution change, independently per renderer.
            dirty |= index >= mPlanes.size() ||
                ( mPlanes[index].mEntity != entity ) ||
                ( !water.IsRevision( mPlanes[index].mRevision ) &&
                ( mPlanes[index].mDimensions != water.GetPlaneDimensions() ||
                ( mPlanes[index].mResolution != water.GetPlaneResolution() ) ) );

            planes.push_back( Plane{ 
                .mEntity = entity, 
                .mRevision = water.GetRevision(), 
                .mDimensions = water.GetPlaneDimensions(), 
                .mResolution = water.GetPlaneResolution() 
            } );
        }

        if ( dirty || planes.size() != mPlanes.size() ) {
            eastl::vector<core::u32> indices{};
            eastl::vector<core::float3> vertices{};

            for ( auto& plane: planes ) {
                const auto& water{ mScene->GetRegistry().get<scene::WaterRenderComponent>( plane.mEntity ) };

                const auto resolution{ water.GetPlaneResolution() };
                const auto dimensions{ water.GetPlaneDimensions() };

                const auto xCells{ static_cast<core::u32>( resolution.x ) };
                const auto zCells{ static_cast<core::u32>( resolution.y ) };

                const auto columns{ xCells + 1 };

                plane.mFirstVertex = static_cast<core::u32>( vertices.size() );
                plane.mFirstIndex = static_cast<core::u32>( indices.size() );
                plane.mIndexCount = xCells * zCells * 6;

                vertices.reserve( vertices.size() + columns * ( zCells + 1 ) );
                indices.reserve( indices.size() + plane.mIndexCount );

                for ( core::u32 row{}; row <= zCells; ++row ) {
                    for ( core::u32 col{}; col <= xCells; ++col ) {
                        vertices.emplace_back( col * dimensions.x / xCells, 0.f, row * dimensions.y / zCells );
                    }
                }

                for ( core::u32 row{}; row < zCells; ++row ) {
                    for ( core::u32 col{}; col < xCells; ++col ) {
                        const auto a{ row * columns + col }, c{ a + columns };

                        for ( const auto index: { a, c, a + 1, a + 1, c, c + 1 } ) {
                            indices.push_back( index );
                        }
                    }
                }
            }

            if ( !planes.empty() ) {
                const auto& info{ b.Get<WaterSimulationModuleInfo>() };

                ctx.EnsureBufferCapacity( info.mVertices, vertices.size() * sizeof( core::float3 ) );
                ctx.EnsureBufferCapacity( info.mIndices, indices.size() * sizeof( core::u32 ) );

                mUploads.CopyBuffer( info.mVertices, 0, vertices.data(), vertices.size() * sizeof( core::float3 ) );
                mUploads.CopyBuffer( info.mIndices, 0, indices.data(), indices.size() * sizeof( core::u32 ) );
            }

            mPlanes = eastl::move( planes );
        }
        else {
            for ( core::usize index{}; index < mPlanes.size(); ++index ) {
                mPlanes[index].mRevision = planes[index].mRevision;
            }
        }

        // Only these small records change each frame. Rest vertices and indices
        // remain device-local until plane dimensions/resolution actually change.
        eastl::vector<WaterSurfaceGPU> surfaces{};
        eastl::vector<WaterWaveGPU> waves{};

        for ( const auto& plane: mPlanes ) {
            const auto& water{ mScene->GetRegistry().get<scene::WaterRenderComponent>( plane.mEntity ) };
            const auto material{ water.GetMaterial() };
            const auto& dielectric{ material->GetDielectric() };
            WaterSurfaceGPU surface{};
            surface.mWorld = mScene->GetRegistry().get<scene::TransformComponent>( plane.mEntity ).GetWorldTransform();
            surface.mColorRoughness = { core::float3{ material->GetBaseColorFactor() }, glm::clamp( material->GetRoughnessFactor(), 0.04f, 1.0f ) };
            surface.mAttenuation = { dielectric.mAttenuationColor, dielectric.mAttenuationDistance };
            surface.mOptics = { dielectric.mIndexOfRefraction, dielectric.mTransmission, dielectric.mThickness, water.GetFoamAmount() };
            surface.mDebugPreviewColor = { water.GetDebugPreviewColor(), 1.0f };
            surface.mGeometry = { plane.mFirstVertex, plane.mFirstIndex, static_cast<core::u32>( water.GetPlaneResolution().x ) + 1,
                ( water.IsGridVisible() ? 1u : 0u ) | ( water.AreVerticesVisible() ? 2u : 0u ) };

            const auto* simulation{ mScene->GetRegistry().try_get<scene::WaterSimulationComponent>( plane.mEntity ) };

            if ( simulation && simulation->GetType() == scene::WaterSimulationType::eFastFourierTransform ) {
                const auto& settings{ simulation->GetFftSettings() };
                const auto sampleOffset{ as<u32>( mFftPatches.size() ) * kFftSampleCount };
                surface.mSimulation.z = as<u32>( scene::WaterSimulationType::eFastFourierTransform );
                surface.mSimulation.w = sampleOffset;
                surface.mFft.x = 1.0f / settings.mPatchLength;

                mFftPatches.push_back( FftPatch{
                    .mSettings = settings,
                    .mTime = simulation->GetTime()
                } );
            }
            else if ( simulation ) {
                const auto count{ static_cast<core::u32>( simulation->GetWaveCount() ) };
                surface.mSimulation = { static_cast<core::u32>( waves.size() ), count, static_cast<core::u32>( simulation->GetType() ), 0 };

                for ( const auto& wave: simulation->GetWaves() ) {
                    const float frequency{ glm::two_pi<float>() / wave.mWavelength };
                    // Q_i * k_i * A_i <= steepness / N, so the sum stays below
                    // one and Gerstner crests cannot fold over. Zero A stays flat.
                    const float horizontal{ glm::min( wave.mAmplitude, simulation->GetSteepness() / ( frequency * count ) ) };
                    const float phase{ static_cast<float>( std::remainder( wave.mPhaseOffset -
                                                                               static_cast<double>( frequency ) * wave.mSpeed * simulation->GetTime(),
                        glm::two_pi<double>() ) ) };
                    waves.push_back( { { wave.mDirection, wave.mAmplitude, frequency }, { phase, horizontal, 0.0f, 0.0f } } );
                }
            }

            surfaces.push_back( surface );
        }

        if ( !surfaces.empty() ) {
            const auto& info{ b.Get<WaterSimulationModuleInfo>() };
            PrepareFftInputs( ctx, info );

            ctx.EnsureBufferCapacity( info.mSurfaces, surfaces.size() * sizeof( WaterSurfaceGPU ) );
            mUploads.CopyBuffer( info.mSurfaces, 0, surfaces.data(), surfaces.size() * sizeof( WaterSurfaceGPU ) );

            if ( !waves.empty() ) {
                ctx.EnsureBufferCapacity( info.mWaves, waves.size() * sizeof( WaterWaveGPU ) );
                mUploads.CopyBuffer( info.mWaves, 0, waves.data(), waves.size() * sizeof( WaterWaveGPU ) );
            }
        }
    }
}// namespace mikoto::renderer
