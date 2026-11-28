//    Copyright 2026 ã‚±ã‚¤ãƒˆ
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
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Assets/ImageProcessing.hh>
#include <Assets/Utility.hh>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/AtmosphericScatteringModule.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/ShadowMappingModule.hh>
#include <Renderer/Rhi/Utility.hh>

#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    // Fixed export dimensions, independent of the scene viewport and its MSAA.
    static constexpr core::u32 kNoiseTextureSize{ 1024 };

    static auto RegisterNoiseTexture( FrameGraph &graph, const char *passName, const char *fragmentShader,
        const char *outputPath, FractalBrownianMotionParameters parameters ) -> FGTextureHandle {
        const eastl::string name{ passName };
        const auto texture{ graph.Create( FGTextureDescription{}
            .SetName( name + "_Texture" )
            .SetWidth( kNoiseTextureSize )
            .SetHeight( kNoiseTextureSize )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetFormat( Format::eRGBA8_UNORM )
            .SetUsage( TextureUsageFlagsBits::RenderTarget | TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::CopySource ) ) };
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( name + "_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .SetCullMode( CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .PushShader( "Noise_Vert.slang", FGStageType::eVertex )
            .PushShader( fragmentShader, FGStageType::ePixel ) ) };
        const auto readback{ graph.Create( FGBufferDescription{}
            .SetName( name + "_ReadbackBuffer" )
            .SetUsage( BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( static_cast<core::usize>( kNoiseTextureSize ) * kNoiseTextureSize * 4 )
            .SetHeapType( HeapType::eReadback ) ) };

        graph.RegisterPass(
            name,
            FGPassType::eGraphics,
            [texture]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( texture, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [texture, pipeline, parameters]( CommandContext &ctx, core::Blackboard & ) {
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( Rect{ kNoiseTextureSize, kNoiseTextureSize } )
                    .AddRenderTarget( texture, kColorBlack, LoadOp::eClear ) );
                ctx.SetViewportState( ViewportState{}
                        .AddViewportAndScissorRect( Viewport( kNoiseTextureSize, kNoiseTextureSize ) ) );
                ctx.BindPipeline( pipeline );
                ctx.PushConstants( parameters );
                ctx.Draw( 4 );
                ctx.EndRender();
            } );
        graph.SetExecutionPolicy( name, FGExecutionPolicy::eOnWake );

        const eastl::string copyName{ name + "_Readback" };
        graph.RegisterPass(
            copyName,
            FGPassType::eTransfer,
            [texture, readback]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( texture, FGPipelineStage::eCopy, FGResourceAccess::eRead );
                builder.UseResource( readback, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [&graph, texture, readback, path = filesystem::Path{ outputPath }]( CommandContext &ctx, core::Blackboard & ) {
                ctx.Copy( readback, texture );
                // Register with the submission that contains the copy. The callback
                // runs only once its fence completes, never while the GPU writes.
                graph.RegisterReadback( [readback, path]( core::Blackboard &, const FGResourceManager &manager ) {
                    const void *pixels{ manager.GetBufferMappedAddress( readback ) };

                    if ( !pixels ) {
                        MKT_CORE_LOGGER_ERROR( "Noise texture readback failed: {}", path.GetAbsolute() );
                        return;
                    }

                    const auto destination{ std::filesystem::u8path( path.GetAbsolute().c_str() ) };
                    std::error_code error{};
                    std::filesystem::create_directories( destination.parent_path(), error );

                    if ( error ) {
                        MKT_CORE_LOGGER_ERROR( "Could not save noise texture: {}", path.GetAbsolute() );
                        return;
                    }

                    try {
                        asset::WriteImage( path, pixels, kNoiseTextureSize, kNoiseTextureSize, asset::ImageFormat::eRGBA8_UINT );
                        MKT_CORE_LOGGER_INFO( "Saved noise texture: {}", path.GetAbsolute() );
                    } catch ( const std::exception &exception ) {
                        MKT_CORE_LOGGER_ERROR( "Could not save noise texture: {}", exception.what() );
                    }
                } );
            } );

        graph.SetExecutionPolicy( copyName, FGExecutionPolicy::eOnWake );
        return texture;
    }

    auto PrepassModule::RegisterPerlinNoise( FrameGraph &graph ) -> void {
        // Eight cells across the image make the single-octave structure visible.
        graph.GetOrCreate<PrepassModuleInfo>().mPerlinNoiseTarget = RegisterNoiseTexture(
            graph, "PerlinNoise", "PerlinNoise_Frag.slang", "Resources/Noise/PerlinNoise.png",
            FractalBrownianMotionParameters{
                .mOctaves = 1,
                .mPersistence = 0.5f,
                .mLacunarity = 2.7f,
                .mBaseFrequency = 8.0f,
            } );
    }

    auto PrepassModule::RegisterFractalBrownianMotion( FrameGraph &graph ) -> void {
        graph.GetOrCreate<PrepassModuleInfo>().mFractalBrownianMotionTarget = RegisterNoiseTexture(
            graph, "FractalBrownianMotion", "FractalBrownianMotion_Frag.slang", "Resources/Noise/FractalBrownianMotion.png",
            mFractalBrownianMotionParameters );
    }

    auto PrepassModule::SetFractalBrownianMotionParameters( const FractalBrownianMotionParameters &parameters ) -> void {
        if ( parameters.mOctaves < 1 || parameters.mOctaves > 12 || parameters.mPersistence < 0.0f || parameters.mPersistence > 1.0f ||
             parameters.mLacunarity < 1.0f || parameters.mLacunarity > 4.0f || parameters.mBaseFrequency <= 0.0f ||
             parameters.mBaseFrequency > 64.0f ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid fBm parameters: expected octaves [1,12], persistence [0,1], lacunarity [1,4], base frequency (0,64]." );
        }

        mFractalBrownianMotionParameters = parameters;
    }

    auto PrepassModule::GetFractalBrownianMotionParameters() const -> FractalBrownianMotionParameters {
        return mFractalBrownianMotionParameters;
    }

    PrepassModule::PrepassModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto PrepassModule::SetScene( const scene::Scene *scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScene = scene;
    }

    auto PrepassModule::SetCamera( const scene::Camera *camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto PrepassModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        MKT_BEGIN_PROFILER();
        mGeometryManagement = MKT_ADDRESSOF( geom );
    }

    auto PrepassModule::RegisterAabb( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();
        auto &info{ graph.GetOrCreate<PrepassModuleInfo>() };

        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "AABBGenComp_Clusters" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured )
            .SetElementsSize( mNumClusters, MKT_SIZEOF( ClusterParameters ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        info.mClusterBuffer = graph.Create( gpuBufferDesc );
        info.mClusterCount = mNumClusters;

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "AABBCluster_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "AABBGen_Comp.slang", FGStageType::eCompute ) };
        info.mAabbGenPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ClusterAabbConstruct",
            FGPassType::eCompute,
            []( FGNodeBuilder &b, core::Blackboard &blackboard ) {
                const auto &aabb{ blackboard.Get<PrepassModuleInfo>() };
                const auto &camera{ blackboard.Get<CameraModuleInfo>() };

                b.UseResource( aabb.mClusterBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                b.UseResource( camera.mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
                const auto &aabbData{ blackboard.Get<PrepassModuleInfo>() };
                const auto &cameraData{ blackboard.Get<CameraModuleInfo>() };

                struct ComputeParams {
                    core::float4 mGridSize{};
                    core::u64 mAabbBuffer{};
                    core::u64 mCameraInfoBuffer{};
                } params{
                    .mGridSize = glm::vec4{ mGridSizeX, mGridSizeY, mGridSizeZ, 0.0f },
                    .mAabbBuffer = ctx.GetDeviceBufferAddress( aabbData.mClusterBuffer ),
                    .mCameraInfoBuffer = ctx.GetDeviceBufferAddress( cameraData.mCameraData )
                };
                ctx.PushConstants( params );
                ctx.BindPipeline( aabbData.mAabbGenPipeline );

                ctx.Dispatch( mGridSizeX, mGridSizeY, mGridSizeZ );
            } );
    }

    auto PrepassModule::RegisterLightCulling( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mLights.resize( kMaxActiveLights );

        auto &info{ graph.GetOrCreate<PrepassModuleInfo>() };

        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "LightCulling_Clusters" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( kMaxActiveLights, MKT_SIZEOF( LightParameters ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) };
        info.mLightsBuffer = graph.Create( gpuBufferDesc );
        info.mAreaEmissionSampler = graph.Create( FGSamplerDescription{}
            .SetName( "AreaLight_EmissionSampler" )
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetMipLevels( 32.0f )
            .SetWrap( rhi::SamplerWrapMode::eRepeat ) );
        info.mLightsBufferCount = kMaxActiveLights;

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "LightCulling_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "LightCulling_Comp.slang", FGStageType::eCompute ) };
        info.mLightCullingPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "LightCulling_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, core::Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<PrepassModuleInfo>() };
                b.UseResource( data.mLightsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
                auto &data{ blackboard.Get<PrepassModuleInfo>() };
                const auto *atmosphere{ blackboard.TryGet<AtmosphereModuleInfo>() };
                SetupLightList( ctx, data.mLightsBuffer, data.mAreaEmissionSampler, atmosphere ? atmosphere->mSunLight : eastl::nullopt );

                data.mActiveLightCount = mActiveLights;
                data.mCpuLights = mLights.data();
                data.mShadowRecordCount = 0;

                for ( core::u32 i{}; i < mActiveLights; ++i ) {
                    if ( mLights[i].mShadowIndex >= 0 ) {
                        ++data.mShadowRecordCount;
                    }
                }
            } );

        graph.RegisterPass(
            "LightCulling_Compute",
            FGPassType::eCompute,
            []( FGNodeBuilder &b, core::Blackboard &blackboard ) {
                const auto &aabbData{ blackboard.Get<PrepassModuleInfo>() };
                const auto &cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto &lightCullingData{ blackboard.Get<PrepassModuleInfo>() };

                b.UseResource( aabbData.mClusterBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                b.UseResource( cameraData.mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );

                b.UseResource( lightCullingData.mLightsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, core::Blackboard &blackboard ) -> void {
                const auto &cameraData{ blackboard.Get<CameraModuleInfo>() };
                auto &lightingData{ blackboard.Get<PrepassModuleInfo>() };

                lightingData.mGridSize = core::float4{ mGridSizeX, mGridSizeY, mGridSizeZ, 0.0f };

                struct ComputeParams {
                    core::float4 mGridSize{};

                    SPointer mLightsPtr{};
                    SPointer mClustersPtr{};
                    SPointer mCameraInfoPtr{};

                    core::u32 mClusterCount{};
                    core::u32 mActiveLightCount{};
                } params{
                    .mGridSize = lightingData.mGridSize,

                    .mLightsPtr = ctx.GetDeviceBufferAddress( lightingData.mLightsBuffer ),
                    .mClustersPtr = ctx.GetDeviceBufferAddress( lightingData.mClusterBuffer ),
                    .mCameraInfoPtr = ctx.GetDeviceBufferAddress( cameraData.mCameraData ),

                    .mClusterCount = lightingData.mClusterCount,
                    .mActiveLightCount = mActiveLights
                };
                ctx.PushConstants( params );
                ctx.BindPipeline( lightingData.mLightCullingPipeline );

                const auto numWorkGroupsX{ ( mNumClusters + mLocalSize - 1 ) / mLocalSize };
                ctx.Dispatch( numWorkGroupsX, 1, 1 );
            } );
    }

    auto PrepassModule::SetupLightList( CommandContext &ctx, FGBufferHandle lightBuffer, FGSamplerHandle emissionSampler,
        eastl::optional<core::u32> sunLight ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mActiveLights = 0;

        if ( !mScene ) {
            return;
        }

        auto &registry{ mScene->GetRegistry() };
        const auto lightsView{ registry.view<scene::TagComponent, scene::TransformComponent, scene::LightComponent>() };

        core::i32 lightsCount{};
        core::u32 shadowCount{};
        core::u32 shadowSlots[3]{};

        for ( auto &lightEntity: lightsView ) {
            if ( lightsCount >= kMaxActiveLights ) {
                break;
            }

            const scene::TagComponent &tag{ registry.get<scene::TagComponent>( lightEntity ) };
            const scene::LightComponent &lightComp{ registry.get<scene::LightComponent>( lightEntity ) };
            const scene::TransformComponent &transformCom{ registry.get<scene::TransformComponent>( lightEntity ) };

            auto &uboLight{ mLights[lightsCount] };
            uboLight = {};
            const auto world = transformCom.GetWorldTransform();
            const auto position = float3( world[3] );

            bool castsShadows{};
            constexpr core::i32 inactiveLightType{ -1 };

            if ( !tag.IsActive() ) {
                uboLight.mActiveLightType = inactiveLightType;
                continue;
            }

            switch ( lightComp.GetActiveType() ) {
                case LightType::eDirectional: {
                    auto &dir{ lightComp.Get<DirectionalLight>() };

                    uboLight.mPadding = glm::tan( glm::radians( dir.GetAngularRadius() ) );
                    uboLight.mIntensity = dir.GetIntensity();
                    const bool isSun{ sunLight && tag.GetGuid() == *sunLight };
                    uboLight.mDirection = core::float4{ dir.GetWorldDirection( world ), isSun ? 1.0f : 0.0f };
                    uboLight.mPosition = float4( position, 1.0f );
                    uboLight.mDiffuse = float4( dir.GetColor() * dir.GetIntensity(), 1.0f );
                    uboLight.mActiveLightType = core::as<core::i32>( LightType::eDirectional );
                    castsShadows = dir.IsShadowCaster();

                    break;
                }

                case LightType::ePoint: {
                    auto &point{ lightComp.Get<PointLight>() };

                    uboLight.mPosition = float4( position, 1.0f );
                    uboLight.mDiffuse = float4( point.GetColor(), 0.0f );

                    uboLight.mPadding = point.GetSourceRadius();
                    uboLight.mIntensity = point.GetIntensity();
                    uboLight.mRadius = point.GetRadius();

                    uboLight.mActiveLightType = core::as<core::i32>( LightType::ePoint );
                    castsShadows = point.IsShadowCaster();

                    break;
                }

                case LightType::eSpot: {
                    auto &spot{ lightComp.Get<SpotLight>() };

                    uboLight.mPosition = float4( position, 1.0f );
                    uboLight.mDirection = core::float4{ spot.GetWorldDirection( world ), 0.0f };
                    uboLight.mDiffuse = float4( spot.GetColor(), 1.0f );

                    uboLight.mCutOff = spot.GetCutOff();
                    uboLight.mOuterCutOff = spot.GetOuterCutOff();

                    uboLight.mIntensity = spot.GetIntensity();
                    uboLight.mRadius = spot.GetRadius();

                    uboLight.mActiveLightType = core::as<core::i32>( LightType::eSpot );
                    castsShadows = spot.IsShadowCaster();

                    break;
                }
                case LightType::eAreaLight: {
                    const auto &area{ lightComp.Get<AreaLight>() };

                    if ( area.GetShape() == AreaLightShape::eMesh || area.GetRange() <= 0 ) {
                        continue;
                    }

                    const float3 axisX{ world[0] };
                    const float3 axisY{ world[1] };
                    const float lengthX{ glm::length( axisX ) };
                    const float lengthY{ glm::length( axisY ) };
                    const float lengthZ{ glm::length( float3{ world[2] } ) };
                    const float3 cross{ glm::cross( axisX, axisY ) };
                    const float crossLength{ glm::length( cross ) };

                    if ( lengthX <= 1e-6f || crossLength <= 1e-6f ) {
                        continue;
                    }

                    const float3 right{ axisX / lengthX };
                    const float3 up{ glm::cross( cross / crossLength, right ) };
                    // Preserve the authored +Z emission side under reflected entity transforms.
                    const float3 normal{ cross / crossLength * ( glm::determinant( glm::mat3{ world } ) < 0 ? -1.0f : 1.0f ) };
                    uboLight.mPosition = float4{ position, as<float>( area.GetShape() ) };
                    uboLight.mDirection = float4{ normal, area.IsTwoSided() ? 1.0f : 0.0f };
                    uboLight.mDiffuse = float4{ area.GetColor(), 1.0f };
                    uboLight.mIntensity = area.GetIntensity();
                    uboLight.mRadius = area.GetRange();
                    const float sourceScale{ area.GetShape() == AreaLightShape::eTube ? glm::max( lengthY, lengthZ ) : glm::max( lengthX, glm::max( lengthY, lengthZ ) ) };
                    uboLight.mPadding = area.GetRadius() * sourceScale;
                    uboLight.mAreaAxisX = float4{ right, 0.5f * area.GetSize().x * lengthX };
                    uboLight.mAreaAxisY = float4{ up, 0.5f * area.GetSize().y * glm::abs( glm::dot( axisY, up ) ) };
                    if ( area.GetShape() == AreaLightShape::eRectangle && area.IsMaterialEmissionEnabled() ) {
                        const auto *selected{ mScene->FindByID( area.GetEmissionSourceEntityID() ) };
                        const auto sourceEntity{ area.GetEmissionSourceEntityID() ? ( selected ? selected->Get() : entt::null ) : lightEntity };
                        const auto *source{ registry.try_get<scene::MaterialComponent>( sourceEntity ) };
                        auto sourceMaterial{ source ? material::SurfaceMaterialHandle{
                                                          dynamic_cast<material::SurfaceMaterial *>( source->GetMaterial().GetPtr() ) }
                                                    : material::SurfaceMaterialHandle{} };
                        auto map{ material::MapType::eEmissive };
                        bool videoSource{};

                        // Match visible geometry's dynamic material rather than
                        // sampling the authored asset's old poster/static map.
                        if ( const auto *video{ registry.try_get<scene::VideoComponent>( sourceEntity ) } ) {
                            const auto data{ video->GetRenderData() };
                            auto presented{ data ? material::SurfaceMaterialHandle{
                                                       dynamic_cast<material::SurfaceMaterial *>( data->GetMaterial().GetPtr() ) }
                                                 : material::SurfaceMaterialHandle{} };
                            if ( presented ) {
                                sourceMaterial = eastl::move( presented );
                                map = video->GetOutputMap() == VideoOutputMap::eEmissive ? material::MapType::eEmissive : material::MapType::eBaseColor;
                                videoSource = true;
                            }
                        }

                        if ( sourceMaterial ) {
                            if ( !videoSource && !( sourceMaterial->IsTextureUsed( map ) && sourceMaterial->HasTexture( map ) ) ) {
                                map = material::MapType::eBaseColor;
                            }
                            auto texture{ sourceMaterial->IsTextureUsed( map ) ? sourceMaterial->GetTexture( map ) : rhi::TextureHandle{} };
                            if ( texture && texture->GetDimension() == TextureDimension::eTexture2D ) {
                                const auto &format{ rhi::GetFormatInfo( texture->GetFormat() ) };
                                const bool decodeSrgb{ !format.mIsSRGB && format.mKind != rhi::FormatKind::Float };
                                uboLight.mAreaEmission = { ctx.UseImportedTexture( ctx.ImportSampledTexture( texture ) ),
                                    ctx.PushSampler( emissionSampler ), decodeSrgb ? 1u : 0u, texture->GetMipLevelCount() };
                                uboLight.mAreaEmissionUv = sourceMaterial->GetUvTransform();
                                const float3 tint{ map == material::MapType::eEmissive ? sourceMaterial->GetEmissiveFactor() * sourceMaterial->GetEmissiveStrength() : float3{ sourceMaterial->GetBaseColorFactor() } };
                                uboLight.mDiffuse *= float4{ tint, 1 };
                            }
                        }
                    }

                    uboLight.mActiveLightType = as<core::i32>( LightType::eAreaLight );
                    break;
                }

                default:
                    continue;
            }

            if ( uboLight.mIntensity <= 0 ) {
                continue;
            }

            const auto type = uboLight.mActiveLightType;

            if ( castsShadows && type >= 0 && type < 3 && shadowSlots[type] < kMaxShadowLightsPerType ) {
                uboLight.mShadowIndex = as<core::i32>( shadowCount++ );
                uboLight.mShadowSlot = shadowSlots[type]++;
            }

            ++lightsCount;
        }

        // Reuse the regular point-light path for ground/character illumination.
        // The flash state is shared with the cloud shader; no helper scene entities.
        for ( const auto [entity, cloud, tag, transform]: registry.view<CloudComponent, TagComponent, TransformComponent>().each() ) {
            if ( !tag.IsActive() || lightsCount >= kMaxActiveLights ) {
                continue;
            }
            const auto flash{ cloud.GetLightningState( mScene->GetVolumeTime(), transform.GetWorldTransform() ) };
            if ( flash.mColorIntensity.w <= 0 ) {
                continue;
            }
            auto &light{ mLights[lightsCount++] };
            light = {};
            light.mPosition = { float3{ flash.mPositionRadius }, 1.0f };
            light.mDiffuse = { float3{ flash.mColorIntensity }, 0.0f };
            light.mActiveLightType = as<i32>( LightType::ePoint );
            light.mRadius = flash.mPositionRadius.w;
            // The falloff shader uses inverse-square attenuation; scale the
            // artist's flash brightness to its influence radius in metres.
            light.mIntensity = flash.mColorIntensity.w * flash.mPositionRadius.w * flash.mPositionRadius.w * .01f;
        }

        mActiveLights = lightsCount;

        if ( mActiveLights != 0 ) {
            ctx.Copy( lightBuffer, 0, mLights.data(), mActiveLights * MKT_SIZEOF( LightParameters ) );
        }
    }

    auto PrepassModule::RegisterGBuffer( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &info{ graph.GetOrCreate<PrepassModuleInfo>() };
        auto dimensions{ InferDimensions( mResolution ) };

        auto positionDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Position" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA32_FLOAT ) };
        info.mGBufferPositionTarget = graph.Create( positionDesc );

        auto normalDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Normal" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        info.mGBufferNormalTarget = graph.Create( normalDesc );
        info.mGBufferContourTarget = graph.Create( normalDesc.SetName( "GBuffer_Contour" ) );

        auto colorDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Color" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        info.mGBufferColorTarget = graph.Create( colorDesc );

        auto emissiveDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Emissive" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA16_FLOAT ) };
        info.mGBufferEmissiveTarget = graph.Create( emissiveDesc );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "GBuffer_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetBlendEnable( false )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .SetDepthWrite( false )
            .SetDepthTest( true )
            .AddColorFormat( renderer::rhi::Format::eRGBA32_FLOAT )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .PushShader( "GBuffer_Vert.slang", FGStageType::eVertex )
            .PushShader( "GBuffer_Frag.slang", FGStageType::ePixel ) };
        info.mGBufferPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "GBuffer",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, core::Blackboard &blackboard ) {
                const auto &prepassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto &geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto &cameraPassData{ blackboard.Get<CameraModuleInfo>() };

                builder.UseResource( prepassData.mGBufferColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mGBufferPositionTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mGBufferNormalTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mGBufferContourTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mGBufferEmissiveTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                builder.UseResource( prepassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );

                builder.UseResource( cameraPassData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mDrawInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningCacheBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mAttributeBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) -> void {
                const auto &prepassData{ b.Get<PrepassModuleInfo>() };
                const auto &geometryData{ b.Get<GeometryCullModuleInfo>() };
                const auto &cameraPassData{ b.Get<CameraModuleInfo>() };

                struct DrawParams {
                    core::u64 mGeometryInfoBufferID{};
                    core::u64 mMaterialsInfoBufferID{};
                    core::u64 mSkinningInfoBufferID{};
                    core::u64 mGeometryAllocBuffer{};
                    core::u64 mCameraInfoBufferID{};
                    core::u32 mBasicSamplerID{};
                } params{
                    .mGeometryInfoBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mMaterialsInfoBufferID = ctx.GetDeviceBufferAddress( geometryData.mMaterialsBuffer ),
                    .mSkinningInfoBufferID = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),
                    .mGeometryAllocBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mCameraInfoBufferID = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData ),
                    .mBasicSamplerID = ctx.PushSampler( geometryData.mMaterialSampler )
                };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddDepthTarget( prepassData.mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( prepassData.mGBufferPositionTarget, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear )
                    .AddRenderTarget( prepassData.mGBufferNormalTarget, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear )
                    .AddRenderTarget( prepassData.mGBufferColorTarget, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear )
                    .AddRenderTarget( prepassData.mGBufferEmissiveTarget, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear )
                    .AddRenderTarget( prepassData.mGBufferContourTarget, renderer::rhi::Color{ 0, 0, 0, 0 }, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( prepassData.mGBufferPipeline );
                // BLEND meshes may contain opaque pixels. The shader keeps
                // exactly the coverage accepted by the depth prepass.
                mGeometryManagement->DrawInstancesIndirect( ctx, GeometryDrawGroup::eAll );

                ctx.EndRender();
            } );
    }

    auto PrepassModule::RegisterDepthPrepass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        PrepassModuleInfo &info{ graph.GetOrCreate<PrepassModuleInfo>() };

        auto dimensions{ InferDimensions( mResolution ) };

        // Color attachment for debugging
        auto colorImage{ FGTextureDescription{}
            .SetName( "DepthPrepass_ColorImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget |
                           renderer::rhi::TextureUsageFlagsBits::ShaderResource )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) };
        info.mDepthPrepassColorTarget = graph.Create( colorImage );

        auto depthImage{ FGTextureDescription{}
            .SetName( "DepthPrepass_DepthImage01" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget |
                           renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::CopySource )
            .SetFormat( renderer::rhi::Format::eD32 ) };
        info.mPrepassDepthTarget = graph.Create( depthImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "DepthPrePass_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthWrite( true )
            .SetDepthTest( true )
            .AddColorFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .PushShader( "ZPass_Vert.slang", FGStageType::eVertex )
            .PushShader( "ZPass_Frag.slang", FGStageType::ePixel ) };
        info.mDepthPrepassPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "DepthPrePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, core::Blackboard &blackboard ) {
                auto &prepassData{ blackboard.Get<PrepassModuleInfo>() };
                auto &cameraPassData{ blackboard.Get<CameraModuleInfo>() };
                auto &geometryData{ blackboard.Get<GeometryCullModuleInfo>() };

                builder.UseResource( prepassData.mDepthPrepassColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( geometryData.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( prepassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );

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
            [this]( CommandContext &ctx, core::Blackboard &b ) -> void {
                const auto &prepassData{ b.Get<PrepassModuleInfo>() };
                const auto &cameraPassData{ b.Get<CameraModuleInfo>() };
                const auto &geometryData{ b.Get<GeometryCullModuleInfo>() };

                struct DrawParams {
                    core::u64 mGeometryBufferID{};
                    core::u64 mSkinningBufferID{};
                    core::u64 mGeometryAllocationBufferID{};

                    core::u64 mCameraBda{};
                    core::u64 mMaterialsPtr{};
                    core::u32 mBasicSamplerID{};
                } params{
                    .mGeometryBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mSkinningBufferID = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),

                    .mGeometryAllocationBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),

                    .mCameraBda = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData ),
                    .mMaterialsPtr = ctx.GetDeviceBufferAddress( geometryData.mMaterialsBuffer ),
                    .mBasicSamplerID = ctx.PushSampler( geometryData.mMaterialSampler )
                };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                // Black background because Inversed Z
                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddDepthTarget( prepassData.mPrepassDepthTarget, renderer::rhi::LoadOp::eClear )
                    .AddRenderTarget( prepassData.mDepthPrepassColorTarget, renderer::rhi::kColorBlack, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( prepassData.mDepthPrepassPipeline );

                // Let the shader separate opaque coverage from fractional
                // opacity, rather than discarding whole BLEND meshes here.
                mGeometryManagement->DrawInstancesIndirect( ctx, GeometryDrawGroup::eAll );

                ctx.EndRender();
            } );
    }
}// namespace mikoto::renderer
