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

    using namespace mikoto::scene;

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

        RegisterAABB( graph );
        RegisterLightCulling( graph );

        RegisterDepthPrepass( graph );
        RegisterGBuffer( graph );
    }

    auto PrepassModule::RegisterAABB( FrameGraph &graph ) -> void {
        PrepassModuleInfo& info{ graph.GetOrCreate<PrepassModuleInfo>() };

        // GPU buffer (fast, written by compute shader)
        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "AABBGenComp_Clusters" )
            .SetUsage( BufferUsageFlagsBits::kStorage )
            .SetElementsSize( mNumClusters, MKT_SIZEOF( ClusterParameters ) )
            .SetHeapType( HeapType::eDeviceLocal ) };

        info.mClusterBuffer = graph.Create( gpuBufferDesc );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "AABBCluster_Pipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "AABBGen_Comp.slang", FGStageType::eCompute ) };

        info.mAabbGenPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ClusterAABBPass",
            FGPassType::eCompute,
            []( FGNodeBuilder&b, Blackboard& blackboard ) {
                const auto& aabb{ blackboard.Get<PrepassModuleInfo>() };
                const auto& camera{ blackboard.Get<CameraModuleInfo>() };

                b.Write( aabb.mClusterBuffer, FGResourceState::eUnorderedAccess );
                b.Read( camera.mCameraData, FGResourceState::eShaderResource );
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) -> void {
                const auto &aabbData{ blackboard.Get<PrepassModuleInfo>() };
                const auto &cameraInfo{ blackboard.Get<CameraModuleInfo>() };

                struct ComputeParams {
                    float4 mGridSize{};
                    u32 mAabbBuffer{};
                    u32 mCameraInfo{};
                    u32 mActiveLightCount{};
                } params{
                    .mGridSize = glm::vec4{ mGridSizeX, mGridSizeY, mGridSizeZ, 0.0f },
                    .mAabbBuffer = ctx.PushBuffer_UAV( aabbData.mClusterBuffer ),
                    .mCameraInfo = ctx.PushBuffer_SRV( cameraInfo.mCameraData ),
                    .mActiveLightCount = 0
                };

                ctx.PushConstants( params );
                ctx.BindPipeline( aabbData.mAabbGenPipeline );

                ctx.Dispatch( mGridSizeX, mGridSizeY, mGridSizeZ );
            } );
    }

    auto PrepassModule::RegisterLightCulling( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mLights.resize( kMaxActiveLights );

        PrepassModuleInfo& info{ graph.GetOrCreate<PrepassModuleInfo>() };

        // GPU buffer (fast, written by compute shader)
        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "LightCulling_Clusters" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxActiveLights, MKT_SIZEOF( LightParameters ) )
            .SetHeapType( HeapType::eDeviceLocal ) };

        info.mLightCullingBuffer = graph.Create( gpuBufferDesc );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "LightCulling_Pipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "LightCulling_Comp.slang", FGStageType::eCompute ) };

        info.mLightCullingPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "LightCulling_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder&b, Blackboard& blackboard ) {
                const auto &lightCullingInfo{ blackboard.Get<PrepassModuleInfo>() };
                b.Write( lightCullingInfo.mLightCullingBuffer, FGResourceState::eCopyDest );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                auto &lightCullingInfo{ blackboard.Get<PrepassModuleInfo>() };
                SetupLightList( ctx, lightCullingInfo.mLightCullingBuffer );

                lightCullingInfo.mActiveLightCount = mActiveLights;
            } );

        graph.RegisterPass(
            "LightCulling_Compute",
            FGPassType::eCompute,
            []( FGNodeBuilder&b, Blackboard& blackboard ) {
                const auto &aabbData{ blackboard.Get<PrepassModuleInfo>() };
                const auto &cameraInfo{ blackboard.Get<CameraModuleInfo>() };
                const auto &lightCullingInfo{ blackboard.Get<PrepassModuleInfo>() };

                b.Write( aabbData.mClusterBuffer, FGResourceState::eShaderResource );
                b.Read( cameraInfo.mCameraData, FGResourceState::eShaderResource );

                b.Read( lightCullingInfo.mLightCullingBuffer, FGResourceState::eUnorderedAccess );
            },
            [this]( CommandContext& ctx, Blackboard& blackboard ) -> void {
                const auto &cameraInfo{ blackboard.Get<CameraModuleInfo>() };
                auto &lightCullingInfo{ blackboard.Get<PrepassModuleInfo>() };

                if (mActiveLights == 0) {
                    return;
                }

                lightCullingInfo.mGridSize = float4{ mGridSizeX, mGridSizeY, mGridSizeZ, 0.0f };

                struct ComputeParams {
                    float4 mGridSize{};
                    u32 mClusterBuffer{};
                    u32 mCameraInfo{};
                    u32 mLightCullingBuffer{};
                    u32 mActiveLightCount{};
                } params{
                    .mGridSize = lightCullingInfo.mGridSize,
                    .mClusterBuffer = ctx.PushBuffer_UAV( lightCullingInfo.mClusterBuffer ),
                    .mCameraInfo = ctx.PushBuffer_SRV( cameraInfo.mCameraData ),
                    .mLightCullingBuffer = ctx.PushBuffer_SRV( lightCullingInfo.mLightCullingBuffer ),
                    .mActiveLightCount = mActiveLights
                };

                ctx.PushConstants( params );
                ctx.BindPipeline( lightCullingInfo.mLightCullingPipeline );

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

        PrepassModuleInfo& info{ graph.GetOrCreate<PrepassModuleInfo>() };

        auto dimensions{ InferDimensions( mResolution ) };

        auto positionDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Position" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA32_FLOAT ) };

        info.mGBufferPositionTarget = graph.Create( positionDesc );

        auto normalDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Normal" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };

        info.mGBufferNormalTarget = graph.Create( normalDesc );

        auto colorDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Color" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };

        info.mGBufferColorTarget = graph.Create( colorDesc );

        auto emissiveDesc{ FGTextureDescription{}
            .SetName( "GBuffer_Emissive" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA16_FLOAT ) };

        info.mGBufferEmissiveTarget = graph.Create( emissiveDesc );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "GBuffer_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetCullMode( CullMode::eNone )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eRGBA32_FLOAT )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .AddColorFormat( Format::eRGBA16_FLOAT )
            .PushShader( "GBuffer_Vert.slang", FGStageType::eVertex )
            .PushShader( "GBuffer_Frag.slang", FGStageType::eFragment ) };

        info.mGBufferPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "GBuffer",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                PrepassModuleInfo& prepassInfo{ blackboard.Get<PrepassModuleInfo>() };
                CameraModuleInfo& cameraPassInfo{ blackboard.Get<CameraModuleInfo>() };
                GeometryManagementModuleInfo& geometryInfo{ blackboard.Get<GeometryManagementModuleInfo>() };

                builder.Write( prepassInfo.mGBufferColorTarget, FGResourceState::eRenderTarget );
                builder.Write( prepassInfo.mGBufferPositionTarget, FGResourceState::eRenderTarget );
                builder.Write( prepassInfo.mGBufferNormalTarget, FGResourceState::eRenderTarget );
                builder.Write( prepassInfo.mGBufferEmissiveTarget, FGResourceState::eRenderTarget );

                builder.Read( prepassInfo.mDepthPrepassDepthTarget, FGResourceState::eDepthRead );

                builder.Read( cameraPassInfo.mCameraData, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mGeometryBuffer, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mMaterialsBuffer, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mSkinningBuffer, FGResourceState::eShaderResource );

                builder.Read( geometryInfo.mVerticesBuffer, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mIndicesBuffer, FGResourceState::eShaderResource );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                PrepassModuleInfo& prepassInfo{ b.Get<PrepassModuleInfo>() };
                CameraModuleInfo& cameraPassInfo{ b.Get<CameraModuleInfo>() };
                GeometryManagementModuleInfo& geometryInfo{ b.Get<GeometryManagementModuleInfo>() };

                struct DrawParams {
                    u32 mGeometryInfoBufferID{};
                    u32 mMaterialsInfoBufferID{};
                    u32 mSkinningInfoBufferID{};

                    u32 mBasicSamplerID{};

                    u32 mIndexID{};
                    u32 mVertexID{};

                    u32 mCameraInfoBufferID{};
                } params{
                    .mGeometryInfoBufferID = ctx.PushBuffer_SRV( geometryInfo.mGeometryBuffer ),
                    .mMaterialsInfoBufferID = ctx.PushBuffer_SRV( geometryInfo.mMaterialsBuffer ),
                    .mSkinningInfoBufferID = ctx.PushBuffer_SRV( geometryInfo.mSkinningBuffer ),

                    .mBasicSamplerID = ctx.PushSampler( geometryInfo.mBasicSampler ),

                    .mIndexID = ctx.PushBuffer_SRV( geometryInfo.mIndicesBuffer ),
                    .mVertexID = ctx.PushBuffer_SRV( geometryInfo.mVerticesBuffer ),

                    .mCameraInfoBufferID = ctx.PushBuffer_SRV( cameraPassInfo.mCameraData ),
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prepassInfo.mDepthPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( prepassInfo.mGBufferPositionTarget, kColorBlack, LoadOp::eClear )
                    .AddRenderTarget( prepassInfo.mGBufferNormalTarget, kColorBlack, LoadOp::eClear )
                    .AddRenderTarget( prepassInfo.mGBufferColorTarget, kColorBlack, LoadOp::eClear )
                    .AddRenderTarget( prepassInfo.mGBufferEmissiveTarget, kColorBlack, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( prepassInfo.mGBufferPipeline );
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
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };

        info.mDepthPrepassColorTarget = graph.Create( colorImage );

        auto depthImage{ FGTextureDescription{}
            .SetName( "DepthPrepass_DepthImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        info.mDepthPrepassDepthTarget = graph.Create( depthImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "DepthPrePass_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetDepthFormat( Format::eD32 )
            .SetCullMode( CullMode::eNone )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .PushShader( "ZPass_Vert.slang", FGStageType::eVertex )
            .PushShader( "ZPass_Frag.slang", FGStageType::eFragment ) };

        info.mDepthPrepassPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "DepthPrePass",
            FGPassType::eGraphics,
            []( FGNodeBuilder&builder, Blackboard& blackboard ) {
                PrepassModuleInfo& prepassInfo{ blackboard.Get<PrepassModuleInfo>() };
                CameraModuleInfo& cameraPassInfo{ blackboard.Get<CameraModuleInfo>() };
                GeometryManagementModuleInfo& geometryInfo{ blackboard.Get<GeometryManagementModuleInfo>() };

                builder.Write( prepassInfo.mDepthPrepassColorTarget, FGResourceState::eRenderTarget );
                builder.Write( prepassInfo.mDepthPrepassDepthTarget, FGResourceState::eDepthWrite );

                builder.Read( cameraPassInfo.mCameraData, FGResourceState::eShaderResource );

                builder.Read( geometryInfo.mGeometryBuffer, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mSkinningBuffer, FGResourceState::eShaderResource );

                builder.Read( geometryInfo.mVerticesBuffer, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mIndicesBuffer, FGResourceState::eShaderResource );
            },

            [this]( CommandContext& ctx, Blackboard& b ) -> void {
                PrepassModuleInfo& prepassInfo{ b.Get<PrepassModuleInfo>() };
                CameraModuleInfo& cameraPassInfo{ b.Get<CameraModuleInfo>() };
                GeometryManagementModuleInfo& geometryInfo{ b.Get<GeometryManagementModuleInfo>() };

                struct DrawParams {
                    u32 mGeometryInfoBufferID{};
                    u32 mSkinningInfoBufferID{};

                    u32 mIndexID{};
                    u32 mVertexID{};

                    u32 mCameraInfoBufferID{};
                } params{
                    .mGeometryInfoBufferID = ctx.PushBuffer_SRV( geometryInfo.mGeometryBuffer ),
                    .mSkinningInfoBufferID = ctx.PushBuffer_SRV( geometryInfo.mSkinningBuffer ),

                    .mIndexID = ctx.PushBuffer_SRV( geometryInfo.mIndicesBuffer ),
                    .mVertexID = ctx.PushBuffer_SRV( geometryInfo.mVerticesBuffer ),

                    .mCameraInfoBufferID = ctx.PushBuffer_SRV( cameraPassInfo.mCameraData ),
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prepassInfo.mDepthPrepassDepthTarget, LoadOp::eClear )
                    .AddRenderTarget( prepassInfo.mDepthPrepassColorTarget, kColorWhite, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( prepassInfo.mDepthPrepassPipeline );

                mGeometryManagement->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );
    }
}
