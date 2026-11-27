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
#include <cstring>

#include <EASTL/sort.h>

#include <Core/String.hh>

#include <Renderer/Clouds/CloudLighting.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/CloudsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/TerrainGenerationModule.hh>

namespace mikoto::renderer {

    using namespace core;
    using namespace rhi;

    static constexpr u32 kCloudSunMapSize{ 1024 };
    static constexpr u32 kCloudLocalMapSize{ 256 };
    static constexpr u32 kCloudShadowCommandCapacity{ 262144 };

    auto CloudsModule::SetGeometryManager( GeometryCullModule &geometry ) -> void {
        mGeometryManagement = &geometry;
    }

    auto CloudsModule::PrepareBolt( const CloudLightningSettings &settings, const CloudLightningState &state ) -> void {
        BuildCloudBolt( settings, state, mBolts );
    }

    // Select up to eight nearby point/spot sources by contribution; internal flashes
    // have priority. Each cloud owns a quarter-field-resolution local-light cache,
    // updated every frame. Finite RGB Beer paths include overlapping clouds, range,
    // cones, HG phase, mesh maps and exact terrain-triangle visibility. A light's Cast
    // shadows controls geometry visibility; cloud attenuation still applies. Receive
    // shadows disables incident-light occlusion consistently for sun and local lights.
    auto CloudsModule::PrepareLocalLighting() -> void {
        struct Candidate {
            CloudLocalLight mLight{};
            f32 mPriority{};
            u64 mGuid{};
        };
        eastl::vector<Candidate> candidates{};
        const auto add{ [&]( CloudLocalLight light, u64 guid, bool flash ) {
            const float3 position{ light.mPositionRange };
            const f32 radius{ light.mPositionRange.w };
            if ( radius <= 0 || light.mColorIntensity.w <= 0 || mRecords.empty() ) {
                return;
            }
            bool intersects{};
            for ( const auto &cloud: mRecords ) {
                const float3 local{ cloud.mWorldToLocal * float4{ position, 1 } };
                // Transform a world sphere conservatively into the cloud's local box.
                const float3 reach{ radius * glm::length( float3{ cloud.mWorldToLocal[0][0], cloud.mWorldToLocal[1][0], cloud.mWorldToLocal[2][0] } ),
                    radius * glm::length( float3{ cloud.mWorldToLocal[0][1], cloud.mWorldToLocal[1][1], cloud.mWorldToLocal[2][1] } ),
                    radius * glm::length( float3{ cloud.mWorldToLocal[0][2], cloud.mWorldToLocal[1][2], cloud.mWorldToLocal[2][2] } ) };
                intersects |= glm::all( glm::lessThanEqual( glm::abs( local ), reach + .5f ) );
            }
            if ( !intersects ) {
                return;
            }
            const float3 delta{ position - float3{ mFrame.mCamera } };
            const f32 brightness{ glm::max( light.mColorIntensity.x, glm::max( light.mColorIntensity.y, light.mColorIntensity.z ) ) * light.mColorIntensity.w };
            candidates.push_back( { light, flash ? 1e30f : brightness / glm::max( glm::dot( delta, delta ), 1.0f ), guid } );
        } };

        for ( u32 i{}; i < mRecords.size(); ++i ) {
            const auto &cloud{ mRecords[i] };
            CloudLocalLight light{};
            light.mPositionRange = cloud.mLightningPositionRadius;
            light.mColorIntensity = cloud.mLightningColorIntensity;
            light.mColorIntensity.w *= light.mPositionRange.w * light.mPositionRange.w * .01f;
            light.mParameters.y = 1;
            light.mControl = { 1, 1, 0, 0 };
            add( light, i, true );
        }

        for ( const auto [entity, tag, transform, component]: mScene->GetRegistry().view<scene::TagComponent, scene::TransformComponent, scene::LightComponent>().each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }
            const auto type{ component.GetActiveType() };
            if ( type != LightType::ePoint && type != LightType::eSpot ) {
                continue;
            }
            const auto world{ transform.GetWorldTransform() };
            CloudLocalLight light{};
            light.mPositionRange = float4{ float3{ world[3] }, 0 };
            if ( type == LightType::ePoint ) {
                const auto &source{ component.Get<PointLight>() };
                light.mPositionRange.w = source.GetRadius();
                light.mColorIntensity = { source.GetColor(), source.GetIntensity() };
                light.mParameters.y = source.GetSourceRadius();
                light.mControl = { 1, source.IsShadowCaster() ? 1u : 0u, 0, 0 };
            }
            else {
                const auto &source{ component.Get<SpotLight>() };
                light.mPositionRange.w = source.GetRadius();
                light.mDirectionOuter = { source.GetWorldDirection( world ), source.GetOuterCutOff() };
                light.mColorIntensity = { source.GetColor(), source.GetIntensity() };
                light.mParameters = { source.GetCutOff(), .1f, 0, 0 };
                light.mControl = { 2, source.IsShadowCaster() ? 1u : 0u, 0, 0 };
            }
            add( light, tag.GetGuid(), false );
        }
        eastl::sort( candidates.begin(), candidates.end(), []( const auto &a, const auto &b ) {
            return a.mPriority != b.mPriority ? a.mPriority > b.mPriority : a.mGuid < b.mGuid;
        } );
        u64 hash{ 1469598103934665603ull };
        for ( usize i{}; i < glm::min<usize>( candidates.size(), kMaxCloudLocalLights ); ++i ) {
            auto light{ candidates[i].mLight };
            light.mControl.z = mCapacity + as<u32>( i );
            mLocalLights.push_back( light );
            const auto *bytes{ reinterpret_cast<const ubyte *>( &light ) };
            for ( usize j{}; j < sizeof( light ); ++j ) {
                hash = ( hash ^ bytes[j] ) * 1099511628211ull;
            }
        }
        if ( hash != mLocalLightHash || !mBolts.empty() ) {
            mFrame.mControl.z = 0;
        }
        mLocalLightHash = hash;
        mFrame.mLightingControl = { as<u32>( mLocalLights.size() ), as<u32>( mBolts.size() ), as<u32>( mRecords.size() ), 0 };
    }

    auto CloudsModule::PrepareInteractions() -> void {
        eastl::vector<entt::entity> entities{};
        const auto &registry{ mScene->GetRegistry() };
        for ( const auto [entity, tag, transform, interaction]: registry.view<scene::TagComponent, scene::TransformComponent, scene::CloudInteractionComponent>().each() ) {
            if ( tag.IsActive() && interaction.IsEnabled() && interaction.GetStrength() > 0 ) {
                entities.push_back( entity );
            }
        }
        eastl::sort( entities.begin(), entities.end(), [&]( auto a, auto b ) {
            return registry.get<scene::TagComponent>( a ).GetGuid() < registry.get<scene::TagComponent>( b ).GetGuid();
        } );
        u64 hash{ 1469598103934665603ull };
        for ( const auto entity: entities ) {
            if ( mInteractions.size() == 64 ) {
                break;
            }
            const auto &interaction{ registry.get<scene::CloudInteractionComponent>( entity ) };
            const auto world{ registry.get<scene::TransformComponent>( entity ).GetWorldTransform() * glm::scale( float4x4{ 1 }, interaction.GetDimensions() ) };
            if ( glm::abs( glm::determinant( world ) ) < 1e-12f ) {
                continue;
            }
            const float3 center{ world[3] };
            const float3 extent{ ( glm::abs( float3{ world[0] } ) + glm::abs( float3{ world[1] } ) + glm::abs( float3{ world[2] } ) ) * .5f };
            if ( glm::any( glm::lessThan( center + extent, float3{ mFrame.mBoundsMin } ) ) ||
                 glm::any( glm::greaterThan( center - extent, float3{ mFrame.mBoundsMax } ) ) ) {
                continue;
            }
            CloudInteractionRecord record{ glm::inverse( world ), { as<u32>( interaction.GetShape() ), interaction.GetStrength(), interaction.GetFeather(), 0 } };
            mInteractions.push_back( record );
            const auto *bytes{ reinterpret_cast<const ubyte *>( &record ) };
            for ( usize i{}; i < sizeof( record ); ++i ) {
                hash = ( hash ^ bytes[i] ) * 1099511628211ull;
            }
        }
        if ( hash != mInteractionHash ) {
            mFrame.mControl.z = 0;
            mResetLighting = true;
            mFrame.mFiltering.y = 1;
        }
        mInteractionHash = hash;
        mFrame.mLightingControl.w = as<u32>( mInteractions.size() );
    }

    auto CloudsModule::RegisterMeshLighting( FrameGraph &graph ) -> void {
        auto &info{ graph.GetOrCreate<CloudsModuleInfo>() };
        const auto buffer{ [&]( cstr name, usize size, bool indirect = false ) {
            return graph.Create( FGBufferDescription{}
                .SetName( name )
                .SetSizeBytes( size )
                .SetHeapType( HeapType::eDeviceLocal )
                .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest |
                               ( indirect ? BufferUsageFlagsBits::IndirectDraw : BufferUsageFlagsBits::Structured ) ) );
        } };
        info.mLocalLights = buffer( "Clouds_LocalLights", kMaxCloudLocalLights * sizeof( CloudLocalLight ) );
        info.mMeshShadows = buffer( "Clouds_MeshShadows", ( mCapacity + kMaxCloudLocalLights ) * sizeof( CloudMeshShadow ) );
        info.mShadowCommands = buffer( "Clouds_ShadowCommands", kCloudShadowCommandCapacity * sizeof( DrawIndirectCommand ), true );
        info.mBolts = buffer( "Clouds_Bolts", kMaxCloudBoltSegments * sizeof( CloudBoltSegment ) );
        const auto map{ [&]( const eastl::string &name, u32 size ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( size )
                .SetHeight( size )
                .SetFormat( Format::eD32 )
                .SetUsage( TextureUsageFlagsBits::DepthTarget | TextureUsageFlagsBits::ShaderResource ) );
        } };
        for ( u32 i{}; i < mCapacity; ++i ) {
            info.mSunMaps[i] = map( string::Format( "Clouds_SunMesh_{}", i ), kCloudSunMapSize );
        }
        for ( u32 i{}; i < kMaxCloudLocalLights; ++i ) {
            for ( u32 face{}; face < 6; ++face ) {
                info.mLocalMaps[i][face] = map( string::Format( "Clouds_LocalMesh_{}_{}", i, face ), kCloudLocalMapSize );
            }
        }
        graph.RegisterPass(
            "Pass_Clouds_LightUpload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &info{ b.Get<CloudsModuleInfo>() };
                for ( const auto handle: { info.mFrame, info.mLocalLights, info.mMeshShadows, info.mShadowCommands, info.mBolts } ) {
                    builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
                // Caster culling uses the same up-to-date poses and instance bounds as raster geometry.
                builder.UseResource( b.Get<GeometryCullModuleInfo>().mUnculledIndirectBuffer, FGPipelineStage::eCopy, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                UploadMeshLighting( ctx, b );
            } );

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_MeshShadowPipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetCullMode( CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( true )
            .SetDepthFormat( Format::eD32 )
            .SetBlendEnable( false )
            .PushShader( "DirLightShadows_Vert.slang", FGStageType::eVertex )
            .PushShader( "DirLightShadows_Frag.slang", FGStageType::ePixel ) ) };
        graph.RegisterPass(
            "Pass_Clouds_MeshShadows",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &info{ b.Get<CloudsModuleInfo>() };
                const auto &geometry{ b.Get<GeometryCullModuleInfo>() };
                for ( u32 i{}; i < mCapacity; ++i ) {
                    builder.UseResource( info.mSunMaps[i], FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                }
                for ( const auto &faces: info.mLocalMaps ) {
                    for ( const auto handle: faces ) {
                        builder.UseResource( handle, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                    }
                }
                builder.UseResource( info.mMeshShadows, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                for ( const auto handle: { info.mShadowCommands, geometry.mUnculledIndirectBuffer } ) {
                    builder.UseResource( handle, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
                }
                for ( const auto handle: { geometry.mGeometryBuffer, geometry.mInstanceBuffer, geometry.mDrawInstanceBuffer,
                          geometry.mSkinningBuffer, geometry.mSkinningCacheBuffer, geometry.mGeometryAllocBuffer, geometry.mAttributeBuffer, geometry.mIndexBuffer } ) {
                    builder.UseResource( handle, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                }
                builder.UseResource( geometry.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
            },
            [this, pipeline]( CommandContext &ctx, Blackboard &b ) {
                if ( !mGeometryManagement ) {
                    return;
                }
                const auto &geometry{ b.Get<GeometryCullModuleInfo>() };
                const auto &info{ b.Get<CloudsModuleInfo>() };
                struct Parameters {
                    SPointer mGeometry{};
                    SPointer mSkinning{};
                    SPointer mAllocation{};
                    SPointer mRecords{};
                    SPointer mMaterials{};
                    u32 mSampler{};
                    u32 mRecord{};
                    u32 mFace{};
                    u32 mPadding{};
                } parameters{
                    .mGeometry = ctx.GetDeviceBufferAddress( geometry.mGeometryBuffer ),
                    .mSkinning = ctx.GetDeviceBufferAddress( geometry.mSkinningBuffer ),
                    .mAllocation = ctx.GetDeviceBufferAddress( geometry.mGeometryAllocBuffer ),
                    .mRecords = ctx.GetDeviceBufferAddress( info.mMeshShadows ),
                    .mMaterials = ctx.GetDeviceBufferAddress( geometry.mMaterialsBuffer ),
                    .mSampler = ctx.PushSampler( geometry.mMaterialSampler ),
                };
                for ( const auto &view: mShadowViews ) {
                    parameters.mRecord = view.mRecord;
                    parameters.mFace = view.mFace;
                    ctx.BeginRender( RenderBeginDescription{}
                        .SetRenderArea( Rect{ 0, as<i32>( view.mSize ), 0, as<i32>( view.mSize ) } )
                        .AddDepthTarget( view.mTarget, LoadOp::eClear ) );
                    ctx.SetViewportState( ViewportState{}.AddViewportAndScissorRect( Viewport{ 0, float( view.mSize ), 0, float( view.mSize ), 0, 1 } ) );
                    ctx.BindPipeline( pipeline );
                    ctx.PushConstants( parameters );
                    if ( view.mUseUnculled ) {
                        mGeometryManagement->DrawInstancesIndirect( ctx, GeometryDrawGroup::eAll, false );
                    }
                    else if ( view.mDrawCount ) {
                        ctx.DrawIndirect( DrawIndirectState{}.SetFirstDraw( view.mFirstDraw ).SetDrawCount( view.mDrawCount ).SetBuffer( info.mShadowCommands ) );
                    }
                    ctx.EndRender();
                }
            } );
        RegisterLocalLighting( graph );
    }

    auto CloudsModule::UploadMeshLighting( CommandContext &ctx, Blackboard &b ) -> void {
        const auto &info{ b.Get<CloudsModuleInfo>() };
        mMeshShadows.assign( mCapacity + kMaxCloudLocalLights, CloudMeshShadow{} );
        mShadowViews.clear();
        mShadowCommands.clear();
        const auto add{ [&]( u32 record, u32 face, FGTextureHandle target, u32 size ) {
            auto &shadow{ mMeshShadows[record] };
            shadow.mTextureIDs[face] = ctx.PushTexture_SRV( target );
            ShadowView view{ target, record, face, size, as<u32>( mShadowCommands.size() ), 0, false };
            if ( mGeometryManagement ) {
                u32 tested{};
                u32 culled{};
                view.mUseUnculled = !mGeometryManagement->AppendShadowDraws( shadow.mViewProjection[face], mShadowCommands,
                    kCloudShadowCommandCapacity, tested, culled );
                view.mDrawCount = as<u32>( mShadowCommands.size() ) - view.mFirstDraw;
            }
            mShadowViews.push_back( view );
        } };
        for ( u32 i{}; i < mRecords.size(); ++i ) {
            auto &shadow{ mMeshShadows[i] };
            if ( ( mRecords[i].mFlags.w & 2u ) == 0 ) {
                continue;
            }
            shadow.mFaceCount = 1;
            shadow.mLightType = 0;
            shadow.mViewProjection[0] = BuildCloudSunShadow( mRecords[i].mLocalToWorld, float3{ mFrame.mSunDirection }, kCloudSunMapSize, mSettings.mMaxDistance );
            const auto &matrix{ shadow.mViewProjection[0] };
            const float depthScale{ glm::length( float3{ matrix[0].z, matrix[1].z, matrix[2].z } ) };
            shadow.mBias.x = .05f * depthScale;
            add( i, 0, info.mSunMaps[i], kCloudSunMapSize );
        }
        for ( u32 i{}; i < mLocalLights.size(); ++i ) {
            const auto &light{ mLocalLights[i] };
            if ( !light.mControl.y ) {
                continue;
            }
            const u32 index{ mCapacity + i };
            auto &shadow{ mMeshShadows[index] };
            shadow.mLightType = as<i32>( light.mControl.x );
            shadow.mPositionRange = light.mPositionRange;
            shadow.mFaceCount = light.mControl.x == 1 ? 6 : 1;
            shadow.mBias.x = .01f / light.mPositionRange.w;
            for ( u32 face{}; face < shadow.mFaceCount; ++face ) {
                shadow.mViewProjection[face] = BuildCloudLocalShadow( float3{ light.mPositionRange }, float3{ light.mDirectionOuter },
                    light.mPositionRange.w, light.mDirectionOuter.w, face, light.mControl.x == 1 );
                add( index, face, info.mLocalMaps[i][face], kCloudLocalMapSize );
            }
        }
        if ( mGeometryManagement ) {
            const auto revision{ mGeometryManagement->GetShadowCasterRevision() };
            if ( revision != mCasterRevision ) {
                mFrame.mControl.z = 0;
            }
            mCasterRevision = revision;
        }
        ctx.CopyBuffer( info.mFrame, 0, &mFrame, sizeof( mFrame ) );
        ctx.CopyBuffer( info.mMeshShadows, 0, mMeshShadows.data(), mMeshShadows.size() * sizeof( CloudMeshShadow ) );
        if ( !mLocalLights.empty() ) {
            ctx.CopyBuffer( info.mLocalLights, 0, mLocalLights.data(), mLocalLights.size() * sizeof( CloudLocalLight ) );
        }
        if ( !mBolts.empty() ) {
            ctx.CopyBuffer( info.mBolts, 0, mBolts.data(), mBolts.size() * sizeof( CloudBoltSegment ) );
        }
        if ( !mShadowCommands.empty() ) {
            ctx.CopyBuffer( info.mShadowCommands, 0, mShadowCommands.data(), mShadowCommands.size() * sizeof( DrawIndirectCommand ) );
        }
    }

    auto CloudsModule::RegisterLocalLighting( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_LocalLightingPipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsLocalLighting_Comp.slang", FGStageType::eCompute ) ) };
        for ( u32 i{}; i < mCapacity; ++i ) {
            const auto output{ graph.GetData<CloudsModuleInfo>().mVolumes[i].mLocalLighting };
            graph.RegisterPass(
                string::Format( "Pass_Clouds_LocalLighting_{}", i ),
                FGPassType::eCompute,
                [output]( FGNodeBuilder &builder, Blackboard &b ) {
                    const auto &info{ b.Get<CloudsModuleInfo>() };
                    const auto &terrain{ b.Get<TerrainGenerationInfo>() };
                    for ( const auto handle: { info.mFrame, info.mModels, info.mInteractions, info.mLocalLights, info.mMeshShadows, info.mDistanceFields,
                              terrain.mVertices, terrain.mSamplingRecords, terrain.mHeightBounds } ) {
                        builder.UseResource( handle, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                    for ( const auto &volume: info.mVolumes ) {
                        builder.UseResource( volume.mModel, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                    for ( const auto handle: info.mNoiseMips ) {
                        builder.UseResource( handle, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                    builder.UseResource( info.mNoise, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    for ( const auto &faces: info.mLocalMaps ) {
                        for ( const auto handle: faces ) {
                            builder.UseResource( handle, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        }
                    }
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                },
                [this, pipeline, output, i]( CommandContext &ctx, Blackboard &b ) {
                    if ( i >= mRecords.size() || mLocalLights.empty() ) {
                        return;
                    }
                    struct Parameters {
                        SPointer mFrame{};
                        SPointer mModels{};
                        SPointer mCamera{};
                        u32 mOutput{};
                        u32 mMetadata{};
                        u32 mInput{};
                        u32 mInputMetadata{};
                        u32 mHistory{};
                        u32 mHistoryMetadata{};
                        u32 mDepth{};
                        u32 mVolume{};
                    } parameters{};
                    parameters.mFrame = ctx.GetDeviceBufferAddress( b.Get<CloudsModuleInfo>().mFrame );
                    parameters.mOutput = ctx.PushTexture_UAV( output );
                    parameters.mVolume = i;
                    ctx.BindPipeline( pipeline );
                    ctx.PushConstants( parameters );
                    ctx.Dispatch( ( mFrame.mResolution.x / 4 + 3 ) / 4, ( mFrame.mResolution.y / 4 + 3 ) / 4, ( mFrame.mResolution.z / 4 + 3 ) / 4 );
                } );
        }
    }

    auto CloudsModule::RegisterBolts( FrameGraph &graph ) -> void {
        const auto blend{ BlendState{}.SetSrcColorFactor( BlendFactor::eOne ).SetDstColorFactor( BlendFactor::eOne ) };
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_LightningPipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetCullMode( CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( true )
            .SetAlphaBlendState( blend )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "CloudsLightning_Vert.slang", FGStageType::eVertex )
            .PushShader( "CloudsLightning_Frag.slang", FGStageType::ePixel ) ) };
        graph.RegisterPass(
            "Pass_Clouds_LightningBolts",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &info{ b.Get<CloudsModuleInfo>() };
                builder.UseResource( info.mFrame, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( info.mBolts, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                for ( const auto handle: { info.mFrame, info.mModels, info.mInteractions, info.mDistanceFields } ) {
                    builder.UseResource( handle, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                for ( const auto &volume: info.mVolumes ) {
                    builder.UseResource( volume.mModel, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                for ( const auto handle: info.mNoiseMips ) {
                    builder.UseResource( handle, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                builder.UseResource( info.mNoise, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &ctx, Blackboard &b ) {
                if ( mBolts.empty() ) {
                    return;
                }
                struct Parameters {
                    SPointer mFrame{};
                    SPointer mCamera{};
                    u32 mDepth{};
                    u32 mPadding{};
                } parameters{
                    .mFrame = ctx.GetDeviceBufferAddress( b.Get<CloudsModuleInfo>().mFrame ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                    .mDepth = ctx.PushTexture_SRV( b.Get<PrepassModuleInfo>().mPrepassDepthTarget ),
                };
                const i32 width{ as<i32>( mFrame.mScreen.x ) };
                const i32 height{ as<i32>( mFrame.mScreen.y ) };
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( Rect{ 0, width, 0, height } )
                    .AddRenderTarget( b.Get<GeomShadingModuleInfo>().mColorImage, Color{}, LoadOp::eLoad ) );
                ctx.SetViewportState( ViewportState{}.AddViewportAndScissorRect( Viewport{ 0, float( width ), 0, float( height ), 0, 1 } ) );
                ctx.BindPipeline( pipeline );
                ctx.PushConstants( parameters );
                ctx.Draw( 6, as<u32>( mBolts.size() ) );
                ctx.EndRender();
            } );
    }

}// namespace mikoto::renderer
