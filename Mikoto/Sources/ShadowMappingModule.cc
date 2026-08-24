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
            // Depth images for directional light shadows
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

        // Depth images for point light shadows
        // Depth images for spotlight shadows


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
            .SetCullMode( CullMode::eCullBack )
            .PushShader( "DirLightShadows_Vert.slang", FGStageType::eVertex ) };
        info.mDirShadowMapPipeline = graph.Create( pipelineBuilder );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "DirShadowMap_Sampler01" )
            .SetFilter( SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };
        info.mDirShadowSampler = graph.Create( samplerDes );

        auto bufferDesc{ FGBufferDescription{}
            .SetName( "ShadowsBuffer01" )
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

            builder.UseResource( geometryData.mVerticesBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
            builder.UseResource( geometryData.mIndicesBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

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
                        u32 mGeometryInfoBufferID{};
                        u32 mSkinningInfoBufferID{};

                        u32 mIndicesBufferID{};
                        u32 mVerticesBufferID{};

                        u32 mShadowCasterID{};

                        u32 mDirectionalShadowsInfoBufferID{};
                    } params{
                        .mGeometryInfoBufferID = ctx.PushBuffer_SRV( geometryData.mGeometryBuffer ),
                        .mSkinningInfoBufferID = ctx.PushBuffer_SRV( geometryData.mSkinningBuffer ),

                        .mIndicesBufferID = ctx.PushBuffer_SRV( geometryData.mIndicesBuffer ),
                        .mVerticesBufferID = ctx.PushBuffer_SRV( geometryData.mVerticesBuffer ),

                        .mShadowCasterID = count,
                        .mDirectionalShadowsInfoBufferID = ctx.PushBuffer_SRV( shadowMapData.mDirShadowsBuffer ) };
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

        graph.RegisterPass(
            "PointLightShadowMapPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, Blackboard& blackboard ) -> void {
                const auto& geom{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& cam{ blackboard.Get<CameraModuleInfo>() };
                const auto& shadow{ blackboard.Get<ShadowMapInfo>() };

                for (const auto& map : shadow.mPointShadowMaps) {
                    if (map.mHandle != 0) {
                        b.Write( map, FGPipelineStage::eDepthTarget );
                    }
                }

                b.Read( cam.mCameraData, FGPipelineStage::ePixelShader );
                b.Read( geom.mGeometryBuffer, FGPipelineStage::ePixelShader );
                b.Read( geom.mSkinningBuffer, FGPipelineStage::ePixelShader );
            },
            []( CommandContext&, Blackboard& ) -> void {
        } );
    }

    auto ShadowMappingModule::RegisterSpotShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "SpotLightShadowMapPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder& b, Blackboard& blackboard ) -> void {
                const auto& geom{ blackboard.Get<GeometryCullModuleInfo>() };
                const auto& cam{ blackboard.Get<CameraModuleInfo>() };
                const auto& shadow{ blackboard.Get<ShadowMapInfo>() };

                for (const auto& map : shadow.mSpotShadowMaps) {
                    if (map.mHandle != 0) {
                        b.Write( map, FGPipelineStage::eDepthTarget );
                    }
                }

                b.Read( cam.mCameraData, FGPipelineStage::ePixelShader );
                b.Read( geom.mGeometryBuffer, FGPipelineStage::ePixelShader );
                b.Read( geom.mSkinningBuffer, FGPipelineStage::ePixelShader );
            },
            []( CommandContext&, Blackboard& ) -> void {
            } );
    }
}
