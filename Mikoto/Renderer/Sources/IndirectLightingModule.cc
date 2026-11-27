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

#include <algorithm>
#include <array>
#include <cmath>

#include <Core/Profiler.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/IndirectLightingModule.hh>

namespace mikoto::renderer {

    // Stachowiak / Uludag: normalized BRDF/PDF ray reuse and reflection-depth reprojection.
    // https://advances.realtimerendering.com/s2015/Stochastic%20Screen-Space%20Reflections.pptx
    // Useful open implementations of the traversal/reconstruction stages:
    // https://github.com/GPUOpen-Effects/FidelityFX-SSSR
    // https://github.com/turanszkij/WickedEngine/tree/master/WickedEngine/shaders

    namespace {

        constexpr core::u32 kMaxReflectionMips{ 16 };

        struct ReflectionFrameData {
            core::float4x4 mInverseView{ 1.0f };
            core::float4x4 mPreviousViewProjection{ 1.0f };
            core::float4x4 mPreviousView{ 1.0f };
            core::float4 mScreen{};
            core::float4 mTrace{};
            core::float4 mFilter{};
            glm::uvec4 mControl{};
        };

    }// namespace

    IndirectLightingModule::IndirectLightingModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto IndirectLightingModule::SetSettings( const ScreenSpaceReflectionSettings& settings ) -> void {
        if ( settings != mSettings ) {
            ResetHistory();
            mSettings = settings;
        }
    }

    auto IndirectLightingModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto IndirectLightingModule::ResetHistory() -> void {
        mReflectionHistoryValid = false;
        mFrameIndex = 0;
    }

    auto IndirectLightingModule::Prepare( const scene::Scene* scene, const scene::Camera* camera,
        bool rayTracedReflections ) -> void {
        SetCamera( camera );
        mCanRenderReflections = scene && camera && mSettings.mEnabled && !rayTracedReflections;

        if ( !mCanRenderReflections ) {
            ResetHistory();
            return;
        }

        float projectionDifference{};
        const auto& projection{ camera->GetProjection() };
        for ( core::u32 column{}; column < 4; ++column ) {
            for ( core::u32 row{}; row < 4; ++row ) {
                projectionDifference = std::max( projectionDifference, std::abs( projection[column][row] - mPreviousProjection[column][row] ) );
            }
        }

        const auto inverseView{ glm::inverse( camera->GetViewMatrix() ) };
        const auto inversePreviousView{ glm::inverse( mPreviousView ) };
        const float facing{ glm::dot( core::float3{ inverseView[2] }, core::float3{ inversePreviousView[2] } ) };
        if ( scene != mHistoryScene || camera != mHistoryCamera || projectionDifference > 0.001f ||
             glm::length( camera->GetPosition() - mPreviousCameraPosition ) > 5.0f || facing < 0.5f ) {
            ResetHistory();
        }

        mHistoryScene = scene;
        mHistoryCamera = camera;
    }

    auto IndirectLightingModule::RegisterSurfaceTargets( FrameGraph& graph, rhi::Multisampling samples ) -> void {
        const auto [width, height]{ rhi::InferDimensions( mResolution ) };
        auto& info{ graph.GetOrCreate<IndirectLightingModuleInfo>() };
        const auto target{ [&]( core::cstr name, rhi::Multisampling count ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name ).SetWidth( width ).SetHeight( height )
                .SetFormat( rhi::Format::eRGBA16_FLOAT ).SetMultisampling( count )
                .SetUsage( rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource |
                           rhi::TextureUsageFlagsBits::CopySource | rhi::TextureUsageFlagsBits::CopyDest ) );
        } };

        info.mSurfaceResponse = target( "SSR_SurfaceResponse", rhi::Multisampling::eMsaaX1 );
        info.mEnvironmentSpecular = target( "SSR_EnvironmentSpecular", rhi::Multisampling::eMsaaX1 );
        info.mRasterSurfaceResponse = info.mSurfaceResponse;
        info.mRasterEnvironmentSpecular = info.mEnvironmentSpecular;

        if ( samples != rhi::Multisampling::eMsaaX1 ) {
            info.mRasterSurfaceResponse = target( "SSR_SurfaceResponse_MSAA", samples );
            info.mRasterEnvironmentSpecular = target( "SSR_EnvironmentSpecular_MSAA", samples );
        }
    }

    auto IndirectLightingModule::RegisterPasses( FrameGraph& graph, rhi::Multisampling samples ) -> void {
        MKT_BEGIN_PROFILER();

        const auto [renderWidth, renderHeight]{ rhi::InferDimensions( mResolution ) };
        const core::u32 width{ core::as<core::u32>( renderWidth ) };
        const core::u32 height{ core::as<core::u32>( renderHeight ) };
        const core::u32 traceWidth{ mSettings.mHalfResolution ? ( width + 1 ) / 2 : width };
        const core::u32 traceHeight{ mSettings.mHalfResolution ? ( height + 1 ) / 2 : height };
        const auto surface{ graph.GetData<IndirectLightingModuleInfo>() };
        const auto position{ graph.GetData<PrepassModuleInfo>().mGBufferPositionTarget };
        const auto normal{ graph.GetData<PrepassModuleInfo>().mGBufferNormalTarget };
        const auto sceneColor{ graph.GetData<GeomShadingModuleInfo>().mColorImage };
        const auto camera{ graph.GetData<CameraModuleInfo>().mCameraData };

        const auto texture{ [&]( core::cstr name, core::u32 x, core::u32 y, rhi::Format format = rhi::Format::eRGBA16_FLOAT ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name ).SetWidth( x ).SetHeight( y ).SetFormat( format )
                .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::UnorderedAccess |
                           rhi::TextureUsageFlagsBits::CopySource | rhi::TextureUsageFlagsBits::CopyDest ) );
        } };
        const auto pipeline{ [&]( core::cstr name, core::cstr shader ) {
            return graph.Create( FGPipelineDescription{}.SetName( name )
                .SetPipelineType( rhi::PipelineType::eCompute ).PushShader( shader, FGStageType::eCompute ) );
        } };
        const auto dispatch{ []( CommandContext& ctx, FGPipelineHandle pipeline, const auto& parameters,
            core::u32 x, core::u32 y ) {
            ctx.BindPipeline( pipeline );
            ctx.PushConstants( parameters );
            ctx.Dispatch( ( x + 7 ) / 8, ( y + 7 ) / 8, 1 );
        } };

        const auto frameBuffer{ graph.Create( FGBufferDescription{}
            .SetName( "SSR_FrameData" ).SetSizeBytes( MKT_SIZEOF( ReflectionFrameData ) )
            .SetUsage( rhi::BufferUsageFlagsBits::Structured | rhi::BufferUsageFlagsBits::CopyDest )
            .SetHeapType( rhi::HeapType::eDeviceLocal ) ) };
        const auto sampler{ graph.Create( FGSamplerDescription{}.SetName( "SSR_LinearClamp" )
            .SetFilter( rhi::SamplerFilter::eLinear ).SetWrap( rhi::SamplerWrapMode::eClampToEdge ) ) };

        graph.RegisterPass( "SSR_Upload", FGPassType::eTransfer,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( frameBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [=, this]( CommandContext& ctx, core::Blackboard& ) {
                if ( !mCanRenderReflections ) {
                    return;
                }
                const auto& projection{ mCamera->GetProjection() };
                const auto nearClip{ projection * core::float4{ 0, 0, -mCamera->GetNearPlane(), 1 } };
                const auto farClip{ projection * core::float4{ 0, 0, -mCamera->GetFarPlane(), 1 } };
                ReflectionFrameData frame{};
                frame.mInverseView = glm::inverse( mCamera->GetViewMatrix() );
                frame.mPreviousViewProjection = mPreviousViewProjection;
                frame.mPreviousView = mPreviousView;
                frame.mScreen = { core::as<core::f32>( width ), core::as<core::f32>( height ),
                    core::as<core::f32>( traceWidth ), core::as<core::f32>( traceHeight ) };
                frame.mTrace = { mSettings.mMaxDistance, mSettings.mThickness, mSettings.mMaxRoughness, mSettings.mSamplingBias };
                frame.mFilter = { mSettings.mHistoryWeight, core::as<core::f32>( mSettings.mSpatialSamples ), mSettings.mIntensity, 0 };
                frame.mControl = { mFrameIndex, mReflectionHistoryValid ? 1u : 0u,
                    nearClip.z / nearClip.w > farClip.z / farClip.w ? 1u : 0u, mSettings.mMaxSteps };
                ctx.CopyBuffer( frameBuffer, frame, 0 );
            } );

        if ( samples != rhi::Multisampling::eMsaaX1 ) {
            graph.RegisterPass( "SSR_ResolveSurface", FGPassType::eGraphics,
                [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                    for ( auto input: { surface.mRasterSurfaceResponse, surface.mRasterEnvironmentSpecular } ) {
                        builder.UseResource( input, FGPipelineStage::eResolve, FGResourceAccess::eRead );
                    }
                    for ( auto output: { surface.mSurfaceResponse, surface.mEnvironmentSpecular } ) {
                        builder.UseResource( output, FGPipelineStage::eResolve, FGResourceAccess::eWrite );
                    }
                },
                [=]( CommandContext& ctx, core::Blackboard& ) {
                    ctx.ResolveTexture( surface.mRasterSurfaceResponse, surface.mSurfaceResponse );
                    ctx.ResolveTexture( surface.mRasterEnvironmentSpecular, surface.mEnvironmentSpecular );
                } );
        }

        std::array<FGTextureHandle, kMaxReflectionMips> depths{};
        std::array<FGTextureHandle, kMaxReflectionMips> colors{};
        core::u32 mipCount{};
        core::u32 mipWidth{ width };
        core::u32 mipHeight{ height };
        const auto depthPipeline{ pipeline( "SSR_DepthHierarchy", "SSR_Depth_Comp.slang" ) };
        const auto colorPipeline{ pipeline( "SSR_ColorPyramid", "SSR_Color_Comp.slang" ) };
        const auto reduce{ [&, this]( core::cstr name, FGTextureHandle input, FGTextureHandle output,
            core::u32 x, core::u32 y, core::u32 mode, FGPipelineHandle shader ) {
            graph.RegisterPass( name, FGPassType::eCompute,
                [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                    builder.UseResource( frameBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( camera, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                },
                [=, this]( CommandContext& ctx, core::Blackboard& ) {
                    if ( !mCanRenderReflections ) {
                        return;
                    }
                    struct Parameters {
                        SPointer mFrame;
                        SPointer mCamera;
                        core::u32 mInput;
                        core::u32 mOutput;
                        core::u32 mWidth;
                        core::u32 mHeight;
                        core::u32 mMode;
                    } parameters{
                        .mFrame = ctx.GetDeviceBufferAddress( frameBuffer ),
                        .mCamera = ctx.GetDeviceBufferAddress( camera ),
                        .mInput = ctx.PushTexture_SRV( input ),
                        .mOutput = ctx.PushTexture_UAV( output ),
                        .mWidth = x,
                        .mHeight = y,
                        .mMode = mode,
                    };
                    dispatch( ctx, shader, parameters, x, y );
                } );
        } };

        while ( mipCount < kMaxReflectionMips ) {
            const auto depthName{ string::Format( "SSR_Depth_{}", mipCount ) };
            depths[mipCount] = texture( depthName.c_str(), mipWidth, mipHeight, rhi::Format::eR32_FLOAT );
            reduce( depthName.c_str(), mipCount == 0 ? position : depths[mipCount - 1],
                depths[mipCount], mipWidth, mipHeight, mipCount == 0 ? 0 : 1, depthPipeline );

            if ( mipCount == 0 ) {
                colors[0] = sceneColor;
            }
            else {
                const auto colorName{ string::Format( "SSR_Color_{}", mipCount ) };
                colors[mipCount] = texture( colorName.c_str(), mipWidth, mipHeight );
                reduce( colorName.c_str(), colors[mipCount - 1], colors[mipCount], mipWidth, mipHeight, 2, colorPipeline );
            }
            ++mipCount;
            if ( mipWidth == 1 && mipHeight == 1 ) {
                break;
            }
            mipWidth = std::max( 1u, ( mipWidth + 1 ) / 2 );
            mipHeight = std::max( 1u, ( mipHeight + 1 ) / 2 );
        }

        const auto hitPoints{ texture( "SSR_HitPoints", traceWidth, traceHeight, rhi::Format::eRGBA32_FLOAT ) };
        const auto directions{ texture( "SSR_DirectionPdf", traceWidth, traceHeight, rhi::Format::eRGBA32_FLOAT ) };
        const auto resolved{ texture( "SSR_SpatialResolve", width, height ) };
        const auto reflectionDistance{ texture( "SSR_ReflectionDistance", width, height ) };
        const auto filtered{ texture( "SSR_TemporalResolve", width, height ) };
        const auto metadata{ texture( "SSR_CurrentMetadata", width, height, rhi::Format::eRGBA32_FLOAT ) };
        const auto history{ texture( "SSR_ReflectionHistory", width, height ) };
        const auto historyMetadata{ texture( "SSR_MetadataHistory", width, height, rhi::Format::eRGBA32_FLOAT ) };
        const auto composite{ texture( "SSR_Composite", width, height ) };
        const auto tracePipeline{ pipeline( "SSR_Trace", "SSR_Trace_Comp.slang" ) };
        const auto resolvePipeline{ pipeline( "SSR_Resolve", "SSR_Resolve_Comp.slang" ) };
        const auto temporalPipeline{ pipeline( "SSR_Temporal", "SSR_Temporal_Comp.slang" ) };
        const auto compositePipeline{ pipeline( "SSR_Composite", "SSR_Composite_Comp.slang" ) };

        graph.RegisterPass( "SSR_Trace", FGPassType::eCompute,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( frameBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( camera, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( auto input: { position, normal, surface.mSurfaceResponse } ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                for ( core::u32 mip{}; mip < mipCount; ++mip ) {
                    builder.UseResource( depths[mip], FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                for ( auto output: { hitPoints, directions } ) {
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }
            },
            [=, this]( CommandContext& ctx, core::Blackboard& ) {
                if ( !mCanRenderReflections ) {
                    return;
                }
                struct Parameters {
                    SPointer mFrame;
                    SPointer mCamera;
                    std::array<core::u32, kMaxReflectionMips> mDepthMips;
                    core::u32 mMipCount;
                    core::u32 mPosition;
                    core::u32 mNormal;
                    core::u32 mSurface;
                    core::u32 mHits;
                    core::u32 mDirections;
                } parameters{
                    .mFrame = ctx.GetDeviceBufferAddress( frameBuffer ),
                    .mCamera = ctx.GetDeviceBufferAddress( camera ),
                    .mDepthMips = {},
                    .mMipCount = mipCount,
                    .mPosition = ctx.PushTexture_SRV( position ),
                    .mNormal = ctx.PushTexture_SRV( normal ),
                    .mSurface = ctx.PushTexture_SRV( surface.mSurfaceResponse ),
                    .mHits = ctx.PushTexture_UAV( hitPoints ),
                    .mDirections = ctx.PushTexture_UAV( directions ),
                };
                for ( core::u32 mip{}; mip < mipCount; ++mip ) {
                    parameters.mDepthMips[mip] = ctx.PushTexture_SRV( depths[mip] );
                }
                dispatch( ctx, tracePipeline, parameters, traceWidth, traceHeight );
            } );

        graph.RegisterPass( "SSR_SpatialResolve", FGPassType::eCompute,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( frameBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( camera, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( auto input: { position, normal, surface.mSurfaceResponse, hitPoints, directions } ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                for ( core::u32 mip{}; mip < mipCount; ++mip ) {
                    builder.UseResource( colors[mip], FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                for ( auto output: { resolved, reflectionDistance } ) {
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }
            },
            [=, this]( CommandContext& ctx, core::Blackboard& ) {
                if ( !mCanRenderReflections ) {
                    return;
                }
                struct Parameters {
                    SPointer mFrame;
                    SPointer mCamera;
                    std::array<core::u32, kMaxReflectionMips> mColorMips;
                    core::u32 mMipCount;
                    core::u32 mPosition;
                    core::u32 mNormal;
                    core::u32 mSurface;
                    core::u32 mHits;
                    core::u32 mDirections;
                    core::u32 mOutput;
                    core::u32 mDistance;
                    core::u32 mSampler;
                } parameters{
                    .mFrame = ctx.GetDeviceBufferAddress( frameBuffer ),
                    .mCamera = ctx.GetDeviceBufferAddress( camera ),
                    .mColorMips = {},
                    .mMipCount = mipCount,
                    .mPosition = ctx.PushTexture_SRV( position ),
                    .mNormal = ctx.PushTexture_SRV( normal ),
                    .mSurface = ctx.PushTexture_SRV( surface.mSurfaceResponse ),
                    .mHits = ctx.PushTexture_SRV( hitPoints ),
                    .mDirections = ctx.PushTexture_SRV( directions ),
                    .mOutput = ctx.PushTexture_UAV( resolved ),
                    .mDistance = ctx.PushTexture_UAV( reflectionDistance ),
                    .mSampler = ctx.PushSampler( sampler ),
                };
                for ( core::u32 mip{}; mip < mipCount; ++mip ) {
                    parameters.mColorMips[mip] = ctx.PushTexture_SRV( colors[mip] );
                }
                dispatch( ctx, resolvePipeline, parameters, width, height );
            } );

        graph.RegisterPass( "SSR_Temporal", FGPassType::eCompute,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( frameBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( camera, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( auto input: { resolved, reflectionDistance, position, normal, surface.mSurfaceResponse, history, historyMetadata } ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                for ( auto output: { filtered, metadata } ) {
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }
            },
            [=, this]( CommandContext& ctx, core::Blackboard& ) {
                if ( !mCanRenderReflections ) {
                    return;
                }
                struct Parameters {
                    SPointer mFrame;
                    SPointer mCamera;
                    core::u32 mCurrent;
                    core::u32 mDistance;
                    core::u32 mPosition;
                    core::u32 mNormal;
                    core::u32 mSurface;
                    core::u32 mHistory;
                    core::u32 mHistoryMetadata;
                    core::u32 mOutput;
                    core::u32 mMetadataOutput;
                    core::u32 mSampler;
                } parameters{
                    .mFrame = ctx.GetDeviceBufferAddress( frameBuffer ),
                    .mCamera = ctx.GetDeviceBufferAddress( camera ),
                    .mCurrent = ctx.PushTexture_SRV( resolved ),
                    .mDistance = ctx.PushTexture_SRV( reflectionDistance ),
                    .mPosition = ctx.PushTexture_SRV( position ),
                    .mNormal = ctx.PushTexture_SRV( normal ),
                    .mSurface = ctx.PushTexture_SRV( surface.mSurfaceResponse ),
                    .mHistory = ctx.PushTexture_SRV( history ),
                    .mHistoryMetadata = ctx.PushTexture_SRV( historyMetadata ),
                    .mOutput = ctx.PushTexture_UAV( filtered ),
                    .mMetadataOutput = ctx.PushTexture_UAV( metadata ),
                    .mSampler = ctx.PushSampler( sampler ),
                };
                dispatch( ctx, temporalPipeline, parameters, width, height );
            } );

        graph.RegisterPass( "SSR_Composite", FGPassType::eCompute,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( frameBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( auto input: { sceneColor, filtered, surface.mEnvironmentSpecular } ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                builder.UseResource( composite, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [=, this]( CommandContext& ctx, core::Blackboard& ) {
                if ( !mCanRenderReflections ) {
                    return;
                }
                struct Parameters {
                    SPointer mFrame;
                    core::u32 mScene;
                    core::u32 mReflection;
                    core::u32 mEnvironment;
                    core::u32 mOutput;
                } parameters{
                    .mFrame = ctx.GetDeviceBufferAddress( frameBuffer ),
                    .mScene = ctx.PushTexture_SRV( sceneColor ),
                    .mReflection = ctx.PushTexture_SRV( filtered ),
                    .mEnvironment = ctx.PushTexture_SRV( surface.mEnvironmentSpecular ),
                    .mOutput = ctx.PushTexture_UAV( composite ),
                };
                dispatch( ctx, compositePipeline, parameters, width, height );
            } );

        graph.RegisterPass( "SSR_Store", FGPassType::eGraphics,
            [=]( FGNodeBuilder& builder, core::Blackboard& ) {
                for ( auto input: { composite, filtered, metadata } ) {
                    builder.UseResource( input, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                }
                for ( auto output: { sceneColor, history, historyMetadata } ) {
                    builder.UseResource( output, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [=, this]( CommandContext& ctx, core::Blackboard& ) {
                if ( !mCanRenderReflections ) {
                    return;
                }
                // Copy back to the existing HDR handle so later glass, water and
                // post-process passes keep their established render-target bindings.
                const rhi::TextureSlice slice{ .mWidth = width, .mHeight = height, .mDepth = 1 };
                ctx.CopyTexture( composite, slice, sceneColor, slice );
                ctx.CopyTexture( filtered, slice, history, slice );
                ctx.CopyTexture( metadata, slice, historyMetadata, slice );
                mPreviousView = mCamera->GetViewMatrix();
                mPreviousProjection = mCamera->GetProjection();
                mPreviousViewProjection = mPreviousProjection * mPreviousView;
                mPreviousCameraPosition = mCamera->GetPosition();
                mReflectionHistoryValid = true;
                ++mFrameIndex;
            } );
    }

}// namespace mikoto::renderer
