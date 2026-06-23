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
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/ShadowMappingModule.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

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
        mGeometryManager = MKT_ADDRESSOF( culling );
    }

    auto ShadowMappingModule::RegisterPasses( FrameGraph &graph ) -> void {
        ShadowMapInfo& info{ graph.GetOrCreate<ShadowMapInfo>() };

        // This buffer contains a list of structures that hold
        // all parameters to render handle shadow map for specific light type
        auto bufferDesc{ FGBufferDescription{}
            .SetName( "ShadowsBuffer01" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( kMaxShadowMaps * 3, MKT_SIZEOF( ShadowMapParameters ) )
            .SetHeapType( HeapType::eDeviceLocal )};

        info.mShadowsBuffer = graph.Create( bufferDesc );

        // Create shadow maps
        for (u32 count{}; count < kMaxShadowMaps; ++count) {
            // Depth images for directional light shadows
            auto depthImage{ FGTextureDescription{}
                .SetName( string::Format( "DirDepthImage_{}", count ) )
                .SetWidth( as<i32>( 1980 ) )
                .SetHeight( as<i32>( 1080 ) )
                .SetDimensions( TextureDimension::eTexture2D )
                .SetMultisampling( Multisampling::eMsaaX1 )
                .SetUsage( TextureUsageFlagsBits::kDepthTarget | TextureUsageFlagsBits::kShaderResource )
                .SetFormat( Format::eD32 ) };

            info.mDirShadowMaps.emplace_back( graph.Create( depthImage ) );

            // Depth images for point light shadows
            // Depth images for spotlight shadows
        }


        // These would ideally render and update the shadow maps
        // for dynamic shadow casters in a shadow texture atlas
        // TODO: investigate. Start by first integrating shadows for one dir light and proceed with atlas integration
        //https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
        RegisterDirShadowMap( graph );
        RegisterSpotShadowMap( graph );
        RegisterPointShadowMap( graph );
    }

    auto ShadowMappingModule::RegisterDirShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ShadowMapInfo& info{ graph.GetOrCreate<ShadowMapInfo>() };
        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "DirectionalShadowMap_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .SetDepthFormat( Format::eD32 )
            .SetCullMode( CullMode::eCullBack )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .PushShader( "DirLightShadows_Vert.slang", FGStageType::eVertex )
            .PushShader( "DirLightShadows_Frag.slang", FGStageType::ePixel ) };

        info.mDirShadowMapPipeline = graph.Create( pipelineBuilder );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "DirShadowMap_Sampler01" )
            .SetFilter( SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };

        info.mDirShadowSampler = graph.Create( samplerDes );

        graph.RegisterPass(
            "DirectionalShadowMapPass",
            FGPassType::eGraphics,
        []( FGNodeBuilder&b, Blackboard& blackboard ) {
            const auto& geom{ blackboard.Get<GeometryCullModuleInfo>() };
            const auto& cam{ blackboard.Get<CameraModuleInfo>() };
            const auto& shadow{ blackboard.Get<ShadowMapInfo>() };

            for (const auto& map : shadow.mDirShadowMaps) {
                if (map.mHandle != 0) {
                    b.Write( map, FGResourceState::eDepthWrite );
                }
            }

            b.Read( cam.mCameraData, FGResourceState::eShaderResource );
            b.Read( geom.mGeometryBuffer, FGResourceState::eShaderResource );
            b.Read( geom.mSkinningBuffer, FGResourceState::eShaderResource );
            b.Read( shadow.mShadowsBuffer, FGResourceState::eShaderResource );
        },
        [this]( CommandContext &ctx, Blackboard& blackboard ) -> void {
            auto &registry{ mScene->GetRegistry() };
            auto lights{ registry.view<TransformComponent, LightComponent>() };
            if ( lights.size_hint() == 0 ) {
                return;
            }

            for ( const auto &light: lights ) {
                auto &lightComp{ registry.get<LightComponent>( light ) };

                if ( lightComp.IsTypeActive( LightType::eDirectional ) ) {

                }
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
                        b.Write( map, FGResourceState::eDepthWrite );
                    }
                }

                b.Read( cam.mCameraData, FGResourceState::eShaderResource );
                b.Read( geom.mGeometryBuffer, FGResourceState::eShaderResource );
                b.Read( geom.mSkinningBuffer, FGResourceState::eShaderResource );
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
                        b.Write( map, FGResourceState::eDepthWrite );
                    }
                }

                b.Read( cam.mCameraData, FGResourceState::eShaderResource );
                b.Read( geom.mGeometryBuffer, FGResourceState::eShaderResource );
                b.Read( geom.mSkinningBuffer, FGResourceState::eShaderResource );
            },
            []( CommandContext&, Blackboard& ) -> void {
            } );
    }
}
