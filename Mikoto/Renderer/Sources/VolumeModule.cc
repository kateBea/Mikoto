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

#include <Renderer/Core/Volumes.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/VolumeModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

namespace mikoto::renderer {
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::material;
    using namespace mikoto::renderer::rhi;

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

            parameters.mLighting = context.PushTexture_UAV( fog.mLighting );
            parameters.mExtinction = context.PushTexture_UAV( fog.mExtinction );
            parameters.mHistory = context.PushTexture_SRV( fog.mHistory );
            parameters.mExtinctionHistory = context.PushTexture_SRV( fog.mExtinctionHistory );
            parameters.mIrradiance = context.PushTexture_SRV( blackboard.Get<GeomShadingModuleInfo>().mIrradianceCubeRT );

            parameters.mLightCount = prepass.mActiveLightCount;
            parameters.mShadowCount = shadows.mRecordCount;
            parameters.mLightGridXY = float2{ prepass.mGridSize };
            parameters.mLightGridZ = prepass.mGridSize.z;
            parameters.mClusterCount = prepass.mClusterCount;
        }
        else if ( stage == 1 ) {
            parameters.mLighting = context.PushTexture_SRV( fog.mLighting );
            parameters.mExtinction = context.PushTexture_SRV( fog.mExtinction );

            parameters.mRadiance = context.PushTexture_UAV( fog.mRadiance );
            parameters.mTransmittance = context.PushTexture_UAV( fog.mTransmittance );
        }
        else if ( stage == 2 ) {
            parameters.mCamera = context.GetDeviceBufferAddress( blackboard.Get<CameraModuleInfo>().mCameraData );
            parameters.mSource = context.PushTexture_SRV( fog.mSceneCopy );
            parameters.mDepth = context.PushTexture_SRV( prepass.mPrepassDepthTarget );

            parameters.mRadiance = context.PushTexture_SRV( fog.mRadiance );
            parameters.mTransmittance = context.PushTexture_SRV( fog.mTransmittance );
        }
        else {
            parameters.mSource = context.PushTexture_SRV( blackboard.Get<GeomShadingModuleInfo>().mColorImage );
        }

        return parameters;
    }

    static auto VolumeHash( const eastl::vector<ubyte> &data ) -> u64 {
        u64 hash{ 1469598103934665603ull };
        for ( const auto byte: data ) {
            hash = ( hash ^ byte ) * 1099511628211ull;
        }

        return hash;
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

        // Height falloff is otherwise unused for the procedural/grid density models.
        if ( s.mDensityModel == VolumeDensityModel::eExponentialHeight ) {
            result.mNoise.x = s.mHeightFalloff;
        }

        result.mGrid = uint4{ material.GetGridDimensions(), 0 };
        result.mExtinctionMajorant = float4{ properties.GetExtinction() * s.mDensity * multiplier, s.mDensity * multiplier };
        return result;
    }

    VolumeModule::VolumeModule( RenderResolution resolution ) : mResolution{ resolution } {
        mData.resize( 16, 0 );
    }

    auto VolumeModule::GetData() const -> const eastl::vector<core::ubyte>& {
        return mData;
    }

    auto VolumeModule::Prepare( const Scene *scene, const Camera *camera, f32 ambientScale ) -> bool {
        MKT_BEGIN_PROFILER();

        mHistoryValid &= mScene == scene && mCamera == camera && mAmbientScale == ambientScale;
        mScene = scene;
        mCamera = camera;
        mAmbientScale = ambientScale;

        eastl::vector<VolumeEntry> entries{};

        if ( scene ) {
            const auto &registry{ scene->GetRegistry() };
            for ( const auto [entity, tag, transform, source]: registry.view<TagComponent, TransformComponent, CloudComponent>().each() ) {
                if ( !tag.IsActive() || !source.GetMaterial() ) {
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
        mData.assign( 16 + entries.size() * sizeof( VolumeRecord ), 0 );
        std::memcpy( mData.data(), &mCount, sizeof( mCount ) );

        for ( usize i{}; i < entries.size(); ++i ) {
            auto &entry{ entries[i] };
            const auto &grid{ entry.mMaterial->GetDensityGrid() };
            if ( !grid.empty() ) {
                entry.mRecord.mGrid.w = as<u32>( mData.size() );
                const auto offset{ mData.size() };
                mData.resize( offset + grid.size() * sizeof( f32 ) );
                std::memcpy( mData.data() + offset, grid.data(), grid.size() * sizeof( f32 ) );
            }
            std::memcpy( mData.data() + 16 + i * sizeof( VolumeRecord ), &entry.mRecord, sizeof( VolumeRecord ) );
        }

        const u64 hash{ VolumeHash( mData ) };
        const bool changed{ hash != mHash };
        mHash = hash;
        mUploadDirty |= changed;
        mHistoryValid &= !changed && mCount != 0;

        return changed;
    }

    auto VolumeModule::RegisterUpload( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto &info{ graph.GetOrCreate<VolumeModuleInfo>() };
        info.mData = graph.Create( FGBufferDescription{}
            .SetName( "Volumes_Data" )
            .SetUsage( BufferUsageFlagsBits::Structured | 
                BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( 16 )
            .SetHeapType( HeapType::eDeviceLocal ) );

        graph.RegisterPass(
            "Volumes_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard &b ) {
                builder.UseResource( b.Get<VolumeModuleInfo>().mData, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                if ( const auto *fog{ b.TryGet<FogModuleInfo>() } ) {
                    builder.UseResource( fog->mFrame, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &context, Blackboard &b ) {
                if ( const auto *fog{ b.TryGet<FogModuleInfo>() }; fog && mCamera ) {
                    auto frame{ mFogFrame };
                    const f32 distance{ glm::max( mCamera->GetFarPlane(), 0.001f ) };
                    frame.mRange = float4{ 0.0f, distance, mFogFrame.mRange.y, mHistoryValid ? 0.9f : 0.0f };
                    frame.mTextures = uint4{ context.PushTexture_SRV( fog->mRadiance ), context.PushTexture_SRV( fog->mTransmittance ),
                                             context.PushSampler( fog->mSampler ), mCount };
                    context.CopyBuffer( fog->mFrame, 0, &frame, sizeof( frame ) );

                    mFogFrame.mPreviousViewProjection = mCamera->GetViewProjection();
                    mFogFrame.mPreviousInverseViewProjection = glm::inverse( mFogFrame.mPreviousViewProjection );
                    mFogFrame.mPreviousCamera = float4{ mCamera->GetPosition(), 1.0f };
                    mFogFrame.mRange.y = distance;
                    ++mFogFrame.mFrame.x;
                    mFogFrame.mFrame.y = mCamera->IsOrthographic();
                    mHistoryValid = mCount != 0;
                }
                
                if ( !mUploadDirty ) {
                    return;
                }

                const auto buffer{ b.Get<VolumeModuleInfo>().mData };
                context.EnsureBufferCapacity( buffer, mData.size() );
                context.CopyBuffer( buffer, 0, mData.data(), mData.size() );

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
                context.CopyTexture( fog.mLighting, slice, fog.mHistory, slice );
                context.CopyTexture( fog.mExtinction, slice, fog.mExtinctionHistory, slice );
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
                context.CopyTexture( b.Get<GeomShadingModuleInfo>().mColorImage, slice, b.Get<FogModuleInfo>().mSceneCopy, slice );
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
