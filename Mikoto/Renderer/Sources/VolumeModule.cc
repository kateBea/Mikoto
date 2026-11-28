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

#include <cstring>

#include <EASTL/sort.h>

#include <Core/Profiler.hh>

#include <Material/VolumeMaterial.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/Volumes.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/CloudsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>
#include <Renderer/ForwardRenderer/Passes/VolumeModule.hh>

#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

    // https://alextardif.com/ParticipatingMedia.html

    struct FogParameters {
        SPointer mVolumes{};
        SPointer mCamera{};
        SPointer mFrame{};
        SPointer mLights{};
        SPointer mShadows{};
        SPointer mClusters{};

        u32 mLighting{};
        u32 mExtinction{};
        u32 mHistory{};
        u32 mExtinctionHistory{};
        u32 mRadiance{};
        u32 mTransmittance{};
        u32 mSource{};
        u32 mDepth{};

        u32 mSampler{};
        u32 mLightCount{};
        u32 mShadowCount{};
        u32 mStage{};
        u32 mIrradiance{};
        f32 mAmbientScale{};
        float2 mLightGridXY{};
        f32 mLightGridZ{};
        u32 mClusterCount{};
    };

    struct VolumeEntry {
        u64 mID{};
        VolumeRecord mRecord{};
        VolumeMaterialHandle mMaterial{};
    };

    static auto MakeFogParameters( CommandContext &context, Blackboard &blackboard, u32 stage ) -> FogParameters {
        const auto &fog{ blackboard.Get<FogModuleInfo>() };
        const auto &prepass{ blackboard.Get<PrepassModuleInfo>() };
        const auto &shadows{ blackboard.Get<ShadowMapInfo>() };

        FogParameters parameters{
            .mFrame = context.GetDeviceBufferAddress( fog.mFrame ),
            .mSampler = stage != 1 ? context.PushSampler( fog.mSampler ) : 0,
            .mStage = stage
        };

        if ( stage == 0 ) {
            parameters.mVolumes = context.GetDeviceBufferAddress( blackboard.Get<VolumeModuleInfo>().mData );
            parameters.mCamera = context.GetDeviceBufferAddress( blackboard.Get<CameraModuleInfo>().mCameraData );
            parameters.mLights = context.GetDeviceBufferAddress( prepass.mLightsBuffer );
            parameters.mShadows = context.GetDeviceBufferAddress( shadows.mRecordsBuffer );
            parameters.mClusters = context.GetDeviceBufferAddress( prepass.mClusterBuffer );

            parameters.mLighting = context.PushUAV( fog.mLighting );
            parameters.mExtinction = context.PushUAV( fog.mExtinction );
            parameters.mHistory = context.PushSRV( fog.mHistory );
            parameters.mExtinctionHistory = context.PushSRV( fog.mExtinctionHistory );
            parameters.mIrradiance = context.PushSRV( blackboard.Get<GeomShadingModuleInfo>().mIrradianceCubeRT );

            parameters.mLightCount = prepass.mActiveLightCount;
            parameters.mShadowCount = shadows.mRecordCount;
            parameters.mLightGridXY = float2{ prepass.mGridSize };
            parameters.mLightGridZ = prepass.mGridSize.z;
            parameters.mClusterCount = prepass.mClusterCount;
        }
        else if ( stage == 1 ) {
            parameters.mLighting = context.PushSRV( fog.mLighting );
            parameters.mExtinction = context.PushSRV( fog.mExtinction );

            parameters.mRadiance = context.PushUAV( fog.mRadiance );
            parameters.mTransmittance = context.PushUAV( fog.mTransmittance );
        }
        else if ( stage == 2 ) {
            parameters.mCamera = context.GetDeviceBufferAddress( blackboard.Get<CameraModuleInfo>().mCameraData );
            parameters.mSource = context.PushSRV( fog.mSceneCopy );
            parameters.mDepth = context.PushSRV( prepass.mPrepassDepthTarget );

            parameters.mRadiance = context.PushSRV( fog.mRadiance );
            parameters.mTransmittance = context.PushSRV( fog.mTransmittance );
        }
        else {
            parameters.mSource = context.PushSRV( blackboard.Get<GeomShadingModuleInfo>().mColorImage );
        }

        return parameters;
    }

    static auto VolumeHash( const void *data, usize bytes ) -> u64 {
        u64 hash{ 1469598103934665603ull };
        const auto *values{ static_cast<const ubyte *>( data ) };
        for ( usize i{}; i < bytes; ++i ) {
            hash = ( hash ^ values[i] ) * 1099511628211ull;
        }

        return hash;
    }

    struct VolumeMajorantGrid {
        uint3 mDimensions{};
        eastl::vector<f32> mValues{};
    };

    // Include every texel whose trilinear filter can touch a coarse cell.
    // These are upper bounds, not averages: underestimates bias null tracking.
    static auto BuildVolumeMajorants( uint3 size, const eastl::vector<f32> &density ) -> VolumeMajorantGrid {
        VolumeMajorantGrid result{};
        result.mDimensions = glm::max( ( size - 1u + 7u ) / 8u, uint3{ 1 } );
        const auto cells{ result.mDimensions };
        result.mValues.resize( usize( cells.x ) * cells.y * cells.z );
        for ( u32 z{}; z < cells.z; ++z ) {
            for ( u32 y{}; y < cells.y; ++y ) {
                for ( u32 x{}; x < cells.x; ++x ) {
                    const uint3 cell{ x, y, z };
                    const uint3 lo{ cell * ( size - 1u ) / cells };
                    const uint3 hi{ ( ( cell + 1u ) * ( size - 1u ) + cells - 1u ) / cells };
                    f32 maximum{};
                    for ( u32 iz{ lo.z }; iz <= hi.z; ++iz ) {
                        for ( u32 iy{ lo.y }; iy <= hi.y; ++iy ) {
                            for ( u32 ix{ lo.x }; ix <= hi.x; ++ix ) {
                                maximum = glm::max( maximum, density[ix + usize( size.x ) * ( iy + usize( size.y ) * iz )] );
                            }
                        }
                    }
                    result.mValues[x + usize( cells.x ) * ( y + usize( cells.y ) * z )] = maximum;
                }
            }
        }
        return result;
    }

    static auto MakeVolumeRecord( const VolumeMaterial &material, float4x4 world, float3 dimensions,
        f32 referenceHeight, f32 multiplier, bool sphere, bool casts, bool receives, float3 wind ) -> VolumeRecord {
        const auto &s{ material.GetSettings() };
        const MediumProperties properties{
            .mAbsorption = s.mAbsorption,
            .mScattering = s.mScattering
        };

        VolumeRecord result{};
        result.mWorldToLocal = glm::inverse( world * glm::scale( float4x4{ 1 }, dimensions ) );
        result.mAbsorptionDensity = float4{ s.mAbsorption, s.mDensity * multiplier };
        result.mScatteringPhase = float4{ s.mScattering, s.mAnisotropy };
        result.mDimensionsHeight = float4{ dimensions, referenceHeight };
        result.mNoise = float4{ s.mNoiseScale, s.mErosionScale, s.mErosion, s.mCoverage };
        result.mFlags = uint4{ sphere, as<u32>( s.mDensityModel ), casts, receives };
        result.mWindOctaves = float4{ wind, as<f32>( s.mOctaves ) };
        result.mCloudPattern = uint4{ s.mCloudFormationCount, s.mCloudSeed, 0, 0 };
        result.mCloudProfile = float4{ as<f32>( s.mCloudForm ), s.mCloudProfileDepth,
            s.mCloudFormationScale, s.mCloudFormationVariation };
        result.mCloudWeather = float4{ s.mWeatherScale, s.mCoverageVariation,
            s.mCloudTypeVariation, s.mCloudHeightVariation };

        // Height falloff is otherwise unused for the procedural/grid density models.
        if ( s.mDensityModel == VolumeDensityModel::eExponentialHeight ) {
            result.mNoise.x = s.mHeightFalloff;
        }

        result.mGrid = uint4{ material.GetGridDimensions(), 0 };
        const f32 bound{ s.mDensityModel == VolumeDensityModel::eDensityGrid && material.GetDensityGrid().empty() ? 0.0f : s.mDensity * multiplier };
        result.mExtinctionMajorant = float4{ properties.GetExtinction() * bound, bound };
        return result;
    }

    VolumeModule::VolumeModule( RenderResolution resolution ) : mResolution{ resolution } {
        mData.resize( 16, 0 );
    }

    auto VolumeModule::GetData() const -> const eastl::vector<core::ubyte> & {
        return mData;
    }

    auto VolumeModule::Prepare( const Scene *scene, const Camera *camera, f32 ambientScale, bool includeClouds ) -> bool {
        MKT_BEGIN_PROFILER();

        mHistoryValid &= mScene == scene && mCamera == camera && mAmbientScale == ambientScale;
        mScene = scene;
        mCamera = camera;
        mAmbientScale = ambientScale;

        eastl::vector<VolumeEntry> entries{};

        if ( scene ) {
            const auto &registry{ scene->GetRegistry() };
            for ( const auto [entity, tag, transform, source]: registry.view<TagComponent, TransformComponent, CloudComponent>().each() ) {
                if ( !includeClouds || !tag.IsActive() || !source.GetMaterial() ) {
                    continue;
                }
                const auto &cloud{ source };
                auto world{ transform.GetWorldTransform() };
                float3 dimensions{ cloud.GetDimensions() };
                if ( cloud.GetShape() == CloudShape::eSkyLayer ) {
                    world = glm::translate( float4x4{ 1 }, float3{ world[3].x, cloud.GetBottomAltitude() + cloud.GetLayerThickness() * .5f, world[3].z } );
                    dimensions = float3{ 2 * cloud.GetHorizonRadius(), cloud.GetLayerThickness(), 2 * cloud.GetHorizonRadius() };
                }
                if ( glm::abs( glm::determinant( world ) ) < 1e-12f ) {
                    continue;
                }
                const auto direction{ glm::length( cloud.GetWindDirection() ) > 0 ? glm::normalize( cloud.GetWindDirection() ) : float2{ 0 } };
                const float2 wind{ direction * cloud.GetWindSpeed() * as<f32>( scene->GetVolumeTime() ) };
                // Bounds and authored grids must move with the same wind as density.
                world[3] += float4{ wind.x, 0, wind.y, 0 };
                entries.push_back( { tag.GetGuid(), MakeVolumeRecord( *cloud.GetMaterial(), world, dimensions, world[3].y, cloud.GetDensityMultiplier(), cloud.GetShape() == CloudShape::eSphere, cloud.IsShadowCaster(), cloud.IsShadowReceiver(), float3{ wind.x, 0, wind.y } ), cloud.GetMaterial() } );
            }

            for ( const auto [entity, tag, transform, source]: registry.view<TagComponent, TransformComponent, FogComponent>().each() ) {
                if ( !tag.IsActive() || !source.GetMaterial() ) {
                    continue;
                }
                const auto &fog{ source };
                auto world{ transform.GetWorldTransform() };
                float3 dimensions{ fog.GetDimensions() };
                if ( fog.GetShape() == FogShape::eGlobal ) {
                    world = glm::translate( float4x4{ 1 }, float3{ world[3] } );
                    dimensions = float3{ 2 * fog.GetHorizonRadius() };
                }
                if ( glm::abs( glm::determinant( world ) ) < 1e-12f ) {
                    continue;
                }
                entries.push_back( { tag.GetGuid(), MakeVolumeRecord( *fog.GetMaterial(), world, dimensions, world[3].y, fog.GetDensityMultiplier(), fog.GetShape() != FogShape::eBox, fog.IsShadowCaster(), fog.IsShadowReceiver(), float3{ 0 } ), fog.GetMaterial() } );
            }
        }

        // Stable entity order keeps save/load and EnTT compaction from resetting accumulation.
        eastl::sort( entries.begin(), entries.end(), []( const VolumeEntry &a, const VolumeEntry &b ) {
            return a.mID < b.mID;
        } );

        mCount = as<u32>( entries.size() );
        const usize headerBytes{ 16 + entries.size() * sizeof( VolumeRecord ) };
        u64 payloadHash{ entries.size() };
        for ( const auto &entry: entries ) {
            const auto size{ entry.mMaterial->GetGridDimensions() };
            payloadHash = ( payloadHash ^ entry.mMaterial->GetGridHash() ^ VolumeHash( &size, sizeof( size ) ) ) * 1099511628211ull;
        }
        const bool payloadChanged{ payloadHash != mPayloadHash || mGridOffsets.size() != entries.size() };
        if ( payloadChanged || mData.size() < headerBytes ) {
            mPayloadDirty = true;
            mPayloadHash = payloadHash;
            mData.assign( headerBytes, 0 );
            mGridOffsets.assign( entries.size(), 0 );
            mMajorantLayouts.assign( entries.size(), uint4{} );
            for ( usize i{}; i < entries.size(); ++i ) {
                const auto &grid{ entries[i].mMaterial->GetDensityGrid() };
                if ( grid.empty() ) {
                    continue;
                }
                const usize offset{ mData.size() };
                mGridOffsets[i] = as<u32>( offset );
                mData.resize( offset + grid.size() * sizeof( f32 ) );
                std::memcpy( mData.data() + offset, grid.data(), grid.size() * sizeof( f32 ) );

                const auto bounds{ BuildVolumeMajorants( entries[i].mMaterial->GetGridDimensions(), grid ) };
                const usize majorantOffset{ mData.size() };
                mMajorantLayouts[i] = uint4{ bounds.mDimensions, as<u32>( majorantOffset ) };
                mData.resize( majorantOffset + bounds.mValues.size() * sizeof( f32 ) );
                std::memcpy( mData.data() + majorantOffset, bounds.mValues.data(), bounds.mValues.size() * sizeof( f32 ) );
            }
        }
        std::memcpy( mData.data(), &mCount, sizeof( mCount ) );
        for ( usize i{}; i < entries.size(); ++i ) {
            entries[i].mRecord.mGrid.w = mGridOffsets[i];
            entries[i].mRecord.mMajorantGrid = mMajorantLayouts[i];
            std::memcpy( mData.data() + 16 + i * sizeof( VolumeRecord ), &entries[i].mRecord, sizeof( VolumeRecord ) );
        }

        // Animated placement invalidates accumulation, but does not recopy or
        // rehash immutable voxel payloads or rebuild their majorant grids.
        const u64 hash{ VolumeHash( mData.data(), headerBytes ) ^ payloadHash };
        const bool changed{ hash != mHash };
        mHash = hash;
        mUploadDirty |= changed;
        mHistoryValid &= !changed && mCount != 0;

        return changed;
    }

    auto VolumeModule::RegisterUpload( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();
        mUploadDirty = mPayloadDirty = true;

        auto &info{ graph.GetOrCreate<VolumeModuleInfo>() };
        info.mData = graph.Create( FGBufferDescription{}
            .SetName( "Volumes_Data" )
            .SetUsage( BufferUsageFlagsBits::Structured |
                           BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 16 )
            .SetHeapType( HeapType::eDeviceLocal ) );

        // Allocate before the upload pass records barriers for the new storage.
        graph.RegisterPass(
            "Pass_Volumes_Prepare",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<VolumeModuleInfo>().mData, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, Blackboard &b ) {
                if ( context.EnsureBufferCapacity( b.Get<VolumeModuleInfo>().mData, mData.size() ) ) {
                    mPayloadDirty = mUploadDirty = true;
                }
            } );

        graph.RegisterPass(
            "Pass_Volumes_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<VolumeModuleInfo>().mData, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, Blackboard &b ) {
                if ( !mUploadDirty ) {
                    return;
                }

                const auto buffer{ b.Get<VolumeModuleInfo>().mData };
                const usize bytes{ mPayloadDirty ? mData.size() : 16 + mCount * sizeof( VolumeRecord ) };
                context.Copy( buffer, 0, mData.data(), bytes );
                mPayloadDirty = false;

                mUploadDirty = false;
            } );
    }

    auto VolumeModule::RegisterFog( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        // Design reference: Bart Wronski, SIGGRAPH 2014, "Volumetric Fog".
        // Use separate lighting injection, integration and composition passes.
        // Godot's implementation is the reference for temporal density/lighting history.
        // https://github.com/godotengine/godot/blob/master/servers/rendering/renderer_rd/environment/fog.cpp
        // https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf

        const auto dimensions{ InferDimensions( mResolution ) };
        const u32 width{ as<u32>( dimensions.first ) };
        const u32 height{ as<u32>( dimensions.second ) };
        mFogFrame.mGrid = uint4{ ( width + 15u ) / 16u, ( height + 15u ) / 16u, 64, 8 };
        mHistoryValid = false;
        auto &info{ graph.GetOrCreate<FogModuleInfo>() };
        info.mFrame = graph.Create( FGBufferDescription{}
            .SetName( "Fog_Frame" )
            .SetSizeBytes( sizeof( FogFrameData ) )
            .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest )
            .SetHeapType( HeapType::eDeviceLocal ) );
        auto volume{ FGTextureDescription{}
            .SetWidth( mFogFrame.mGrid.x * mFogFrame.mGrid.w )
            .SetHeight( mFogFrame.mGrid.y * mFogFrame.mGrid.w )
            .SetFormat( Format::eRGBA16_FLOAT )
            .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess |
                           TextureUsageFlagsBits::CopySource | TextureUsageFlagsBits::CopyDest ) };
        info.mLighting = graph.Create( volume.SetName( "Fog_Lighting" ) );
        info.mExtinction = graph.Create( volume.SetName( "Fog_Extinction" ) );
        info.mHistory = graph.Create( volume.SetName( "Fog_LightingHistory" ) );
        info.mExtinctionHistory = graph.Create( volume.SetName( "Fog_ExtinctionHistory" ) );
        info.mRadiance = graph.Create( volume.SetName( "Fog_IntegratedRadiance" ) );
        info.mTransmittance = graph.Create( volume.SetName( "Fog_IntegratedTransmittance" ) );
        info.mSceneCopy = graph.Create( FGTextureDescription{}
            .SetName( "Fog_SceneCopy" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( Format::eRGBA16_FLOAT )
            .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::CopyDest ) );
        info.mSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Fog_VolumeSampler" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge ) );

        // Register after allocation so the graph sees the frame's copy-write edge.
        graph.RegisterPass(
            "Pass_Fog_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<FogModuleInfo>().mFrame, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, Blackboard &b ) {
                if ( !mCamera ) {
                    return;
                }
                const auto &fog{ b.Get<FogModuleInfo>() };
                auto frame{ mFogFrame };
                const f32 distance{ glm::max( mCamera->GetFarPlane(), 0.001f ) };
                frame.mRange = float4{ 0.0f, distance, mFogFrame.mRange.y, mHistoryValid ? 0.9f : 0.0f };
                frame.mTextures = uint4{ context.PushSRV( fog.mRadiance ), context.PushSRV( fog.mTransmittance ),
                    context.PushSampler( fog.mSampler ), mCount };
                context.Copy( fog.mFrame, 0, &frame, sizeof( frame ) );

                mFogFrame.mPreviousViewProjection = mCamera->GetViewProjection();
                mFogFrame.mPreviousInverseViewProjection = glm::inverse( mFogFrame.mPreviousViewProjection );
                mFogFrame.mPreviousCamera = float4{ mCamera->GetPosition(), 1.0f };
                mFogFrame.mRange.y = distance;
                ++mFogFrame.mFrame.x;
                mFogFrame.mFrame.y = mCamera->IsOrthographic();
                mHistoryValid = mCount != 0;
            } );

        RegisterFogLighting( graph );
        RegisterFogIntegration( graph );
        RegisterFogComposite( graph );
    }

    auto VolumeModule::RegisterFogLighting( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Fog_Lighting" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "FogLighting_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "Fog_Lighting",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &fog{ b.Get<FogModuleInfo>() };
                if ( const auto *clouds{ b.TryGet<CloudsModuleInfo>() } ) {
                    builder.UseResource( clouds->mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( clouds->mModels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    for ( const auto &volume: clouds->mVolumes ) {
                        builder.UseResource( volume.mLighting, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                }

                builder.UseResource( b.Get<VolumeModuleInfo>().mData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mLightsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mClusterBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<ShadowMapInfo>().mRecordsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mIrradianceCubeRT, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fog.mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fog.mHistory, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fog.mExtinctionHistory, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fog.mLighting, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( fog.mExtinction, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                for ( const auto map: b.Get<ShadowMapInfo>().mMaps ) {
                    builder.UseResource( map, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
            },
            [this, pipeline]( CommandContext &context, Blackboard &b ) {
                if ( !mCamera || mCount == 0 ) {
                    return;
                }
                context.BindPipeline( pipeline );
                auto parameters{ MakeFogParameters( context, b, 0 ) };
                parameters.mAmbientScale = mAmbientScale;
                context.PushConstants( parameters );
                context.Dispatch( ( mFogFrame.mGrid.x + 3 ) / 4, ( mFogFrame.mGrid.y + 3 ) / 4, mFogFrame.mGrid.z / 4 );
            } );
    }

    auto VolumeModule::RegisterFogIntegration( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Fog_Integration" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "FogIntegrate_Comp.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "Fog_Integration",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &fog{ b.Get<FogModuleInfo>() };
                builder.UseResource( fog.mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fog.mLighting, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fog.mExtinction, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( fog.mRadiance, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( fog.mTransmittance, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &context, Blackboard &b ) {
                if ( !mCamera || mCount == 0 ) {
                    return;
                }
                context.BindPipeline( pipeline );
                context.PushConstants( MakeFogParameters( context, b, 1 ) );
                context.Dispatch( ( mFogFrame.mGrid.x + 7 ) / 8, ( mFogFrame.mGrid.y + 7 ) / 8, 1 );
            } );

        graph.RegisterPass(
            "Fog_History",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &fog{ b.Get<FogModuleInfo>() };
                builder.UseResource( fog.mLighting, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( fog.mExtinction, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( fog.mHistory, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( fog.mExtinctionHistory, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, Blackboard &b ) {
                if ( !mCamera || mCount == 0 ) {
                    return;
                }
                const auto &fog{ b.Get<FogModuleInfo>() };
                const TextureSlice slice{
                    .mWidth = mFogFrame.mGrid.x * mFogFrame.mGrid.w,
                    .mHeight = mFogFrame.mGrid.y * mFogFrame.mGrid.w,
                    .mDepth = 1
                };
                context.Copy( fog.mLighting, slice, fog.mHistory, slice );
                context.Copy( fog.mExtinction, slice, fog.mExtinctionHistory, slice );
            } );
    }

    auto VolumeModule::RegisterFogComposite( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Fog_Composite" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "FogComposite_Frag.slang", FGStageType::ePixel ) ) };

        graph.RegisterPass(
            "Fog_SceneCopy",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( b.Get<FogModuleInfo>().mSceneCopy, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, Blackboard &b ) {
                if ( !mCamera || mCount == 0 ) {
                    return;
                }
                const auto [width, height]{ InferDimensions( mResolution ) };
                const TextureSlice slice{ .mWidth = as<u32>( width ), .mHeight = as<u32>( height ), .mDepth = 1 };
                context.Copy( b.Get<GeomShadingModuleInfo>().mColorImage, slice, b.Get<FogModuleInfo>().mSceneCopy, slice );
            } );

        graph.RegisterPass(
            "Fog_Composite",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &fog{ b.Get<FogModuleInfo>() };
                builder.UseResource( fog.mFrame, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                for ( const auto texture: { fog.mSceneCopy, fog.mRadiance, fog.mTransmittance } ) {
                    builder.UseResource( texture, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                }
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &context, Blackboard &b ) {
                if ( !mCamera || mCount == 0 ) {
                    return;
                }
                const auto [width, height]{ InferDimensions( mResolution ) };
                context.PushConstants( MakeFogParameters( context, b, 2 ) );
                context.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( Rect{ as<i32>( width ), as<i32>( height ) } )
                    .AddRenderTarget( b.Get<GeomShadingModuleInfo>().mColorImage, Color{}, LoadOp::eClear ) );
                context.SetViewportState( ViewportState{}.AddViewportAndScissorRect( Viewport{ width, height } ) );
                context.BindPipeline( pipeline );
                context.Draw( 3 );
                context.EndRender();
            } );
    }

    auto VolumeModule::RegisterFogMultisample( FrameGraph &graph, Multisampling samples ) -> void {
        // Seed all samples with the fogged opaque result before blending transparent
        // surfaces. Their shader evaluates fog at each surface's own depth.
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Fog_Multisample" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetMultisampling( samples )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "FullQuad_Vert.slang", FGStageType::eVertex )
            .PushShader( "FogComposite_Frag.slang", FGStageType::ePixel ) ) };
        graph.RegisterPass(
            "Fog_Multisample",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &shading{ b.Get<GeomShadingModuleInfo>() };
                builder.UseResource( shading.mColorImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( shading.mRasterColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &context, Blackboard &b ) {
                const auto [width, height]{ InferDimensions( mResolution ) };
                context.PushConstants( MakeFogParameters( context, b, 3 ) );
                context.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( Rect{ as<i32>( width ), as<i32>( height ) } )
                    .AddRenderTarget( b.Get<GeomShadingModuleInfo>().mRasterColorImage, Color{}, LoadOp::eClear ) );
                context.SetViewportState( ViewportState{}.AddViewportAndScissorRect( Viewport{ width, height } ) );
                context.BindPipeline( pipeline );
                context.Draw( 3 );
                context.EndRender();
            } );
    }
}// namespace mikoto::renderer
