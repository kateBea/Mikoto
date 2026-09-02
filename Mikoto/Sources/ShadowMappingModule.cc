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

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/ShadowMappingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::scene;

    ShadowMappingModule::ShadowMappingModule( RenderResolution resolution )
        : mResolution{ resolution } {}

    auto ShadowMappingModule::SetScene( const Scene *scene ) -> void {
        mScene = scene;
    }

    auto ShadowMappingModule::SetCamera( const Camera *camera ) -> void {
        mCamera = camera;
    }

    auto ShadowMappingModule::SetGeometryManager( GeometryCullModule &culling ) -> void {
        mGeometryManagement = MKT_ADDRESSOF( culling );
    }

    auto ShadowMappingModule::SetShadowMapsResolution( rhi::RenderResolution resolution ) -> void {

    }

    auto ShadowMappingModule::RegisterPasses( FrameGraph &graph ) -> void {
        auto& info{ graph.GetOrCreate<ShadowMapInfo>() };

        auto shadowMapDimensions{ InferDimensions( mResolution ) };

        // Create directional shadow map textures
        for (u32 count{}; count < kMaxShadowMaps; ++count) {
            auto depthImage{ FGTextureDescription{}
                .SetName( string::Format( "DirDepthImage_{}", count ) )
                .SetWidth( as<i32>( shadowMapDimensions.first ) )
                .SetHeight( as<i32>( shadowMapDimensions.first ) ) // Same width and height
                .SetDimensions( TextureDimension::eTexture2D )
                .SetMultisampling( Multisampling::eMsaaX1 )
                .SetUsage( TextureUsageFlagsBits::kDepthTarget | TextureUsageFlagsBits::kShaderResource )
                .SetFormat( Format::eD32 ) };

            info.mDirShadowMaps.emplace_back( graph.Create( depthImage ) );
        }

        // Depth images for point light shadows. For these we use cube images
        // as point lights use omnidirectional shadow mapping (they emit shadows
        // in all directions surrounding the light source
        for (u32 count{}; count < kMaxShadowMaps; ++count) {
            auto depthImage{ FGTextureDescription{}
                .SetName( string::Format( "PointDepthImage_{}", count ) )
                .SetWidth( as<i32>( shadowMapDimensions.first ) )
                .SetHeight( as<i32>( shadowMapDimensions.first ) ) // Same width and height
                .SetDimensions( TextureDimension::eTextureCube )
                .SetMultisampling( Multisampling::eMsaaX1 )
                .SetUsage( TextureUsageFlagsBits::kDepthTarget | TextureUsageFlagsBits::kShaderResource )
                .SetFormat( Format::eD32 ) };

            info.mPointShadowMaps.emplace_back( graph.Create( depthImage ) );
        }

        // Depth images for spotlight shadows, spotlights like directional lights
        // also use directional shadow mapping.
        for (u32 count{}; count < kMaxShadowMaps; ++count) {
            auto depthImage{ FGTextureDescription{}
                .SetName( string::Format( "SpotDepthImage_{}", count ) )
                .SetWidth( as<i32>( shadowMapDimensions.first ) )
                .SetHeight( as<i32>( shadowMapDimensions.first ) ) // Same width and height
                .SetDimensions( TextureDimension::eTexture2D ) // Spot works similar to directional lights
                .SetMultisampling( Multisampling::eMsaaX1 )
                .SetUsage( TextureUsageFlagsBits::kDepthTarget | TextureUsageFlagsBits::kShaderResource )
                .SetFormat( Format::eD32 ) };

            info.mSpotShadowMaps.emplace_back( graph.Create( depthImage ) );
        }

        // These would ideally render and update the shadow maps
        // for dynamic shadow casters in a shadow texture atlas
        // TODO: investigate. Start by first integrating shadows for one dir light and proceed with atlas integration
        // https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
        RegisterDirShadowMap( graph );
        RegisterSpotShadowMap( graph );
        RegisterPointShadowMap( graph );
    }

    auto ShadowMappingModule::RegisterDirShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<ShadowMapInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "DirectionalShadowMap_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .SetDepthFormat( Format::eD32 )
            .PushShader( "DirLightShadows_Vert.slang", FGStageType::eVertex ) };
        info.mDirShadowMapPipeline = graph.Create( pipelineBuilder );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "DirShadowMap_Sampler01" )
            .SetFilter( SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };
        info.mDirShadowSampler = graph.Create( samplerDes );

        auto bufferDesc{ FGBufferDescription{}
            .SetName( "DirShadowsBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxShadowMaps, MKT_SIZEOF( ShadowMapParameters ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        info.mDirShadowsBuffer = graph.Create( bufferDesc );

        graph.RegisterPass(
            "DirectionalShadowMapPass_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) -> void {
                const auto& shadowMapData{ blackboard.Get<ShadowMapInfo>() };
                builder.UseResource( shadowMapData.mDirShadowsBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard& b ) -> void {
                auto& shadowMapData{ b.Get<ShadowMapInfo>() };

                // Reset to start counting or avoid having ghost shadow casters
                shadowMapData.mDirShadowCasterCount = 0;

                auto &registry{ mScene->GetRegistry() };
                auto lights{ registry.view<TransformComponent, LightComponent>() };
                if ( lights.size_hint() == 0 ) {
                    return;
                }

                eastl::array<ShadowMapParameters, kMaxShadowMaps> maps{};

                for ( const auto &light: lights ) {
                    auto &lightComp{ registry.get<LightComponent>( light ) };
                    auto &transformComp{ registry.get<TransformComponent>( light ) };

                    if ( lightComp.IsTypeActive( LightType::eDirectional ) ) {
                        auto& lightInfo{ lightComp.Get<DirectionalLight>() };
                        if (!lightInfo.IsShadowCaster()) {
                            continue;
                        }

                        const float zNear{ mCamera->GetNearPlane() };
                        const float zFar{ mCamera->GetFarPlane() };
                        const float orthoSize{ 1000 /*dimensions.first*/ };

                        // Projection
                        maps[shadowMapData.mDirShadowCasterCount].mProjection =
                                glm::ortho(
                                        -orthoSize,
                                        orthoSize,
                                        -orthoSize,
                                        orthoSize,
                                        zNear,
                                        zFar );

                        // View
                        const float3 lightDir{ lightComp.Get<DirectionalLight>().GetDirection() };
                        const float3 lightPos{ -lightDir * 500.0f };

                        maps[shadowMapData.mDirShadowCasterCount].mView =
                                glm::lookAt(
                                        lightPos,
                                        glm::vec3{ 0.0f, 0.0f, 0.0f },
                                        glm::vec3{ 0.0f, 1.0f, 0.0f } );

                        maps[shadowMapData.mDirShadowCasterCount].mLightViewProjection =
                            maps[shadowMapData.mDirShadowCasterCount].mProjection * maps[shadowMapData.mDirShadowCasterCount].mView;

                        maps[shadowMapData.mDirShadowCasterCount].mCasterMapID =
                            ctx.PushTexture_SRV( shadowMapData.mDirShadowMaps[shadowMapData.mDirShadowCasterCount] );

                        ++shadowMapData.mDirShadowCasterCount;
                    }
                }

                if (shadowMapData.mDirShadowCasterCount != 0) {
                    ctx.CopyBuffer( shadowMapData.mDirShadowsBuffer, 0, maps.data(),
                        shadowMapData.mDirShadowCasterCount * MKT_SIZEOF( ShadowMapParameters ) );
                }
            });

        graph.RegisterPass(
            "DirectionalShadowMapPass_Graphics",
            FGPassType::eGraphics,
        []( FGNodeBuilder& builder, Blackboard& blackboard ) {
            const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
            const auto& cameraPassData{ blackboard.Get<CameraModuleInfo>() };
            const auto& shadowMapData{ blackboard.Get<ShadowMapInfo>() };

            for (const auto& map : shadowMapData.mDirShadowMaps) {
                if (map.mHandle != 0) {
                    builder.UseResource( map, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                }
            }

            builder.UseResource( shadowMapData.mDirShadowsBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

            builder.UseResource( cameraPassData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

            builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

            builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

            builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
        },
        [this]( CommandContext &ctx, Blackboard& b ) -> void {
            auto &registry{ mScene->GetRegistry() };
            auto lights{ registry.view<TransformComponent, LightComponent>() };
            if ( lights.size_hint() == 0 ) {
                return;
            }

            auto& shadowMapData{ b.Get<ShadowMapInfo>() };
            const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };

            // I already know shadow caster count from previous pass
            for (u32 count{}; count < shadowMapData.mDirShadowCasterCount; ++count) {
                FGTextureHandle shadowMapHandle{ shadowMapData.mDirShadowMaps[count] };

                    struct DrawParams {
                        SPointer mGeometryInfoBuffer{};
                        SPointer mSkinningInfoBuffer{};

                        SPointer mGeometryAllocBuffer{};

                        SPointer mDirShadowsBuffer{};

                        u32 mShadowCasterID{};
                    } params{
                        .mGeometryInfoBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                        .mSkinningInfoBuffer = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),

                        .mGeometryAllocBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                        .mDirShadowsBuffer = ctx.GetDeviceBufferAddress( shadowMapData.mDirShadowsBuffer ),
                        .mShadowCasterID = count };
                    ctx.PushConstants( params );

                    const auto dimensions{ InferDimensions( mResolution ) };

                    // Shadow maps have width == height
                    const auto graphicsState{ ContextRenderState{}
                        .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.first ) } )
                        .AddDepthTarget( shadowMapHandle, LoadOp::eClear )};
                    ctx.BeginRender( graphicsState );

                    ctx.SetViewportState( ViewportState{}
                        .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.first ) ) ) );

                    ctx.BindPipeline( shadowMapData.mDirShadowMapPipeline );

                    mGeometryManagement->DrawInstancesIndirect( ctx );

                    ctx.EndRender();
                }
        } );
    }

    auto ShadowMappingModule::RegisterPointShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<ShadowMapInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "PointShadowMap_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .SetDepthFormat( Format::eD32 )
            .PushShader( "PointShadows_Vert.slang", FGStageType::eVertex ) };
        info.mPointShadowMapPipeline = graph.Create( pipelineBuilder );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "PointShadowMap_Sampler01" )
            .SetFilter( SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };
        info.mPointShadowSampler = graph.Create( samplerDes );

        auto bufferDesc{ FGBufferDescription{}
            .SetName( "PointShadowsBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxShadowMaps, MKT_SIZEOF( ShadowMapParameters ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        info.mPointShadowsBuffer = graph.Create( bufferDesc );

        graph.RegisterPass(
            "PointLightShadowMapPass_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) -> void {
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& cameraPassData{ blackboard.Get<CameraModuleInfo>() };
                const auto& shadowMapData{ blackboard.Get<ShadowMapInfo>() };

                for (const auto& map : shadowMapData.mPointShadowMaps) {
                    if (map.mHandle != 0) {
                        builder.UseResource( map, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                    }
                }

                builder.UseResource( shadowMapData.mPointShadowsBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( cameraPassData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            },
            []( CommandContext& ctx, Blackboard& b ) -> void {
        } );
    }

    auto ShadowMappingModule::RegisterSpotShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<ShadowMapInfo>() };

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SpotShadowMap_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .SetDepthFormat( Format::eD32 )
            .PushShader( "SpotShadows_Vert.slang", FGStageType::eVertex ) };
        info.mSpotShadowMapPipeline = graph.Create( pipelineBuilder );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "SpotShadowMap_Sampler01" )
            .SetFilter( SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };
        info.mSpotShadowSampler = graph.Create( samplerDes );

        auto bufferDesc{ FGBufferDescription{}
            .SetName( "SpotShadowsBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxShadowMaps, MKT_SIZEOF( ShadowMapParameters ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        info.mSpotShadowsBuffer = graph.Create( bufferDesc );

        graph.RegisterPass(
            "SpotLightShadowMapPass_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) -> void {
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& cameraPassData{ blackboard.Get<CameraModuleInfo>() };
                const auto& shadowMapData{ blackboard.Get<ShadowMapInfo>() };

                for (const auto& map : shadowMapData.mSpotShadowMaps) {
                    if (map.mHandle != 0) {
                        builder.UseResource( map, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );
                    }
                }

                builder.UseResource( shadowMapData.mSpotShadowsBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( cameraPassData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
            },
            []( CommandContext& ctx, Blackboard& b ) -> void {
            } );
    }
}
