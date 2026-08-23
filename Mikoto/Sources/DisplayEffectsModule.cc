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

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/PostProcessModule.hh>
#include <Renderer/Passes/GeometryShadingModule.hh>

#include <Renderer/Passes/DisplayEffectsModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    DisplayEffectsModule::DisplayEffectsModule( rhi::RenderResolution resolution )
        : mResolution{ resolution }
    {
    }

    auto DisplayEffectsModule::SetScene( const scene::Scene *scene ) -> void {
        mScene = scene;
    }

    auto DisplayEffectsModule::SetCamera( const scene::Camera *camera ) -> void {
        mCamera = camera;
    }

    auto DisplayEffectsModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        mGeometryCullModule = MKT_ADDRESSOF( geom );
    }

    auto DisplayEffectsModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterDepthOfField( graph );

        RegisterFilmGrainPass( graph );
        RegisterLensDirtPass( graph );
        RegisterLensFlarePass( graph );
        RegisterVignettePass( graph );
        RegisterLensDistortionPass( graph );

        RegisterChromaticAberration( graph );
    }

    auto DisplayEffectsModule::RegisterDepthOfField( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Uses the scene main camera

        // https://photographylife.com/what-is-depth-of-field
        // https://dev.epicgames.com/documentation/en-us/unreal-engine/depth-of-field-in-unreal-engine
        // https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-28-practical-post-process-depth-field
        graph.RegisterPass(
            "DepthOfField_InitializeCoC",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "DepthOfField_ComputeNearCoC",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "DepthOfField_BlurNearCoC",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );

        graph.RegisterPass(
            "DepthOfField_DebugPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Green close blur
                // Black semi transparent not blurred
                // Blue far blur
            } );
    }

    auto DisplayEffectsModule::RegisterChromaticAberration( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<DisplayEffectsModuleInfo>() };

        const auto dimensions{ InferDimensions( mResolution ) };

        auto colorImage{ FGTextureDescription{}
            .SetName( "ChromaticAberration_ColorImage01" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eRGBA8_UNORM ) };
        info.mChromaAbRenderTarget = graph.Create( colorImage );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ChromaticAberration_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .AddColorFormat( Format::eRGBA8_UNORM )
            .SetCullMode( CullMode::eNone )
            .PushShader( "ChromaticAberration_Vert.slang", FGStageType::eVertex )
            .PushShader( "ChromaticAberration_Frag.slang", FGStageType::ePixel ) };
        info.mChromaAbPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ChromaticAberration",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, Blackboard& blackboard ) {
                const auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto& displayEffectsData{ blackboard.Get<DisplayEffectsModuleInfo>() };

                builder.UseResource( geometryData.mTonemapColor, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( displayEffectsData.mChromaAbRenderTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, Blackboard& blackboard ) {
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };

                const auto& geometryData{ blackboard.Get<GeomShadingModuleInfo>() };
                const auto& displayEffectsData{ blackboard.Get<DisplayEffectsModuleInfo>() };

                struct DrawParams {
                    u32 mSamplerID{};
                    u32 mTonemapImageID{};

                    u32 mCameraBufferID{};
                } params{
                    .mSamplerID = ctx.PushSampler( geometryData.mDefaultSampler ),
                    .mTonemapImageID = ctx.PushTexture_SRV( geometryData.mTonemapColor ),
                    .mCameraBufferID = ctx.PushBuffer_SRV( cameraData.mCameraData ) };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>(dimensions.first), as<i32>(dimensions.second) } )
                    .AddRenderTarget( displayEffectsData.mChromaAbRenderTarget, kColorCyan, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>(dimensions.first), as<f32>(dimensions.second) ) ) );

                ctx.BindPipeline( displayEffectsData.mChromaAbPipeline );

                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto DisplayEffectsModule::RegisterFilmGrainPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "FilmGrain",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard& ) {

            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );
    }

    auto DisplayEffectsModule::RegisterLensDirtPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "LensDirt",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard& ) {

            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );
    }

    auto DisplayEffectsModule::RegisterLensFlarePass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "LensFlare",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard& ) {

            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );
    }

    auto DisplayEffectsModule::RegisterVignettePass( FrameGraph& graph ) -> void {
        graph.RegisterPass(
            "Vignette",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard& ) {

            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );
    }

    auto DisplayEffectsModule::RegisterLensDistortionPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "LensDistortion",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard& ) {

            },
            []( CommandContext&, Blackboard& ) {
                // Nothing
            } );
    }
}// namespace mikoto::renderer