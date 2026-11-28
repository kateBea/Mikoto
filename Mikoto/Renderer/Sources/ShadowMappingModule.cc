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

#include <EASTL/algorithm.h>
#include <EASTL/numeric_limits.h>

#include <glm/gtx/compatibility.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>

namespace mikoto::renderer {
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    ShadowMappingModule::ShadowMappingModule( ShadowMapQuality quality ) {
        SetShadowMapsQuality( quality );
    }

    auto ShadowMappingModule::SetScene( const scene::Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScene = scene;
    }

    auto ShadowMappingModule::SetCamera( const scene::Camera* camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto ShadowMappingModule::SetShadowMapsQuality( ShadowMapQuality quality ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRegistered ) {
            MKT_CORE_LOGGER_WARN( "Shadow quality must be set before registering the frame graph." );
            return;
        }

        core::u32 dimensions{ 1024 };

        switch ( quality ) {
            case ShadowMapQuality::eLow:
                dimensions = 512;
                break;
            case ShadowMapQuality::eMedium:
                dimensions = 1024;
                break;
            case ShadowMapQuality::eHigh:
                dimensions = 4096;
                break;
            default:
                break;
        }

        mDirectionalSize = mSpotSize = dimensions;
        mPointSize = glm::max( 256u, mDirectionalSize / 2 );
    }

    auto ShadowMappingModule::SetCascadedShadowsEnabled( bool enabled ) -> void {
        if ( mRegistered ) {
            MKT_CORE_LOGGER_WARN( "Cascade mode must be set before registering the frame graph." );
            return;
        }

        mDirectionalCascadeCount = enabled ? kDirectionalCascades : 1u;
    }

    auto ShadowMappingModule::GetDiagnostics() const -> const ShadowDiagnostics& {
        MKT_BEGIN_PROFILER();
        return mDiagnostics;
    }

    auto ShadowMappingModule::SetGeometryManager( GeometryCullModule& geometry ) -> void {
        MKT_BEGIN_PROFILER();
        mGeometryManagement = MKT_ADDRESSOF( geometry );
    }

    auto ShadowMappingModule::SetShadowMappDistance( core::f32 distance ) -> void {
        mCascadesDistance = math::Clamp( distance, 200.0f, 1000.0f );
    }

    auto ShadowMappingModule::RegisterUploadPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();
        auto& info = graph.GetOrCreate<ShadowMapInfo>();
        mRegistered = true;

        info.mRecordsBuffer = graph.Create( FGBufferDescription{}
            .SetName( "ShadowRecords" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( kMaxShadowRecords, sizeof( ShadowMapParameters ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        info.mIndirectBuffer = graph.Create( FGBufferDescription{}
            .SetName( "ShadowIndirectCommands" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::IndirectDraw | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( kMaxShadowCommands, sizeof( DrawIndirectCommand ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        const auto uploadSetup = []( FGNodeBuilder& builder, core::Blackboard& b ) {
            // These edges order the CPU geometry/light snapshots before command generation.
            builder.UseResource( b.Get<GeometryCullModuleInfo>().mGeometryBuffer, FGPipelineStage::eCopy, FGResourceAccess::eRead );
            builder.UseResource( b.Get<PrepassModuleInfo>().mLightsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eRead );
            builder.UseResource( b.Get<ShadowMapInfo>().mIndirectBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            builder.UseResource( b.Get<ShadowMapInfo>().mRecordsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
        };

        graph.RegisterPass(
            "ShadowMaps_Upload",
            FGPassType::eTransfer,
            uploadSetup,
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                UploadShadowMaps( ctx, b );
            } );
    }

    auto ShadowMappingModule::RegisterShadowMaps( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto& info = graph.GetOrCreate<ShadowMapInfo>();
        
        info.mDirectionalSize = mDirectionalSize;
        info.mDirectionalCascadeCount = mDirectionalCascadeCount;
        
        const auto create = [&]( core::cstr kind, core::u32 light, core::u32 face, core::u32 size ) {
            const auto texture = graph.Create( FGTextureDescription{}
                .SetName( string::Format( "{}Shadow_{}_{}", kind, light, face ) )
                .SetWidth( size )
                .SetHeight( size )
                .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
                .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                .SetFormat( renderer::rhi::Format::eD32 ) );
            info.mMaps.push_back( texture );
            return texture;
        };

        // Construct maps for each type
        for ( core::u32 light = 0; light < kMaxShadowLightsPerType; ++light ) {
            for ( core::u32 face = 0; face < mDirectionalCascadeCount; ++face ) {
                info.mDirectional[light][face] = create( "Directional", light, face, mDirectionalSize );
            }

            for ( core::u32 face = 0; face < 6; ++face ) {
                info.mPoint[light][face] = create( "Point", light, face, mPointSize );
            }

            info.mSpot[light] = create( "Spot", light, 0, mSpotSize );
        }

        info.mPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "ShadowMapping_Pipeline01" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetBlendEnable( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .PushShader( "DirLightShadows_Vert.slang", FGStageType::eVertex )
            .PushShader( "DirLightShadows_Frag.slang", FGStageType::ePixel ) );

        const auto renderSetup = []( FGNodeBuilder& builder, core::Blackboard& b ) {
            const auto& shadows{ b.Get<ShadowMapInfo>() };
            const auto& geometry{ b.Get<GeometryCullModuleInfo>() };

            for ( const auto texture: shadows.mMaps ) {
                builder.UseResource( texture, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
            }

            builder.UseResource( shadows.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            builder.UseResource( shadows.mRecordsBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mDrawInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mSkinningCacheBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mAttributeBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometry.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

            // Kept for the bounded command arena's unculled fallback.
            builder.UseResource( geometry.mUnculledIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
        };

        graph.RegisterPass(
            "ShadowMaps_Render",
            FGPassType::eGraphics,
            renderSetup,
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                RenderShadowMaps( ctx, b );
            } );
    }

    auto ShadowMappingModule::RegisterDirectionalVisibility( FrameGraph& graph, rhi::RenderResolution resolution ) -> void {
        // Tardif's screen-space soft-shadow chain, adapted to compute passes and one
        // visibility image per directional light so shadowing never darkens IBL or other lights.
        // https://alextardif.com/shadowmapping.html
        const auto dimensions{ InferDimensions( resolution ) };
        const core::u32 imageWidth{ as<core::u32>( dimensions.first ) };
        const core::u32 imageHeight{ as<core::u32>( dimensions.second ) };
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "ShadowMaps_VisibilityPipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "ShadowVisibility_Comp.slang", FGStageType::eCompute ) ) };

        enum class VisibilityStage : core::u32 {
            eEvaluate,
            eDownsample,
            eBlurHorizontal,
            eBlurVertical,
            eUpsample
        };

        struct VisibilityParameters {
            core::u64 mCamera{};
            core::u32 mRecords{};
            core::u32 mRecord{};
            core::u32 mDepth{};
            core::u32 mInput{ ~core::u32{} };
            core::u32 mOutput{};
            core::u32 mStage{};
            core::float3 mToLight{};
            core::u32 mRawVisibility{ ~core::u32{} };
        };

        const eastl::array<core::cstr, 5> stageNames{
            "Visibility", "Downsample", "BlurHorizontal", "BlurVertical", "Upsample"
        };
        auto& shadows{ graph.GetOrCreate<ShadowMapInfo>() };

        // Keep a sampled receiver-depth snapshot separate from the later read-only
        // depth attachment. Material passes can then sample it without changing that attachment's layout.
        const auto receiverPipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "ShadowMaps_ReceiverDataPipeline" )
            .SetPipelineType( rhi::PipelineType::eCompute )
            .PushShader( "ShadowReceiverDepth_Comp.slang", FGStageType::eCompute ) ) };
        shadows.mVisibilityDepth = graph.Create( FGTextureDescription{}
            .SetName( "ShadowMaps_ReceiverDepth" )
            .SetWidth( imageWidth )
            .SetHeight( imageHeight )
            .SetFormat( rhi::Format::eRGBA32_FLOAT )
            .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::UnorderedAccess ) );

        graph.RegisterPass(
            "ShadowMaps_ReceiverDepth",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<ShadowMapInfo>().mVisibilityDepth, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( b.Get<ShadowMapInfo>().mRecordsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            },
            [receiverPipeline, imageWidth, imageHeight]( CommandContext &ctx, core::Blackboard &b ) {
                const auto& data{ b.Get<ShadowMapInfo>() };

                if ( eastl::all_of( data.mDirectionalRecords.begin(), data.mDirectionalRecords.end(),
                         []( core::u32 record ) {
                             return record == ~core::u32{};
                         } ) ) {
                    return;
                }

                struct Parameters {
                    core::u32 mDepth{};
                    core::u32 mOutput{};
                } parameters{
                    .mDepth = ctx.PushSRV( b.Get<PrepassModuleInfo>().mPrepassDepthTarget ),
                    .mOutput = ctx.PushUAV( data.mVisibilityDepth ),
                };
                ctx.BindPipeline( receiverPipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( ( imageWidth + 7 ) / 8, ( imageHeight + 7 ) / 8, 1 );
            } );

        for ( core::u32 light{}; light < kMaxShadowLightsPerType; ++light ) {
            eastl::array<FGTextureHandle, 5> images{};

            for ( core::u32 index{}; index < images.size(); ++index ) {
                const bool fullResolution{ index == 0 || index == 4 };
                const core::u32 width{ fullResolution ? imageWidth : ( imageWidth + 1 ) / 2 };
                const core::u32 height{ fullResolution ? imageHeight : ( imageHeight + 1 ) / 2 };
                images[index] = graph.Create( FGTextureDescription{}
                    .SetName( string::Format( "ShadowMaps_{}_{}", stageNames[index], light ) )
                    .SetWidth( width )
                    .SetHeight( height )
                    .SetFormat( rhi::Format::eR16_FLOAT )
                    .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::UnorderedAccess ) );
            }

            shadows.mDirectionalVisibility[light] = images.back();

            for ( core::u32 index{}; index < images.size(); ++index ) {
                const auto stage{ static_cast<VisibilityStage>( index ) };
                const auto output{ images[index] };
                const auto input{ index ? images[index - 1] : FGTextureHandle{} };
                const auto raw{ images.front() };
                const bool fullResolution{ index == 0 || index == 4 };
                const core::u32 width{ fullResolution ? imageWidth : ( imageWidth + 1 ) / 2 };
                const core::u32 height{ fullResolution ? imageHeight : ( imageHeight + 1 ) / 2 };

                graph.RegisterPass(
                    string::Format( "ShadowMaps_{}_{}", stageNames[index], light ),
                    FGPassType::eCompute,
                    [stage, light, input, raw, output]( FGNodeBuilder& builder, core::Blackboard& b ) {
                        const auto& data{ b.Get<ShadowMapInfo>() };
                        builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        builder.UseResource( data.mVisibilityDepth, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );

                        if ( stage == VisibilityStage::eEvaluate ) {
                            builder.UseResource( data.mRecordsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );

                            for ( const auto map: data.mDirectional[light] ) {
                                if ( map.mHandle ) {
                                    builder.UseResource( map, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                                }
                            }
                        }
                        else {
                            builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );

                            if ( stage == VisibilityStage::eUpsample ) {
                                builder.UseResource( raw, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                            }
                        }
                    },
                    [this, stage, light, input, raw, output, pipeline, width, height]( CommandContext& ctx, core::Blackboard& b ) {
                        const auto& data{ b.Get<ShadowMapInfo>() };

                        if ( !mSoftShadowsEnabled || data.mDirectionalRecords[light] == ~core::u32{} ) {
                            return;
                        }

                        VisibilityParameters parameters{
                            .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                            .mRecords = ctx.PushSRV( data.mRecordsBuffer ),
                            .mRecord = data.mDirectionalRecords[light],
                            .mDepth = ctx.PushSRV( data.mVisibilityDepth ),
                            .mOutput = ctx.PushUAV( output ),
                            .mStage = as<core::u32>( stage ),
                            .mToLight = data.mDirectionalToLight[light]
                        };

                        if ( input.mHandle ) {
                            parameters.mInput = ctx.PushSRV( input );
                        }

                        if ( stage == VisibilityStage::eUpsample ) {
                            parameters.mRawVisibility = ctx.PushSRV( raw );
                        }

                        ctx.BindPipeline( pipeline );
                        ctx.PushConstants( parameters );
                        ctx.Dispatch( ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
                    } );
            }
        }
    }

    auto ShadowMappingModule::UploadShadowMaps( CommandContext& ctx, core::Blackboard& b ) -> void {
        MKT_BEGIN_PROFILER();

        auto& shadows = b.Get<ShadowMapInfo>();
        const auto& lights{ b.Get<PrepassModuleInfo>() };

        mViews.clear();
        mShadowCommands.clear();
        shadows.mRecordCount = 0;
        shadows.mDirectionalRecords.fill( ~core::u32{} );
        mDiagnostics = {};

        if ( !mScene || !lights.mCpuLights ) {
            return;
        }

        mRecords = {};
        auto& records{ mRecords };
        shadows.mRecordCount = glm::min( lights.mShadowRecordCount, kMaxShadowRecords );
        const auto add = [&]( core::u32 record, core::u32 face, FGTextureHandle texture, core::u32 size ) {
            records[record].mTextureIDs[face] = ctx.PushSRV( texture );
            mViews.push_back( { texture, record, face, size } );
        };

        for ( core::u32 index = 0; index < lights.mActiveLightCount; ++index ) {
            const auto& light = lights.mCpuLights[index];

            if ( light.mActiveLightType >= 0 && light.mActiveLightType < 3 ) {
                ++mDiagnostics.mActiveLights[light.mActiveLightType];
            }

            if ( light.mShadowIndex < 0 || core::as<core::u32>( light.mShadowIndex ) >= shadows.mRecordCount || light.mShadowSlot >= kMaxShadowLightsPerType ) {
                continue;
            }

            const core::u32 id{ core::as<core::u32>( light.mShadowIndex ) };
            const core::u32 slot{ light.mShadowSlot };
            auto& record = records[id];
            record.mLightType = light.mActiveLightType;
            record.mPositionRange = { core::float3( light.mPosition ), light.mRadius };
            record.mBias = { .00003f, .00015f, 0, 0 };

            if ( light.mActiveLightType == core::as<core::i32>( LightType::eDirectional ) ) {
                if ( !mCamera ) {
                    continue;
                }

                const auto cascades = BuildDirectionalCascades( glm::inverse( mCamera->GetProjection() ), glm::inverse( mCamera->GetViewMatrix() ),
                    mCamera->GetNearPlane(), mCamera->GetFarPlane(), core::float3( light.mDirection ),
                    mDirectionalSize, mCascadesDistance );
                record.mCascadeSplits = cascades.splits;
                record.mPositionRange.w = mCamera->GetNearPlane();
                record.mFaceCount = mDirectionalCascadeCount;

                // Directional bias is in meters. Convert separately for each
                // cascade in the shader so padded light depth ranges cannot
                // turn a small precision offset into detached car-sized gaps.
                record.mBias = { .002f, .008f, 0, 0 };
                shadows.mDirectionalRecords[slot] = id;
                shadows.mDirectionalToLight[slot] = -core::float3{ light.mDirection };

                // Cascades occupy slots 0..3. The remaining directional-only slots
                // publish the filtered visibility and its receiver depth without enlarging draw constants.
                const auto visibility{ shadows.mDirectionalVisibility[slot] };
                record.mTextureIDs[4] = mSoftShadowsEnabled && visibility.mHandle ? ctx.PushSRV( visibility ) : ~core::u32{};
                const auto receivers{ shadows.mGrassReceiverData[slot] };
                record.mTextureIDs[5] = receivers.mHandle    ? ctx.PushSRV( receivers )
                                        : visibility.mHandle ? ctx.PushSRV( shadows.mVisibilityDepth )
                                                             : ~core::u32{};

                for ( core::u32 face = 0; face < mDirectionalCascadeCount; ++face ) {
                    record.mViewProjection[face] = cascades.matrices[face];
                    add( id, face, shadows.mDirectional[slot][face], mDirectionalSize );
                }
            }
            else if ( light.mActiveLightType == core::as<core::i32>( LightType::ePoint ) ) {
                const auto matrices = PointShadowMatrices( float3( light.mPosition ), light.mRadius );
                record.mFaceCount = 6;

                for ( core::u32 face = 0; face < 6; ++face ) {
                    record.mViewProjection[face] = matrices[face];
                    add( id, face, shadows.mPoint[slot][face], mPointSize );
                }
            }
            else if ( light.mActiveLightType == core::as<core::i32>( LightType::eSpot ) ) {
                record.mFaceCount = 1;
                record.mViewProjection[0] = SpotShadowMatrix( core::float3( light.mPosition ), core::float3( light.mDirection ), light.mOuterCutOff, light.mRadius );
                add( id, 0, shadows.mSpot[slot], mSpotSize );
            }

            // z: source radius (local) or tan(solar half-angle); w: PCSS enable.
            record.mBias.z = light.mPadding;
            record.mBias.w = mSoftShadowsEnabled ? 1.0f : 0.0f;
            if ( record.mFaceCount ) {
                ++mDiagnostics.mMappedLights[light.mActiveLightType];
            }
        }

        // Use each light's actual clip volume, including the upstream depth
        // padding of directional cascades. Camera visibility is not a caster test.
        if ( mGeometryManagement ) {
            mDiagnostics.mCasterBatches = mGeometryManagement->GetShadowCasterBatchCount();

            for ( auto& view: mViews ) {
                view.mFirstDraw = core::as<core::u32>( mShadowCommands.size() );
                core::u32 tested{};
                core::u32 culled{};
                view.mUseUnculled = !mGeometryManagement->AppendShadowDraws(
                    records[view.mRecord].mViewProjection[view.mFace], mShadowCommands,
                    kMaxShadowCommands, tested, culled );
                view.mDrawCount = core::as<core::u32>( mShadowCommands.size() ) - view.mFirstDraw;
                mDiagnostics.mTestedInstances += tested;
                mDiagnostics.mCulledInstances += culled;

                if ( view.mUseUnculled ) {
                    ++mDiagnostics.mUnculledViews;
                }
            }

            if ( !mShadowCommands.empty() ) {
                // One upload before any map consumes this buffer; never overwrite
                // a command range between shadow draws.
                ctx.Copy( shadows.mIndirectBuffer, 0, mShadowCommands.data(),
                    mShadowCommands.size() * sizeof( DrawIndirectCommand ) );
            }
        }

        if ( shadows.mRecordCount ) {
            ctx.Copy( shadows.mRecordsBuffer, 0, records.data(), shadows.mRecordCount * sizeof( ShadowMapParameters ) );
        }
    }

    auto ShadowMappingModule::RenderShadowMaps( CommandContext& ctx, core::Blackboard& b ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mGeometryManagement || mViews.empty() ) {
            return;
        }

        const auto& shadows = b.Get<ShadowMapInfo>();
        const auto& geometry = b.Get<GeometryCullModuleInfo>();
        struct Parameters {
            SPointer mGeometry;
            SPointer mSkinning;
            SPointer mAllocation;
            SPointer mRecords;
            SPointer mMaterials;
            core::u32 mSampler;
            core::u32 mRecord;
            core::u32 mFace;
            core::u32 mPadding{};
        } params{
            .mGeometry = ctx.GetDeviceBufferAddress( geometry.mGeometryBuffer ),
            .mSkinning = ctx.GetDeviceBufferAddress( geometry.mSkinningBuffer ),
            .mAllocation = ctx.GetDeviceBufferAddress( geometry.mGeometryAllocBuffer ),
            .mRecords = ctx.GetDeviceBufferAddress( shadows.mRecordsBuffer ),
            .mMaterials = ctx.GetDeviceBufferAddress( geometry.mMaterialsBuffer ),
            .mSampler = ctx.PushSampler( geometry.mMaterialSampler ),
            .mRecord = 0,
            .mFace = 0,
        };

        for ( const auto& view: mViews ) {
            params.mRecord = view.mRecord;
            params.mFace = view.mFace;
            ctx.BeginRender( RenderBeginDescription{}
                .SetRenderArea( renderer::rhi::Rect{ 0, core::as<core::i32>( view.mSize ),
                        0, core::as<core::i32>( view.mSize ) } )
                .AddDepthTarget( view.mTarget, renderer::rhi::LoadOp::eClear ) );
            ctx.SetViewportState( renderer::rhi::ViewportState{}
                .AddViewportAndScissorRect( Viewport( 0, core::as<core::f32>( view.mSize ),
                        0, core::as<core::f32>( view.mSize ), 0, 1 ) ) );
            ctx.BindPipeline( shadows.mPipeline );
            ctx.PushConstants( params );

            if ( view.mUseUnculled ) {
                // Capacity is a performance limit, never a reason to drop shadows.
                mGeometryManagement->DrawInstancesIndirect( ctx, GeometryDrawGroup::eAll, false );
                mDiagnostics.mSubmittedBatches += mDiagnostics.mCasterBatches;
            }
            else if ( view.mDrawCount != 0 ) {
                ctx.DrawIndirect( DrawIndirectState{}
                    .SetFirstDraw( view.mFirstDraw )
                    .SetDrawCount( view.mDrawCount )
                    .SetBuffer( shadows.mIndirectBuffer ) );
                mDiagnostics.mSubmittedBatches += view.mDrawCount;
            }

            ++mDiagnostics.mSubmittedMaps;
            ctx.EndRender();
        }
    }

    auto ShadowMappingModule::SafeLightDirection( core::float3 direction ) -> core::float3 {
        MKT_BEGIN_PROFILER();
        const core::f32 length2 = glm::dot( direction, direction );
        return length2 > 1e-12f ? direction / glm::sqrt( length2 ) : core::float3{ 0, -1, 0 };
    }

    auto ShadowMappingModule::LightView( core::float3 position, core::float3 direction ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        direction = SafeLightDirection( direction );
        const core::float3 up = glm::abs( direction.y ) > .99f ? core::float3{ 0, 0, 1 } : core::float3{ 0, 1, 0 };
        return glm::lookAtRH( position, position + direction, up );
    }

    auto ShadowMappingModule::PointShadowMatrices( core::float3 position, core::f32 radius ) -> eastl::array<core::float4x4, 6> {
        MKT_BEGIN_PROFILER();
        const eastl::array<core::float3, 6> directions{ { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } } };
        const eastl::array<core::float3, 6> ups{ { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } } };
        const core::f32 farPlane = eastl::max( radius, .02f );
        const core::f32 nearPlane = eastl::max( .001f, farPlane * .001f );
        const auto projection = glm::perspectiveRH_ZO( glm::radians( 90.0f ), 1.0f, nearPlane, farPlane );
        eastl::array<core::float4x4, 6> result;

        for ( core::usize i = 0; i < 6; ++i ) {
            result[i] = projection * glm::lookAtRH( position, position + directions[i], ups[i] );
        }

        return result;
    }

    auto ShadowMappingModule::SpotShadowMatrix( core::float3 position, core::float3 direction, core::f32 outerCosine, core::f32 radius ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        const core::f32 farPlane = eastl::max( radius, .02f );
        const core::f32 nearPlane = eastl::max( .001f, farPlane * .001f );
        const core::f32 angle = glm::acos( eastl::clamp( outerCosine, .001f, .99999f ) );
        return glm::perspectiveRH_ZO( angle * 2, 1.0f, nearPlane, farPlane ) * LightView( position, direction );
    }

    auto ShadowMappingModule::BuildDirectionalCascades( const core::float4x4& inverseProjection,
        const core::float4x4& inverseView, core::f32 nearPlane, core::f32 farPlane,
        core::float3 direction, core::u32 resolution, core::f32 distance ) -> DirectionalCascadeData {
        MKT_BEGIN_PROFILER();
        DirectionalCascadeData result;
        nearPlane = eastl::max( nearPlane, 1e-4f );
        farPlane = eastl::max( farPlane, nearPlane + 1e-3f );
        const core::f32 end = eastl::min( farPlane, eastl::max( distance, nearPlane + 1e-3f ) );
        // Shaders use w as the coverage limit even with a single shadow map.
        result.splits = core::float4{ end };

        // Reconstruct the camera frustum once before fitting each cascade.
        eastl::array<core::float3, 4> nearCorners{};
        eastl::array<core::float3, 4> farCorners{};

        for ( int i = 0; i < 4; ++i ) {
            const core::f32 x = ( i & 1 ) ? 1.0f : -1.0f;
            const core::f32 y = ( i & 2 ) ? 1.0f : -1.0f;
            const auto a = inverseProjection * core::float4{ x, y, 0, 1 };
            // An interior depth avoids the near-zero homogeneous w of very distant far planes.
            const auto b = inverseProjection * core::float4{ x, y, 0.5f, 1 };
            nearCorners[i] = core::float3( a ) / a.w;
            const core::float3 interior{ core::float3( b ) / b.w };
            const core::float3 ray{ interior - nearCorners[i] };
            farCorners[i] = nearCorners[i] + ray * ( ( -farPlane - nearCorners[i].z ) / ray.z );
        }

        // Fit each split in light space, with overlap for cross-cascade blending.
        const auto rotation = LightView( {}, direction );
        core::f32 previous = nearPlane;

        for ( core::u32 cascade = 0; cascade < mDirectionalCascadeCount; ++cascade ) {
            const core::f32 fraction = core::f32( cascade + 1 ) / mDirectionalCascadeCount;
            const core::f32 split = .7f * nearPlane * glm::pow( end / nearPlane, fraction ) + .3f * ( nearPlane + ( end - nearPlane ) * fraction );
            result.splits[cascade] = split;
            // PBR blends the next map over the final 10% of the current slice.
            // Fit that same overlap into the next cascade so every blend sample is covered.
            const core::f32 previousStart = cascade > 1 ? result.splits[cascade - 2] : nearPlane;
            const core::f32 receiverStart = cascade > 0 ? previous - .1f * ( previous - previousStart ) : nearPlane;
            eastl::array<core::float3, 8> corners;
            core::float3 center{};

            for ( int i = 0; i < 4; ++i ) {
                corners[i] = glm::mix( nearCorners[i], farCorners[i], ( receiverStart - nearPlane ) / ( farPlane - nearPlane ) );
                corners[i + 4] = glm::mix( nearCorners[i], farCorners[i], ( split - nearPlane ) / ( farPlane - nearPlane ) );
                center += corners[i] + corners[i + 4];
            }

            center /= 8.0f;
            core::f32 radius{};

            for ( const auto& corner: corners ) {
                radius = eastl::max( radius, glm::length( corner - center ) );
            }

            // Tardif's stable sphere footprint. Radius is measured before the camera's
            // world transform, so a camera orbit cannot resize the orthographic footprint.
            // https://alextardif.com/shadowmapping.html
            // https://therealmjp.github.io/posts/shadow-maps/
            radius = eastl::max( .1f, glm::ceil( radius * 16.0f ) / 16.0f );
            const core::f32 mapSize{ as<core::f32>( eastl::max( resolution, 4u ) ) };
            // Reserve one texel around the fitted sphere BEFORE computing the snap grid.
            // Padding afterward would make the snap interval differ from the actual texel width.
            radius *= mapSize / ( mapSize - 2.0f );
            const core::f32 texel = 2 * radius / mapSize;
            const auto lightCenter = core::float3( rotation * inverseView * core::float4( center, 1 ) );
            const core::f32 x = glm::round( lightCenter.x / texel );
            const core::f32 y = glm::round( lightCenter.y / texel );
            core::f32 zMin = eastl::numeric_limits<core::f32>::max();
            core::f32 zMax = -zMin;

            for ( const auto& corner: corners ) {
                const core::f32 z = ( rotation * inverseView * core::float4( corner, 1 ) ).z;
                zMin = eastl::min( zMin, z );
                zMax = eastl::max( zMax, z );
            }

            // Include casters upstream of the receiver slice, including off-screen geometry.
            const core::f32 padding = eastl::max( distance, 10.0f );
            // Centered bounds avoid cancellation in (x + radius) - (x - radius)
            // far from the origin. Apply the snapped translation directly in NDC.
            auto projection{ glm::orthoRH_ZO( -radius, radius, -radius, radius, -zMax - padding, -zMin + padding ) };
            projection[3].x = -2.0f * x / mapSize;
            projection[3].y = -2.0f * y / mapSize;
            result.matrices[cascade] = projection * rotation;
            previous = split;
        }

        return result;
    }
}// namespace mikoto::renderer
