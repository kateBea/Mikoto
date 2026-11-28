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
#include <limits>

#include <EASTL/sort.h>

#include <Core/Profiler.hh>
#include <Core/String.hh>

#include <Material/VolumeMaterial.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/CloudsModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/TerrainGenerationModule.hh>
#include <Renderer/ForwardRenderer/Passes/VolumeModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    struct CloudsParameters {
        SPointer mFrame{};
        SPointer mModels{};
        SPointer mCamera{};
        core::u32 mOutput{};
        core::u32 mMetadata{};
        core::u32 mInput{};
        core::u32 mInputMetadata{};
        core::u32 mHistory{};
        core::u32 mHistoryMetadata{};
        core::u32 mDepth{};
        core::u32 mVolume{};
    };

    static auto CloudParameters( CommandContext &ctx, Blackboard &blackboard ) -> CloudsParameters {
        const auto &clouds{ blackboard.Get<CloudsModuleInfo>() };
        return {
            .mFrame = ctx.GetDeviceBufferAddress( clouds.mFrame ),
            .mModels = ctx.GetDeviceBufferAddress( clouds.mModels ),
            .mCamera = ctx.GetDeviceBufferAddress( blackboard.Get<CameraModuleInfo>().mCameraData ),
        };
    }

    CloudsModule::CloudsModule( RenderResolution resolution ) : mResolution{ resolution } {
    }

    auto CloudsModule::SetSettings( const CloudsSettings &settings ) -> void {
        if ( mSettings != settings ) {
            mHistoryValid = false;
            mResetLighting = true;
        }
        if ( mSettings.mDistanceFieldEnabled != settings.mDistanceFieldEnabled ) {
            for ( auto &&dirty: mDistanceDirty ) {
                dirty = true;
            }
        }
        mSettings = settings;
    }

    auto CloudsModule::GetSettings() const -> const CloudsSettings & {
        return mSettings;
    }

    auto CloudsModule::Prepare( const scene::Scene *scene, const scene::Camera *camera,
        const AtmosphereData *atmosphere, bool aerialPerspective ) -> void {
        MKT_BEGIN_PROFILER();

        const auto hashBytes{ []( const void *source, usize count ) -> u64 {
            const auto *bytes{ static_cast<const ubyte *>( source ) };
            u64 result{ 1469598103934665603ull };
            for ( usize index{}; index < count; ++index ) {
                result = ( result ^ bytes[index] ) * 1099511628211ull;
            }
            return result;
        } };

        mHistoryValid &= mScene == scene && mCamera == camera && scene && camera;
        mScene = scene;
        mCamera = camera;
        mRecords.clear();
        mLocalLights.clear();
        mInteractions.clear();
        mBolts.clear();
        mFrame.mLightingControl = {};
        if ( !scene || !camera || !mSettings.mEnabled ) {
            mFrame.mControl.x = 0;
            mHistoryValid = false;
            return;
        }

        const auto previousInverse{ mFrame.mInverseViewProjection };
        const auto previousViewDirection{ mViewDirection };
        const auto previousCamera{ mFrame.mCamera };
        const auto previousTime{ mFrame.mClock.x };
        const auto previousSun{ mFrame.mSunDirection };
        const auto previousSunColor{ mFrame.mSunColor };
        const float time{ as<float>( scene->GetVolumeTime() ) };
        const auto &registry{ scene->GetRegistry() };
        eastl::vector<entt::entity> entities{};

        for ( const auto entity: registry.view<scene::TagComponent, scene::TransformComponent, scene::CloudComponent>() ) {
            const auto &tag{ registry.get<scene::TagComponent>( entity ) };
            const auto &cloud{ registry.get<scene::CloudComponent>( entity ) };
            if ( tag.IsActive() && cloud.GetMaterial() ) {
                entities.push_back( entity );
            }
        }
        eastl::sort( entities.begin(), entities.end(), [&]( auto a, auto b ) {
            return registry.get<scene::TagComponent>( a ).GetGuid() < registry.get<scene::TagComponent>( b ).GetGuid();
        } );

        struct SourceGrid {
            const eastl::vector<f32> *mValues{};
            const eastl::vector<CloudVoxel> *mVoxels{};
        };
        eastl::vector<SourceGrid> grids{};
        float3 minimum{ std::numeric_limits<float>::max() };
        float3 maximum{ -std::numeric_limits<float>::max() };
        u64 historyHash{ 1469598103934665603ull };
        u64 gridDataHash{ 1469598103934665603ull };
        u64 terrainHash{ 1469598103934665603ull };
        for ( const auto [entity, terrain, tag, transform]: registry.view<scene::LandscapeComponent, scene::TagComponent, scene::TransformComponent>().each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }
            const auto revision{ terrain.GetRevision() };
            const auto world{ transform.GetWorldTransform() };
            terrainHash = ( terrainHash ^ hashBytes( &revision, sizeof( revision ) ) ^ hashBytes( &world, sizeof( world ) ) ) * 1099511628211ull;
        }
        mResetLighting |= terrainHash != mTerrainHash;
        historyHash ^= terrainHash;
        mTerrainHash = terrainHash;

        for ( const auto entity: entities ) {
            if ( mRecords.size() >= mCapacity ) {
                break;
            }
            const auto &cloud{ registry.get<scene::CloudComponent>( entity ) };
            const auto &tag{ registry.get<scene::TagComponent>( entity ) };
            const auto material{ cloud.GetMaterial() };
            const auto &settings{ material->GetSettings() };
            auto world{ registry.get<scene::TransformComponent>( entity ).GetWorldTransform() };
            auto dimensions{ cloud.GetDimensions() };
            if ( cloud.GetShape() == scene::CloudShape::eSkyLayer ) {
                world = glm::translate( float4x4{ 1 }, float3{ world[3].x,
                                                           cloud.GetBottomAltitude() + cloud.GetLayerThickness() * 0.5f, world[3].z } );
                dimensions = { cloud.GetHorizonRadius() * 2, cloud.GetLayerThickness(), cloud.GetHorizonRadius() * 2 };
            }
            world *= glm::scale( float4x4{ 1 }, dimensions );
            const auto determinant{ glm::determinant( world ) };
            if ( glm::abs( determinant ) < 1e-12f ) {
                continue;
            }

            // Authored transforms invalidate history. Continuous wind translation
            // uses reprojection and never regenerates the local voxel field.
            historyHash = ( historyHash ^ hashBytes( &world, sizeof( world ) ) ) * 1099511628211ull;
            const auto direction{ glm::length( cloud.GetWindDirection() ) > 0 ? glm::normalize( cloud.GetWindDirection() ) : float2{ 0 } };
            const float3 velocity{ direction.x * cloud.GetWindSpeed(), 0, direction.y * cloud.GetWindSpeed() };
            world[3] += float4{ velocity * time, 0 };

            CloudRecord record{};
            record.mWorldToLocal = glm::inverse( world );
            record.mLocalToWorld = world;
            const float3 extent{ glm::length( float3{ world[0] } ), glm::length( float3{ world[1] } ), glm::length( float3{ world[2] } ) };
            const auto metricInverse{ glm::scale( float4x4{ 1 }, extent ) * record.mWorldToLocal };
            const float inverseScale{ glm::length( float3{ metricInverse[0] } ) +
                                      glm::length( float3{ metricInverse[1] } ) + glm::length( float3{ metricInverse[2] } ) };
            const float distanceScale{ glm::min( extent.x, glm::min( extent.y, extent.z ) ) / inverseScale };
            record.mExtinction = float4{ settings.mAbsorption + settings.mScattering, 0 };
            record.mScatteringPhase = float4{ settings.mScattering, settings.mAnisotropy };
            record.mDensityDetail = { settings.mDensity * cloud.GetDensityMultiplier(),
                settings.mNoiseScale, settings.mErosionScale, settings.mErosion };
            record.mFormation = { settings.mCloudFormationCount, settings.mCloudFormationScale, settings.mCloudFormationVariation, 0 };
            record.mShape = { settings.mCoverage, settings.mCloudDetailType, distanceScale, settings.mCloudProfileDepth };
            record.mWind = float4{ velocity, 0 };
            record.mWeather = { settings.mWeatherScale, settings.mCoverageVariation,
                settings.mCloudTypeVariation, settings.mCloudHeightVariation };
            const auto localShear{ float3{ record.mWorldToLocal * float4{ direction.x, 0, direction.y, 0 } } };
            // Shape coordinates are measured in metres, so remove the inverse
            // scale before using wind to shear a non-uniformly sized volume.
            const float2 metricShear{ localShear.x * glm::length( float3{ world[0] } ),
                localShear.z * glm::length( float3{ world[2] } ) };
            const auto shearLength{ glm::length( metricShear ) };
            const auto shearAxis{ shearLength > 0 ? metricShear / shearLength : float2{ 1, 0 } };
            record.mProfile = { as<f32>( settings.mCloudForm ), settings.mWindShear, shearAxis.x, shearAxis.y };
            record.mFlags = { as<u32>( cloud.GetShape() ), as<u32>( settings.mDensityModel ),
                settings.mCloudSeed != 0 ? settings.mCloudSeed : as<u32>( tag.GetGuid() ),
                ( cloud.IsShadowCaster() ? 1u : 0u ) | ( cloud.IsShadowReceiver() ? 2u : 0u ) |
                    ( material->GetCloudGrid().empty() ? 0u : 4u ) };
            record.mGrid = uint4{ material->GetGridDimensions(), 0 };

            const auto flash{ cloud.GetLightningState( scene->GetVolumeTime(), registry.get<scene::TransformComponent>( entity ).GetWorldTransform() ) };
            PrepareBolt( cloud.GetLightning(), flash );
            record.mLightningPositionRadius = flash.mPositionRadius;
            record.mLightningColorIntensity = flash.mColorIntensity;
            // Flashes invalidate temporal radiance, but never rebuild density/SDF.
            historyHash ^= hashBytes( &flash, sizeof( flash ) );

            auto modelKey{ record };
            modelKey.mWorldToLocal = float4x4{ 1 };
            // Procedural density is authored at metric frequencies. Changing
            // scale rebuilds the field; rigid placement and wind do not.
            modelKey.mLocalToWorld = glm::scale( float4x4{ 1 }, float3{
                                                                    glm::length( float3{ world[0] } ), glm::length( float3{ world[1] } ), glm::length( float3{ world[2] } ) } );
            modelKey.mWind = {};
            modelKey.mLightningPositionRadius = {};
            modelKey.mLightningColorIntensity = {};
            modelKey.mShape.z = 0;
            u64 modelHash{ hashBytes( &modelKey, sizeof( modelKey ) ) };
            const auto &grid{ material->GetDensityGrid() };
            const auto &voxels{ material->GetCloudGrid() };
            // Content hashes are computed once by material grid setters.
            // Wind, camera motion and lightning never rescan megabytes of voxels.
            modelHash ^= material->GetGridHash();
            gridDataHash = ( gridDataHash ^ hashBytes( &record.mGrid, sizeof( record.mGrid ) ) ^
                               material->GetGridHash() ^ ( voxels.empty() ? 0ull : 4ull ) ) *
                           1099511628211ull;
            const auto index{ mRecords.size() };
            mModelDirty[index] = mModelDirty[index] || mFieldHashes[index] != modelHash;
            mDistanceDirty[index] = mDistanceDirty[index] || mModelDirty[index];
            mResetLighting |= mModelDirty[index];
            mFieldHashes[index] = modelHash;
            historyHash = ( historyHash ^ modelHash ) * 1099511628211ull;
            historyHash ^= hashBytes( &velocity, sizeof( velocity ) );

            for ( u32 corner{}; corner < 8; ++corner ) {
                const float3 local{ ( corner & 1 ) ? .5f : -.5f, ( corner & 2 ) ? .5f : -.5f, ( corner & 4 ) ? .5f : -.5f };
                const float3 point{ world * float4{ local, 1 } };
                minimum = glm::min( minimum, point );
                maximum = glm::max( maximum, point );
            }
            mRecords.push_back( record );
            grids.push_back( { &grid, &voxels } );
        }

        const bool layoutChanged{ gridDataHash != mGridDataHash || mGridOffsets.size() != mRecords.size() };
        if ( layoutChanged || mModelData.empty() ) {
            mSourceDataDirty = true;
            mGridDataHash = gridDataHash;
            mGridOffsets.assign( mRecords.size(), 0 );
            mModelData.assign( 16 + mRecords.size() * sizeof( CloudRecord ), 0 );
            for ( usize index{}; index < mRecords.size(); ++index ) {
                const auto &values{ *grids[index].mValues };
                const auto &voxels{ *grids[index].mVoxels };
                const usize bytes{ voxels.empty() ? values.size() * sizeof( f32 ) : voxels.size() * sizeof( CloudVoxel ) };
                if ( bytes == 0 ) {
                    continue;
                }
                const auto offset{ mModelData.size() };
                mGridOffsets[index] = as<u32>( offset );
                mModelData.resize( offset + bytes );
                std::memcpy( mModelData.data() + offset, voxels.empty() ? static_cast<const void *>( values.data() ) : static_cast<const void *>( voxels.data() ), bytes );
            }
        }
        for ( usize index{}; index < mRecords.size(); ++index ) {
            mRecords[index].mGrid.w = mGridOffsets[index];
        }

        mFrame.mPreviousInverseViewProjection = previousInverse;
        mFrame.mPreviousViewProjection = mViewProjection;
        mViewProjection = camera->GetProjection() * camera->GetViewMatrix();
        mFrame.mPreviousCamera = previousCamera;
        mFrame.mInverseViewProjection = glm::inverse( mViewProjection );
        mFrame.mCamera = float4{ camera->GetPosition(), camera->IsOrthographic() ? 1.0f : 0.0f };
        mFrame.mBoundsMin = float4{ mRecords.empty() ? float3{ 0 } : minimum, 0 };
        mFrame.mBoundsMax = float4{ mRecords.empty() ? float3{ 0 } : maximum, 0 };
        mFrame.mSunDirection = atmosphere ? atmosphere->mSunDirection : float4{ glm::normalize( float3{ .4f, .8f, .2f } ), 0 };
        mFrame.mSunColor = atmosphere ? atmosphere->mSunIlluminance : float4{ 6, 5.7f, 5.3f, 1 };
        if ( !atmosphere ) {
            for ( const auto [entity, tag, transform, light]: registry.view<scene::TagComponent, scene::TransformComponent, scene::LightComponent>().each() ) {
                if ( tag.IsActive() && light.GetActiveType() == LightType::eDirectional ) {
                    const auto &sun{ light.Get<DirectionalLight>() };
                    mFrame.mSunDirection = float4{ -sun.GetWorldDirection( transform.GetWorldTransform() ), 0 };
                    mFrame.mSunColor = float4{ sun.GetColor() * sun.GetIntensity(), 1 };
                    break;
                }
            }
        }
        mResetLighting |= mFrame.mSunDirection != previousSun || mFrame.mSunColor != previousSunColor;
        mFrame.mAmbientColor = { .40f, .55f, .85f, mSettings.mAmbientIntensity };
        mFrame.mMarch = { mSettings.mStepLength, mSettings.mMaxDistance, mSettings.mHistoryWeight, mSettings.mMultipleScattering };
        const float elapsed{ glm::max( 0.0f, time - previousTime ) };
        mFrame.mClock = { time, elapsed, mSettings.mDetailStrength, .005f };
        mFrame.mDetail = { mSettings.mEdgeDarkening, mSettings.mInnerGlow, mSettings.mDetailMipBias, mSettings.mAnimatedJitterDistance };
        mFrame.mControl = { as<u32>( mRecords.size() ), mFrame.mControl.y + 1, 0, as<u32>( mSettings.mDebugView ) };
        mFrame.mResolution.w = mSettings.mSunSteps;
        mFrame.mFiltering = { mSettings.mMaxSteps, mResetLighting ? 1u : 0u, atmosphere ? 1u : 0u, atmosphere && aerialPerspective ? 1u : 0u };

        // Reject camera cuts as well as teleports; normal camera motion keeps
        // the wind-aware reprojected history.
        const auto &view{ camera->GetViewMatrix() };
        mViewDirection = glm::normalize( float3{ -view[0].z, -view[1].z, -view[2].z } );
        const bool cameraCut{ glm::dot( previousViewDirection, mViewDirection ) < .8f };
        const float movement{ glm::length( float3{ mFrame.mCamera - previousCamera } ) };
        mHistoryValid &= historyHash == mHistoryHash && elapsed < 0.25f && movement < 200.0f && !cameraCut;
        mFrame.mControl.z = mHistoryValid && !mRecords.empty() ? 1u : 0u;
        mHistoryHash = historyHash;
        mHistoryValid = !mRecords.empty();
        PrepareLocalLighting();
        PrepareInteractions();
    }

    auto CloudsModule::RegisterResources( FrameGraph &graph, u32 volumeCapacity ) -> void {
        mCapacity = glm::clamp( volumeCapacity, 1u, kMaxCloudVolumes );
        mGridDataHash = 0;
        mSourceDataDirty = true;
        mFieldHashes.assign( mCapacity, 0 );
        mModelDirty.assign( mCapacity, true );
        mDistanceDirty.assign( mCapacity, true );
        mNoiseDirty = true;
        mHistoryValid = false;
        mResetLighting = true;

        auto &info{ graph.GetOrCreate<CloudsModuleInfo>() };
        info.mFrame = graph.Create( FGBufferDescription{}
            .SetName( "Clouds_Frame" )
            .SetSizeBytes( sizeof( CloudsFrameData ) )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest ) );
        info.mInteractions = graph.Create( FGBufferDescription{}
            .SetName( "Clouds_Interactions" )
            .SetSizeBytes( 64 * sizeof( CloudInteractionRecord ) )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest ) );
        info.mModels = graph.Create( FGBufferDescription{}
            .SetName( "Clouds_Models" )
            .SetSizeBytes( 16 + mCapacity * sizeof( CloudRecord ) )
            .SetHeapType( HeapType::eDeviceLocal )
            .SetUsage( BufferUsageFlagsBits::Structured | BufferUsageFlagsBits::CopyDest ) );
        info.mClampSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Clouds_ClampLinear" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToEdge ) );
        info.mRepeatSampler = graph.Create( FGSamplerDescription{}
            .SetName( "Clouds_RepeatLinear" )
            .SetFilter( SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eRepeat ) );

        const auto fieldSize{ mSettings.mFieldResolution };
        mFrame.mResolution = { fieldSize, fieldSize / 2, fieldSize, mSettings.mSunSteps };
        const auto usage{ TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess };
        const auto createVolume{ [&]( core::cstr name, u32 width, u32 height, u32 depth ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( width )
                .SetHeight( height )
                .SetDepth( depth )
                .SetDimensions( TextureDimension::eTexture3D )
                .SetFormat( Format::eRGBA16_FLOAT )
                .SetUsage( usage ) );
        } };
        info.mNoise = createVolume( "Clouds_DetailNoise", 128, 128, 128 );
        info.mBaseShapeNoise = createVolume( "Clouds_BaseShapeNoise", 128, 128, 128 );
        info.mWeatherMap = graph.Create( FGTextureDescription{}
            .SetName( "Clouds_WeatherMap" )
            .SetWidth( 512 )
            .SetHeight( 512 )
            .SetFormat( Format::eRGBA16_FLOAT )
            .SetUsage( usage ) );
        for ( u32 mip{}; mip < info.mNoiseMips.size(); ++mip ) {
            const auto size{ 64u >> mip };
            info.mNoiseMips[mip] = createVolume( string::Format( "Clouds_NoiseMip{}", mip ).c_str(), size, size, size );
        }
        const usize voxelCount{ usize( fieldSize ) * ( fieldSize / 2 ) * fieldSize };
        const auto createDistanceBuffer{ [&]( core::cstr name, usize bytes ) {
            return graph.Create( FGBufferDescription{}
                .SetName( name )
                .SetSizeBytes( bytes )
                .SetHeapType( HeapType::eDeviceLocal )
                .SetUsage( BufferUsageFlagsBits::Structured ) );
        } };
        info.mDistanceFields = createDistanceBuffer( "Clouds_PackedDistances", voxelCount * mCapacity );
        info.mDistanceScratchA = createDistanceBuffer( "Clouds_DistanceScratchA", voxelCount * sizeof( float2 ) );
        info.mDistanceScratchB = createDistanceBuffer( "Clouds_DistanceScratchB", voxelCount * sizeof( float2 ) );
        for ( u32 index{}; index < mCapacity; ++index ) {
            const auto name{ string::Format( "Clouds_Volume{}", index ) };
            info.mVolumes.push_back( { createVolume( ( name + "_NVDF" ).c_str(), fieldSize, fieldSize / 2, fieldSize ),
                createVolume( ( name + "_Lighting" ).c_str(), fieldSize / 2, fieldSize / 4, fieldSize / 2 ),
                createVolume( ( name + "_LocalLighting" ).c_str(), fieldSize / 4, fieldSize / 8, fieldSize / 4 ) } );
        }

        const auto dimensions{ InferDimensions( mResolution ) };
        const auto width{ as<u32>( dimensions.first ) };
        const auto height{ as<u32>( dimensions.second ) };
        const u32 traceWidth{ mSettings.mHalfResolution ? ( width + 1 ) / 2 : width };
        const u32 traceHeight{ mSettings.mHalfResolution ? ( height + 1 ) / 2 : height };
        mFrame.mScreen = float4{ width, height, traceWidth, traceHeight };
        const auto createView{ [&]( core::cstr name, Format format ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( traceWidth )
                .SetHeight( traceHeight )
                .SetFormat( format )
                .SetUsage( usage | TextureUsageFlagsBits::CopySource | TextureUsageFlagsBits::CopyDest ) );
        } };
        // Alpha stores metric distance, so retain float32 precision in the view/history textures.
        info.mRawRadiance = createView( "Clouds_RawRadiance", Format::eRGBA32_FLOAT );
        info.mRawTransmittance = createView( "Clouds_RawTransmittance", Format::eRGBA32_FLOAT );
        info.mRadiance = createView( "Clouds_FilteredRadiance", Format::eRGBA32_FLOAT );
        info.mTransmittance = createView( "Clouds_FilteredTransmittance", Format::eRGBA32_FLOAT );
        info.mRadianceHistory = createView( "Clouds_RadianceHistory", Format::eRGBA32_FLOAT );
        info.mTransmittanceHistory = createView( "Clouds_TransmittanceHistory", Format::eRGBA32_FLOAT );
        info.mSceneCopy = graph.Create( FGTextureDescription{}
            .SetName( "Clouds_SceneCopy" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( Format::eRGBA16_FLOAT )
            .SetUsage( TextureUsageFlagsBits::CopyDest | TextureUsageFlagsBits::ShaderResource ) );
        info.mComposite = graph.Create( FGTextureDescription{}
            .SetName( "Clouds_Composite" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( Format::eRGBA16_FLOAT )
            .SetUsage( TextureUsageFlagsBits::CopySource | TextureUsageFlagsBits::UnorderedAccess ) );

        // Storage growth must precede the upload pass's native barriers.
        graph.RegisterPass(
            "Pass_Clouds_Prepare",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<CloudsModuleInfo>().mModels, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                mSourceDataDirty |= ctx.EnsureBufferCapacity( b.Get<CloudsModuleInfo>().mModels, glm::max<usize>( mModelData.size(), 16 ) );
            } );

        graph.RegisterPass(
            "Pass_Clouds_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                const auto &terrain{ b.Get<TerrainGenerationInfo>() };
                // These callbacks capture backing addresses after terrain growth.
                for ( const auto handle: { terrain.mVertices, terrain.mSamplingRecords, terrain.mHeightBounds } ) {
                    builder.UseResource( handle, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                }
                builder.UseResource( clouds.mFrame, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( clouds.mModels, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( clouds.mInteractions, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                auto &clouds{ b.Get<CloudsModuleInfo>() };
                mFrame.mModels = ctx.GetDeviceBufferAddress( clouds.mModels );
                mFrame.mInteractions = ctx.GetDeviceBufferAddress( clouds.mInteractions );
                mFrame.mLocalLights = ctx.GetDeviceBufferAddress( clouds.mLocalLights );
                mFrame.mMeshShadows = ctx.GetDeviceBufferAddress( clouds.mMeshShadows );
                mFrame.mBolts = ctx.GetDeviceBufferAddress( clouds.mBolts );
                const auto &terrain{ b.Get<TerrainGenerationInfo>() };
                mFrame.mTerrainVertices = ctx.GetDeviceBufferAddress( terrain.mVertices );
                mFrame.mTerrainRecords = ctx.GetDeviceBufferAddress( terrain.mSamplingRecords );
                mFrame.mTerrainBounds = ctx.GetDeviceBufferAddress( terrain.mHeightBounds );
                mFrame.mShapeResources = { ctx.PushSRV( clouds.mBaseShapeNoise ),
                    ctx.PushSRV( clouds.mWeatherMap ), 0, 0 };
                mFrame.mDistanceFields = ctx.GetDeviceBufferAddress( clouds.mDistanceFields );
                for ( u32 mip{}; mip < clouds.mNoiseMips.size(); ++mip ) {
                    mFrame.mNoiseMips[mip] = ctx.PushSRV( clouds.mNoiseMips[mip] );
                }
                mFrame.mResources = { ctx.PushSRV( clouds.mNoise ), ctx.PushSampler( clouds.mClampSampler ),
                    ctx.PushSampler( clouds.mRepeatSampler ), mSettings.mCastSurfaceShadows ? 1u : 0u };
                mFrame.mAtmosphere = 0;
                if ( mFrame.mFiltering.z ) {
                    const auto &atmosphere{ b.Get<AtmosphereModuleInfo>() };
                    mFrame.mAtmosphere = ctx.GetDeviceBufferAddress( atmosphere.mParameters );
                    mFrame.mAtmosphereTextures = { ctx.PushSRV( atmosphere.mTransmittance ), ctx.PushSRV( atmosphere.mAerialRadiance ),
                        ctx.PushSRV( atmosphere.mAerialTransmittance ), ctx.PushSampler( atmosphere.mSampler ) };
                }
                mFrame.mFogFrame = 0;
                if ( const auto *fog{ b.TryGet<FogModuleInfo>() } ) {
                    mFrame.mFogFrame = ctx.GetDeviceBufferAddress( fog->mFrame );
                    mFrame.mFogTextures = { ctx.PushSRV( fog->mRadiance ), ctx.PushSRV( fog->mTransmittance ),
                        ctx.PushSampler( fog->mSampler ), 0 };
                }
                for ( usize index{}; index < mRecords.size(); ++index ) {
                    mFrame.mLocalTextures[index / 4][index % 4] = ctx.PushSRV( clouds.mVolumes[index].mLocalLighting );
                    mRecords[index].mTextures = { ctx.PushSRV( clouds.mVolumes[index].mModel ),
                        ctx.PushSRV( clouds.mVolumes[index].mLighting ),
                        as<u32>( index * mFrame.mResolution.x * mFrame.mResolution.y * mFrame.mResolution.z ),
                        mSettings.mDistanceFieldEnabled ? 1u : 0u };
                    std::memcpy( mModelData.data() + 16 + index * sizeof( CloudRecord ), &mRecords[index], sizeof( CloudRecord ) );
                }
                if ( !mInteractions.empty() ) {
                    ctx.Copy( clouds.mInteractions, 0, mInteractions.data(), mInteractions.size() * sizeof( CloudInteractionRecord ) );
                }
                ctx.Copy( clouds.mFrame, 0, &mFrame, sizeof( mFrame ) );
                if ( !mModelData.empty() ) {
                    const usize bytes{ mSourceDataDirty ? mModelData.size() : 16 + mRecords.size() * sizeof( CloudRecord ) };
                    ctx.Copy( clouds.mModels, 0, mModelData.data(), bytes );
                    mSourceDataDirty = false;
                }
            } );
    }

    auto CloudsModule::RegisterModelPasses( FrameGraph &graph ) -> void {
        const auto noise{ graph.GetData<CloudsModuleInfo>().mNoise };
        const auto noisePipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_NoisePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsNoise_Comp.slang", FGStageType::eCompute ) ) };
        const auto baseShape{ graph.GetData<CloudsModuleInfo>().mBaseShapeNoise };
        const auto weather{ graph.GetData<CloudsModuleInfo>().mWeatherMap };
        const auto shapePipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_ShapeNoisePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsShapeNoise_Comp.slang", FGStageType::eCompute ) ) };
        const auto weatherPipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_WeatherPipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsWeather_Comp.slang", FGStageType::eCompute ) ) };
        graph.RegisterPass(
            "Pass_Clouds_Noise",
            FGPassType::eCompute,
            [noise, baseShape, weather]( FGNodeBuilder &builder, Blackboard & ) {
                for ( const auto output: { noise, baseShape, weather } ) {
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }
            },
            [this, noise, baseShape, weather, noisePipeline, shapePipeline, weatherPipeline]( CommandContext &ctx, Blackboard &b ) {
                if ( !mNoiseDirty ) {
                    return;
                }
                auto parameters{ CloudParameters( ctx, b ) };
                parameters.mOutput = ctx.PushUAV( noise );
                ctx.BindPipeline( noisePipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( 32, 32, 32 );

                parameters.mOutput = ctx.PushUAV( baseShape );
                ctx.BindPipeline( shapePipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( 32, 32, 32 );

                parameters.mOutput = ctx.PushUAV( weather );
                ctx.BindPipeline( weatherPipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( 64, 64, 1 );
            } );

        const auto mipPipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_NoiseMipPipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsNoiseMip_Comp.slang", FGStageType::eCompute ) ) };
        for ( u32 mip{}; mip < 4; ++mip ) {
            const auto input{ mip == 0 ? noise : graph.GetData<CloudsModuleInfo>().mNoiseMips[mip - 1] };
            const auto output{ graph.GetData<CloudsModuleInfo>().mNoiseMips[mip] };
            graph.RegisterPass(
                string::Format( "Pass_CloudNoiseMip_{}", mip ),
                FGPassType::eCompute,
                [input, output]( FGNodeBuilder &builder, Blackboard & ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                },
                [this, input, output, mipPipeline, mip]( CommandContext &ctx, Blackboard & ) {
                    if ( !mNoiseDirty ) {
                        return;
                    }
                    CloudsParameters parameters{};
                    parameters.mInput = ctx.PushSRV( input );
                    parameters.mOutput = ctx.PushUAV( output );
                    ctx.BindPipeline( mipPipeline );
                    ctx.PushConstants( parameters );
                    const auto groups{ glm::max( ( 64u >> mip ) / 4, 1u ) };
                    ctx.Dispatch( groups, groups, groups );
                    if ( mip == 3 ) {
                        mNoiseDirty = false;
                    }
                } );
        }

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_ModelPipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsModel_Comp.slang", FGStageType::eCompute ) ) };
        for ( u32 index{}; index < mCapacity; ++index ) {
            const auto field{ graph.GetData<CloudsModuleInfo>().mVolumes[index].mModel };
            const auto name{ string::Format( "Pass_Clouds_Model_{}", index ) };
            graph.RegisterPass(
                name,
                FGPassType::eCompute,
                [field]( FGNodeBuilder &builder, Blackboard &b ) {
                    builder.UseResource( b.Get<CloudsModuleInfo>().mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<CloudsModuleInfo>().mModels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<CloudsModuleInfo>().mNoise, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<CloudsModuleInfo>().mBaseShapeNoise, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( b.Get<CloudsModuleInfo>().mWeatherMap, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( field, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                },
                [this, pipeline, index, field]( CommandContext &ctx, Blackboard &b ) {
                    if ( index >= mRecords.size() || !mModelDirty[index] ) {
                        return;
                    }
                    auto parameters{ CloudParameters( ctx, b ) };
                    parameters.mVolume = index;
                    parameters.mOutput = ctx.PushUAV( field );
                    ctx.BindPipeline( pipeline );
                    ctx.PushConstants( parameters );
                    ctx.Dispatch( mFrame.mResolution.x / 4, mFrame.mResolution.y / 4, mFrame.mResolution.z / 4 );
                    mModelDirty[index] = false;
                } );
        }
        RegisterDistancePasses( graph );
    }

    // Distance/profile rebuilds run after model changes even when skip acceleration
    // is disabled. Two shared float2 scratch grids total 128 MiB at 256x128x256;
    // only conservative, rounded-down one-byte skip distances persist per volume.
    // A voxel support guard makes skips safe. This is not block-compressed sparse
    // Nubis storage; authored dimensional profiles must survive the acceleration bake.
    auto CloudsModule::RegisterDistancePasses( FrameGraph &graph ) -> void {
        struct DistanceParameters {
            SPointer mFrame{};
            SPointer mInput{};
            SPointer mOutput{};
            u32 mVolume{};
            u32 mAxis{};
            u32 mModelOutput{};
        };

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_DistancePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsDistance_Comp.slang", FGStageType::eCompute ) ) };

        // The two scratch buffers are shared by all volumes. Graph dependencies
        // serialize their bakes; only the one-byte distance fields persist.
        for ( u32 index{}; index < mCapacity; ++index ) {
            for ( u32 axis{}; axis < 4; ++axis ) {
                const auto &clouds{ graph.GetData<CloudsModuleInfo>() };
                const auto input{ axis == 1 ? clouds.mDistanceScratchA : axis == 2 ? clouds.mDistanceScratchB
                                                                                   : clouds.mDistanceScratchA };
                const auto output{ axis == 3 ? clouds.mDistanceFields : axis == 1 ? clouds.mDistanceScratchB
                                                                                  : clouds.mDistanceScratchA };
                const auto model{ clouds.mVolumes[index].mModel };

                graph.RegisterPass(
                    string::Format( "Pass_CloudDistance_{}_{}", index, axis ),
                    FGPassType::eCompute,
                    [input, output, model, axis]( FGNodeBuilder &builder, Blackboard &b ) {
                        const auto &clouds{ b.Get<CloudsModuleInfo>() };
                        builder.UseResource( clouds.mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        builder.UseResource( clouds.mInteractions, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        builder.UseResource( clouds.mModels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        if ( axis == 0 ) {
                            builder.UseResource( model, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        }
                        else {
                            builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                        }
                        if ( axis == 3 ) {
                            // Finalize profile/SDF through one UAV per voxel;
                            // unordered access retains both read and write access.
                            builder.UseResource( model, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                        }
                        // Packing a later cloud must preserve the earlier ranges.
                        builder.UseResource( output, FGPipelineStage::eComputeShader,
                            FGResourceAccess::eWrite );
                    },
                    [this, input, output, model, pipeline, index, axis]( CommandContext &ctx, Blackboard &b ) {
                        if ( index >= mRecords.size() || !mDistanceDirty[index] ) {
                            return;
                        }
                        const auto count{ mFrame.mResolution.x * mFrame.mResolution.y * mFrame.mResolution.z };
                        const DistanceParameters parameters{
                            .mFrame = ctx.GetDeviceBufferAddress( b.Get<CloudsModuleInfo>().mFrame ),
                            .mInput = axis == 0 ? 0 : ctx.GetDeviceBufferAddress( input ),
                            .mOutput = ctx.GetDeviceBufferAddress( output ),
                            .mVolume = index,
                            .mAxis = axis,
                            .mModelOutput = axis == 3 ? ctx.PushUAV( model ) : 0,
                        };
                        ctx.BindPipeline( pipeline );
                        ctx.PushConstants( parameters );
                        const auto groups{ axis == 3 ? ( count + 1023 ) / 1024 : count / mFrame.mResolution[axis] };
                        ctx.Dispatch( glm::min( groups, 16384u ), ( groups + 16383 ) / 16384, 1 );
                        if ( axis == 3 ) {
                            mDistanceDirty[index] = false;
                        }
                    } );
            }
        }
    }

    auto CloudsModule::RegisterLightingPasses( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_LightingPipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsLighting_Comp.slang", FGStageType::eCompute ) ) };
        for ( u32 index{}; index < mCapacity; ++index ) {
            const auto output{ graph.GetData<CloudsModuleInfo>().mVolumes[index].mLighting };
            const auto name{ string::Format( "Pass_Clouds_Lighting_{}", index ) };
            graph.RegisterPass(
                name,
                FGPassType::eCompute,
                [output]( FGNodeBuilder &builder, Blackboard &b ) {
                    const auto &clouds{ b.Get<CloudsModuleInfo>() };
                    const auto &terrain{ b.Get<TerrainGenerationInfo>() };
                    for ( const auto handle: { terrain.mVertices, terrain.mSamplingRecords, terrain.mHeightBounds } ) {
                        builder.UseResource( handle, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                    builder.UseResource( clouds.mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( clouds.mInteractions, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( clouds.mModels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( clouds.mNoise, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( clouds.mDistanceFields, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    for ( const auto mip: clouds.mNoiseMips ) {
                        builder.UseResource( mip, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                    for ( const auto &volume: clouds.mVolumes ) {
                        builder.UseResource( volume.mModel, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                    builder.UseResource( output, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                },
                [this, pipeline, output, index]( CommandContext &ctx, Blackboard &b ) {
                    if ( index >= mRecords.size() ) {
                        return;
                    }
                    auto parameters{ CloudParameters( ctx, b ) };
                    parameters.mVolume = index;
                    parameters.mOutput = ctx.PushUAV( output );
                    ctx.BindPipeline( pipeline );
                    ctx.PushConstants( parameters );
                    ctx.Dispatch( mFrame.mResolution.x / 8, mFrame.mResolution.y / 8, mFrame.mResolution.z / 8 );
                    mResetLighting = false;
                } );
        }
    }

    auto CloudsModule::RegisterViewPasses( FrameGraph &graph ) -> void {
        RegisterTrace( graph );
        RegisterTemporal( graph );
        RegisterHistory( graph );
        RegisterComposite( graph );
        RegisterBolts( graph );
    }

    auto CloudsModule::RegisterTrace( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_TracePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsTrace_Comp.slang", FGStageType::eCompute ) ) };
        graph.RegisterPass(
            "Pass_Clouds_Trace",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                builder.UseResource( clouds.mMeshShadows, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( u32 i{}; i < mCapacity; ++i ) {
                    builder.UseResource( clouds.mSunMaps[i], FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( clouds.mVolumes[i].mLocalLighting, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                builder.UseResource( clouds.mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mInteractions, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mModels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mNoise, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mDistanceFields, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( const auto mip: clouds.mNoiseMips ) {
                    builder.UseResource( mip, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<AtmosphereModuleInfo>().mParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<AtmosphereModuleInfo>().mTransmittance, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( const auto &volume: clouds.mVolumes ) {
                    builder.UseResource( volume.mModel, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( volume.mLighting, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                builder.UseResource( clouds.mRawRadiance, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( clouds.mRawTransmittance, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &ctx, Blackboard &b ) {
                auto parameters{ CloudParameters( ctx, b ) };
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                parameters.mDepth = ctx.PushSRV( b.Get<PrepassModuleInfo>().mPrepassDepthTarget );
                parameters.mOutput = ctx.PushUAV( clouds.mRawRadiance );
                parameters.mMetadata = ctx.PushUAV( clouds.mRawTransmittance );
                ctx.BindPipeline( pipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( ( as<u32>( mFrame.mScreen.z ) + 7 ) / 8, ( as<u32>( mFrame.mScreen.w ) + 7 ) / 8, 1 );
            } );
    }

    auto CloudsModule::RegisterTemporal( FrameGraph &graph ) -> void {
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_TemporalPipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsTemporal_Comp.slang", FGStageType::eCompute ) ) };
        graph.RegisterPass(
            "Pass_Clouds_Temporal",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mInteractions, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mModels, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( const auto input: { clouds.mRawRadiance, clouds.mRawTransmittance, clouds.mRadianceHistory, clouds.mTransmittanceHistory } ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                builder.UseResource( clouds.mRadiance, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( clouds.mTransmittance, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &ctx, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                auto parameters{ CloudParameters( ctx, b ) };
                parameters.mInput = ctx.PushSRV( clouds.mRawRadiance );
                parameters.mInputMetadata = ctx.PushSRV( clouds.mRawTransmittance );
                parameters.mHistory = ctx.PushSRV( clouds.mRadianceHistory );
                parameters.mHistoryMetadata = ctx.PushSRV( clouds.mTransmittanceHistory );
                parameters.mOutput = ctx.PushUAV( clouds.mRadiance );
                parameters.mMetadata = ctx.PushUAV( clouds.mTransmittance );
                ctx.BindPipeline( pipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( ( as<u32>( mFrame.mScreen.z ) + 7 ) / 8, ( as<u32>( mFrame.mScreen.w ) + 7 ) / 8, 1 );
            } );
    }

    auto CloudsModule::RegisterHistory( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "Pass_Clouds_History",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                for ( const auto input: { clouds.mRadiance, clouds.mTransmittance } ) {
                    builder.UseResource( input, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                }
                for ( const auto output: { clouds.mRadianceHistory, clouds.mTransmittanceHistory } ) {
                    builder.UseResource( output, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                const TextureSlice slice{ .mWidth = as<u32>( mFrame.mScreen.z ), .mHeight = as<u32>( mFrame.mScreen.w ) };
                ctx.Copy( clouds.mRadiance, slice, clouds.mRadianceHistory, slice );
                ctx.Copy( clouds.mTransmittance, slice, clouds.mTransmittanceHistory, slice );
            } );
    }

    auto CloudsModule::RegisterComposite( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "Pass_Clouds_SceneCopy",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CloudsModuleInfo>().mSceneCopy, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                const TextureSlice slice{ .mWidth = as<u32>( mFrame.mScreen.x ), .mHeight = as<u32>( mFrame.mScreen.y ) };
                ctx.Copy( b.Get<GeomShadingModuleInfo>().mColorImage, slice, b.Get<CloudsModuleInfo>().mSceneCopy, slice );
            } );
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Clouds_CompositePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "CloudsComposite_Comp.slang", FGStageType::eCompute ) ) };
        graph.RegisterPass(
            "Pass_Clouds_Composite",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                builder.UseResource( clouds.mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( clouds.mInteractions, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                for ( const auto input: { clouds.mSceneCopy, clouds.mRadiance, clouds.mTransmittance, b.Get<PrepassModuleInfo>().mPrepassDepthTarget,
                          b.Get<AtmosphereModuleInfo>().mAerialRadiance, b.Get<AtmosphereModuleInfo>().mAerialTransmittance } ) {
                    builder.UseResource( input, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                if ( const auto *fog{ b.TryGet<FogModuleInfo>() } ) {
                    builder.UseResource( fog->mFrame, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( fog->mRadiance, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    builder.UseResource( fog->mTransmittance, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                builder.UseResource( clouds.mComposite, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &ctx, Blackboard &b ) {
                const auto &clouds{ b.Get<CloudsModuleInfo>() };
                auto parameters{ CloudParameters( ctx, b ) };
                parameters.mInput = ctx.PushSRV( clouds.mSceneCopy );
                parameters.mHistory = ctx.PushSRV( clouds.mRadiance );
                parameters.mHistoryMetadata = ctx.PushSRV( clouds.mTransmittance );
                parameters.mDepth = ctx.PushSRV( b.Get<PrepassModuleInfo>().mPrepassDepthTarget );
                parameters.mOutput = ctx.PushUAV( clouds.mComposite );
                ctx.BindPipeline( pipeline );
                ctx.PushConstants( parameters );
                ctx.Dispatch( ( as<u32>( mFrame.mScreen.x ) + 7 ) / 8, ( as<u32>( mFrame.mScreen.y ) + 7 ) / 8, 1 );
            } );
        graph.RegisterPass(
            "Pass_Clouds_Publish",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<CloudsModuleInfo>().mComposite, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard &b ) {
                const TextureSlice slice{ .mWidth = as<u32>( mFrame.mScreen.x ), .mHeight = as<u32>( mFrame.mScreen.y ) };
                ctx.Copy( b.Get<CloudsModuleInfo>().mComposite, slice, b.Get<GeomShadingModuleInfo>().mColorImage, slice );
            } );
    }

}// namespace mikoto::renderer
