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
#include <Core/Profiler.hh>
#include <Core/Types.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    PrepassModule::PrepassModule( RenderResolution resolution )
        : mResolution{ resolution } {}

    auto PrepassModule::SetScene( const Scene *scene ) -> void {
        mScene = scene;
    }

    auto PrepassModule::SetCamera( const Camera *camera ) -> void {
        mCamera = camera;
    }

    auto PrepassModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        mGeometryManagement = MKT_ADDRESSOF( geom );
    }

    auto PrepassModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterAabb( graph );
        RegisterLightCulling( graph );

        RegisterDepthPrepass( graph );
        RegisterGBuffer( graph );
    }

    auto PrepassModule::RegisterAabb( FrameGraph &graph ) -> void {
        auto& info{ graph.GetOrCreate<PrepassModuleInfo>() };

        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "AABBGenComp_Clusters" )
            .SetUsage( BufferUsageFlagsBits::kStorage )
            .SetElementsSize( mNumClusters, MKT_SIZEOF( ClusterParameters ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        info.mClusterBuffer = graph.Create( gpuBufferDesc );
        info.mClusterCount = mNumClusters;

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "AABBCluster_Pipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "AABBGen_Comp.slang", FGStageType::eCompute ) };
        info.mAabbGenPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ClusterAabbConstruct",
            FGPassType::eCompute,
            []( FGNodeBuilder&b, Blackboard& blackboard ) {
                const auto& aabb{ blackboard.Get<PrepassModuleInfo>() };
                const auto& camera{ blackboard.Get<CameraModuleInfo>() };

                b.UseResource( aabb.mClusterBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                b.UseResource( camera.mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) -> void {
                const auto &aabbData{ blackboard.Get<PrepassModuleInfo>() };
                const auto &cameraData{ blackboard.Get<CameraModuleInfo>() };

                struct ComputeParams {
                    float4 mGridSize{};
                    u64 mAabbBuffer{};
                    u64 mCameraInfoBuffer{};
                } params{
                    .mGridSize = glm::vec4{ mGridSizeX, mGridSizeY, mGridSizeZ, 0.0f },
                    .mAabbBuffer = ctx.GetDeviceBufferAddress( aabbData.mClusterBuffer ),
                    .mCameraInfoBuffer = ctx.GetDeviceBufferAddress( cameraData.mCameraData ) };
                ctx.PushConstants( params );
                ctx.BindPipeline( aabbData.mAabbGenPipeline );

                ctx.Dispatch( mGridSizeX, mGridSizeY, mGridSizeZ );
            } );
    }

    auto PrepassModule::RegisterLightCulling( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mLights.resize( kMaxActiveLights );

        auto& info{ graph.GetOrCreate<PrepassModuleInfo>() };

        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "LightCulling_Clusters" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxActiveLights, MKT_SIZEOF( LightParameters ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        info.mLightsBuffer = graph.Create( gpuBufferDesc );
        info.mLightsBufferCount = kMaxActiveLights;

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "LightCulling_Pipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "LightCulling_Comp.slang", FGStageType::eCompute ) };
        info.mLightCullingPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "LightCulling_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder&b, Blackboard& blackboard ) {
                const auto &data{ blackboard.Get<PrepassModuleInfo>() };
                b.UseResource( data.mLightsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                auto &data{ blackboard.Get<PrepassModuleInfo>() };
                SetupLightList( ctx, data.mLightsBuffer );

                data.mActiveLightCount = mActiveLights;
            } );

        graph.RegisterPass(
            "LightCulling_Compute",
            FGPassType::eCompute,
            []( FGNodeBuilder&b, Blackboard& blackboard ) {
                const auto &aabbData{ blackboard.Get<PrepassModuleInfo>() };
                const auto &cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto &lightCullingData{ blackboard.Get<PrepassModuleInfo>() };

                b.UseResource( aabbData.mClusterBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                b.UseResource( cameraData.mCameraData, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );

                b.UseResource( lightCullingData.mLightsBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                const auto &cameraData{ blackboard.Get<CameraModuleInfo>() };
                auto &lightingData{ blackboard.Get<PrepassModuleInfo>() };

                if (mActiveLights == 0) {
                    return;
                }

                lightingData.mGridSize = float4{ mGridSizeX, mGridSizeY, mGridSizeZ, 0.0f };

                struct ComputeParams {
                    float4 mGridSize{};

                    SPointer mLightsPtr{};
                    SPointer mClustersPtr{};
                    SPointer mCameraInfoPtr{};

                    u32 mClusterCount{};
                    u32 mActiveLightCount{};
                } params{
                    .mGridSize = lightingData.mGridSize,

                    .mLightsPtr = ctx.GetDeviceBufferAddress( lightingData.mLightsBuffer ),
                    .mClustersPtr = ctx.GetDeviceBufferAddress( lightingData.mClusterBuffer ),
                    .mCameraInfoPtr = ctx.GetDeviceBufferAddress( cameraData.mCameraData ),

                    .mClusterCount = lightingData.mClusterCount,
                    .mActiveLightCount = mActiveLights };
                ctx.PushConstants( params );
                ctx.BindPipeline( lightingData.mLightCullingPipeline );

                const auto numWorkGroupsX{ ( mNumClusters + mLocalSize - 1 ) / mLocalSize };
                ctx.Dispatch( numWorkGroupsX, 1, 1 );
            } );
    }

    auto PrepassModule::SetupLightList( CommandContext &ctx, FGBufferHandle lightBuffer ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &registry{ mScene->GetRegistry() };
        const auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        i32 lightsCount{};

        for (auto &lightEntity: lightsView) {
            if (lightsCount >= kMaxActiveLights) {
                break;
            }

            const TagComponent &tag{ registry.get<TagComponent>( lightEntity ) };
            const LightComponent &lightComp{ registry.get<LightComponent>( lightEntity ) };
            const TransformComponent &transformCom{ registry.get<TransformComponent>( lightEntity ) };

            auto &uboLight{ mLights[lightsCount] };

            constexpr i32 inactiveLightType{ -1 };

            if (!tag.IsActive()) {
                uboLight.mActiveLightType = inactiveLightType;
                continue;
            }

            switch (lightComp.GetActiveType()) {
                case LightType::eDirectional: {
                    auto &dir{ lightComp.Get<DirectionalLight>() };

                    uboLight.mIntensity = dir.GetIntensity();
                    uboLight.mDirection = float4( dir.GetDirection(), 0.0f );
                    uboLight.mPosition = float4( transformCom.GetTranslation(), 1.0f );
                    uboLight.mDiffuse = float4( dir.GetColor() * dir.GetIntensity(), 1.0f );
                    uboLight.mActiveLightType = as<i32>( LightType::eDirectional );

                    break;
                }

                case LightType::ePoint: {
                    auto &point{ lightComp.Get<PointLight>() };

                    uboLight.mPosition = float4( transformCom.GetTranslation(), 1.0f );
                    uboLight.mDiffuse = float4( point.GetColor(), 0.0f );

                    uboLight.mIntensity = point.GetIntensity();
                    uboLight.mRadius = point.GetRadius();

                    uboLight.mActiveLightType = as<i32>( LightType::ePoint );

                    break;
                }

                case LightType::eSpot: {
                    auto &spot{ lightComp.Get<SpotLight>() };

                    uboLight.mPosition = float4( transformCom.GetTranslation(), 1.0f );
                    uboLight.mDirection = float4( spot.GetDirection(), 0.0f );
                    uboLight.mDiffuse = float4( spot.GetColor(), 1.0f );

                    uboLight.mCutOff = spot.GetCutOff();
                    uboLight.mOuterCutOff = spot.GetOuterCutOff();

                    uboLight.mIntensity = spot.GetIntensity();
                    uboLight.mRadius = spot.GetRadius();

                    uboLight.mActiveLightType = as<i32>( LightType::eSpot );

                    break;
                }
                default:;
            }

            ++lightsCount;
        }

        mActiveLights = lightsCount;

        if (mActiveLights != 0) {
            ctx.CopyBuffer( lightBuffer, 0, mLights.data(), mActiveLights * MKT_SIZEOF( LightParameters )  );
        }
    }

    auto PrepassModule::RegisterGBuffer( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<PrepassModuleInfo>() };
        auto dimensions{ InferDimensions( mResolution ) };

        auto positionDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Position" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA32_FLOAT ) };
        info.mGBufferPositionTarget = graph.Create( positionDesc );

        auto normalDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Normal" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        info.mGBufferNormalTarget = graph.Create( normalDesc );

        auto colorDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Color" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        info.mGBufferColorTarget = graph.Create( colorDesc );

        auto emissiveDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Emissive" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) };
        info.mGBufferEmissiveTarget = graph.Create( emissiveDesc );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "GBuffer_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetCullMode( CullMode::eNone )
            .SetDepthFormat( Format::eD32 )
            .SetDepthWrite( false )
            .SetDepthTest( true )
            .AddColorFormat( Format::eRGBA32_FLOAT )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "GBuffer_Vert.slang", FGStageType::eVertex )
            .PushShader( "GBuffer_Frag.slang", FGStageType::ePixel ) };
        info.mGBufferPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "GBuffer",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                const auto& prepassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& cameraPassData{ blackboard.Get<CameraModuleInfo>() };

                builder.UseResource( prepassData.mGBufferColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mGBufferPositionTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mGBufferNormalTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mGBufferEmissiveTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );

                builder.UseResource( prepassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );

                builder.UseResource( cameraPassData.mCameraData, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                const auto& prepassData{ b.Get<PrepassModuleInfo>() };
                const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };
                const auto& cameraPassData{ b.Get<CameraModuleInfo>() };

                struct DrawParams {
                    u64 mGeometryInfoBufferID{};
                    u64 mMaterialsInfoBufferID{};
                    u64 mSkinningInfoBufferID{};
                    u64 mGeometryAllocBuffer{};
                    u64 mCameraInfoBufferID{};
                    u32 mBasicSamplerID{};
                } params{
                    .mGeometryInfoBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mMaterialsInfoBufferID = ctx.GetDeviceBufferAddress( geometryData.mMaterialsBuffer ),
                    .mSkinningInfoBufferID = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),
                    .mGeometryAllocBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mCameraInfoBufferID = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData ),
                    .mBasicSamplerID = ctx.PushSampler( geometryData.mBasicSampler ) };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prepassData.mPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( prepassData.mGBufferPositionTarget, kColorBlack, LoadOp::eClear )
                    .AddRenderTarget( prepassData.mGBufferNormalTarget, kColorBlack, LoadOp::eClear )
                    .AddRenderTarget( prepassData.mGBufferColorTarget, kColorBlack, LoadOp::eClear )
                    .AddRenderTarget( prepassData.mGBufferEmissiveTarget, kColorBlack, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( prepassData.mGBufferPipeline );
                mGeometryManagement->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );
    }

    auto PrepassModule::RegisterDepthPrepass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        PrepassModuleInfo& info{ graph.GetOrCreate<PrepassModuleInfo>() };

        auto dimensions{ InferDimensions( mResolution ) };

        // Color attachment for debugging
        auto colorImage{ FGTextureDescription{}
            .SetName( "DepthPrepass_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        info.mDepthPrepassColorTarget = graph.Create( colorImage );

        auto depthImage{ FGTextureDescription{}
            .SetName( "DepthPrepass_DepthImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };
        info.mPrepassDepthTarget = graph.Create( depthImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "DepthPrePass_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .SetCullMode( CullMode::eNone )
            .SetDepthWrite( true )
            .SetDepthTest( true )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .PushShader( "ZPass_Vert.slang", FGStageType::eVertex )
            .PushShader( "ZPass_Frag.slang", FGStageType::ePixel ) };
        info.mDepthPrepassPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "DepthPrePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                auto& prepassData{ blackboard.Get<PrepassModuleInfo>() };
                auto& cameraPassData{ blackboard.Get<CameraModuleInfo>() };
                auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };

                builder.UseResource( prepassData.mDepthPrepassColorTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( prepassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );

                builder.UseResource( cameraPassData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            },
            [this]( CommandContext& ctx, Blackboard& b ) -> void {
                const auto& prepassData{ b.Get<PrepassModuleInfo>() };
                const auto& cameraPassData{ b.Get<CameraModuleInfo>() };
                const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };

                struct DrawParams {
                    u64 mGeometryBufferID{};
                    u64 mSkinningBufferID{};
                    u64 mGeometryAllocationBufferID{};

                    u64 mCameraBda{};
                } params{
                    .mGeometryBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mSkinningBufferID = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),

                    .mGeometryAllocationBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),

                    .mCameraBda = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData ) };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                // Black background because Inversed Z
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prepassData.mPrepassDepthTarget, LoadOp::eClear )
                    .AddRenderTarget( prepassData.mDepthPrepassColorTarget, kColorBlack, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( prepassData.mDepthPrepassPipeline );

                mGeometryManagement->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );
    }
}
