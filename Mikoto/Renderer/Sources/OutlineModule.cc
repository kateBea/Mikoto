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
#include <cstddef>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Profiler.hh>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/OutlineModule.hh>
#include <Renderer/ForwardRenderer/Passes/MousePickingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>

namespace mikoto::renderer {

    // Only distances inside width + the one-pixel antialiasing fringe matter.
    static auto FirstOutlineJump( core::f32 width ) -> core::u32 {
        const auto extent{ core::as<core::u32>( std::ceil( width + 1.0f ) ) };
        core::u32 power{ 1 };

        while ( power < extent ) {
            power *= 2;
        }

        return eastl::max( 1u, power / 2 );
    }

    static auto CreateOutlineTexture( FrameGraph& graph, eastl::string_view name,
        rhi::RenderResolution resolution, rhi::Format format, rhi::TextureUsageFlags usage,
        rhi::Multisampling samples = rhi::Multisampling::eMsaaX1 ) -> FGTextureHandle {
        const auto [width, height]{ rhi::InferDimensions( resolution ) };
        return graph.Create( FGTextureDescription{}.SetName( name ).SetWidth( core::as<core::u32>( width ) ).SetHeight( core::as<core::u32>( height ) ).SetFormat( format ).SetUsage( usage ).SetMultisampling( samples ) );
    }

    static auto CreateOutlinePipeline( FrameGraph& graph, eastl::string_view name,
        const char* fragment, rhi::Format format,
        rhi::Multisampling samples = rhi::Multisampling::eMsaaX1,
        bool blend = false, const char* vertex = "FullQuad_Vert.slang" ) -> FGPipelineHandle {
        return graph.Create( FGPipelineDescription{}.SetName( name ).SetPipelineType( rhi::PipelineType::eGraphics ).SetTopology( rhi::PrimitiveTopology::eTriangleList ).SetCullMode( rhi::CullMode::eNone ).SetDepthTest( false ).SetDepthWrite( false ).SetBlendEnable( blend ).SetMultisampling( samples ).AddColorFormat( format ).PushShader( vertex, FGStageType::eVertex ).PushShader( fragment, FGStageType::ePixel ) );
    }

    static auto BeginOutlineRender( CommandContext& ctx, FGTextureHandle target,
        FGPipelineHandle pipeline, rhi::RenderResolution resolution, rhi::Color clear,
        rhi::LoadOp load = rhi::LoadOp::eClear ) -> void {
        const auto [width, height]{ rhi::InferDimensions( resolution ) };
        ctx.BeginRender( RenderBeginDescription{}
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } )
            .AddRenderTarget( target, clear, load ) );
        ctx.SetViewportState( rhi::ViewportState{}.AddViewportAndScissorRect(
            rhi::Viewport( core::as<core::f32>( width ), core::as<core::f32>( height ) ) ) );
        ctx.BindPipeline( pipeline );
    }

    OutlineModule::OutlineModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto OutlineModule::SetGeometryManager( GeometryCullModule& geometry ) -> void {
        if ( mGeometry != &geometry ) {
            mGeometry = &geometry;
            BumpRevision();
        }
    }

    auto OutlineModule::SetEnabled( bool enabled ) -> void {
        if ( mEnabled != enabled ) {
            mEnabled = enabled;
            BumpRevision();
        }
    }

    auto OutlineModule::SetStyle( const core::float4& color, core::f32 width ) -> void {
        const auto sanitizedColor{ glm::clamp( color, core::float4{ 0 }, core::float4{ 1 } ) };
        const auto sanitizedWidth{ glm::clamp( width, 1.0f, kMaxOutlineWidth ) };

        if ( mColor != sanitizedColor || mWidth != sanitizedWidth ) {
            mColor = sanitizedColor;
            mWidth = sanitizedWidth;
            BumpRevision();
        }
    }

    auto OutlineModule::RegisterHighlightFilter( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        auto& info{ graph.GetOrCreate<OutlineModuleInfo>() };
        info.mHighlightedDraws = graph.Create( FGBufferDescription{}.SetName( "Outline_HighlightedDraws" ).SetUsage( rhi::BufferUsageFlagsBits::IndirectDraw | rhi::BufferUsageFlagsBits::CopySource | rhi::BufferUsageFlagsBits::CopyDest ).SetSizeBytes( 16 * sizeof( DrawIndirectCommand ) ) );
        graph.RegisterPass(
            "Outline_HighlightPrepare",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                // GeometryFilter refreshes HighlightComponent membership and packed instance IDs.
                // Reading its commands orders this CPU filtering callback after that producer.
                builder.UseResource( b.Get<GeometryCullModuleInfo>().mUnculledIndirectBuffer,
                    FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
                builder.UseResource( b.Get<OutlineModuleInfo>().mHighlightedDraws,
                    FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                mUploads.Clear();
                auto& outline{ b.Get<OutlineModuleInfo>() };

                if ( !IsRevision( outline.mSettingsRevision ) ) {
                    outline.mColor = mColor;
                    outline.mWidth = mWidth;
                    outline.mFirstJumpStep = FirstOutlineJump( mWidth );
                    outline.mSettingsRevision = GetRevision();
                }

                mHighlightedDraws.clear();

                if ( mEnabled && mGeometry && mColor.a > 0 ) {
                    mGeometry->AppendHighlightedDraws( mHighlightedDraws );
                }

                outline.mDrawCount = core::as<core::u32>( mHighlightedDraws.size() );
                outline.mEnabled = outline.mDrawCount != 0;

                if ( !outline.mEnabled ) {
                    return;
                }

                const auto bytes{ mHighlightedDraws.size() * sizeof( DrawIndirectCommand ) };
                ctx.EnsureBufferCapacity( outline.mHighlightedDraws, bytes );
                mUploads.CopyBuffer( outline.mHighlightedDraws, 0, mHighlightedDraws.data(), bytes );
            } );
        graph.RegisterPass(
            "Outline_HighlightFilter",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<OutlineModuleInfo>().mHighlightedDraws, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& ) {
                mUploads.Submit( ctx );
            } );
    }

    auto OutlineModule::RegisterInteriorMask( FrameGraph& graph, rhi::Multisampling samples ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        auto& info{ graph.GetOrCreate<OutlineModuleInfo>() };
        // Use the HDR format covered by the renderer's device sample-count check.
        // White covered samples also supply the BUFFERFILL silhouette; no second mesh draw is needed.
        info.mInteriorMask = CreateOutlineTexture( graph, "Outline_InteriorMask", mResolution,
            rhi::Format::eRGBA16_FLOAT, rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::CopySource, samples );
        const auto pipeline{ CreateOutlinePipeline( graph, "Outline_InteriorPipeline",
            "OutlineSilhouette_Frag.slang", rhi::Format::eRGBA16_FLOAT, samples, false,
            "OutlineSilhouette_Vert.slang" ) };
        graph.RegisterPass(
            "Outline_InteriorMask",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& geometry{ b.Get<GeometryCullModuleInfo>() };
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData,
                    FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                for ( const auto buffer: { geometry.mGeometryBuffer, geometry.mInstanceBuffer,
                          geometry.mDrawInstanceBuffer, geometry.mSkinningBuffer, geometry.mSkinningCacheBuffer,
                          geometry.mGeometryAllocBuffer, geometry.mAttributeBuffer, geometry.mIndexBuffer } ) {
                    builder.UseResource( buffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                }

                builder.UseResource( geometry.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<OutlineModuleInfo>().mHighlightedDraws,
                    FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );
                builder.UseResource( b.Get<OutlineModuleInfo>().mInteriorMask,
                    FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& outline{ b.Get<OutlineModuleInfo>() };

                if ( !outline.mEnabled ) {
                    return;
                }

                const auto& geometry{ b.Get<GeometryCullModuleInfo>() };
                struct Parameters {
                    core::u64 mGeometry{}, mSkinning{}, mAllocation{}, mCamera{}, mMaterials{};
                    core::u32 mSampler{};
                } params{
                    ctx.GetDeviceBufferAddress( geometry.mGeometryBuffer ),
                    ctx.GetDeviceBufferAddress( geometry.mSkinningBuffer ),
                    ctx.GetDeviceBufferAddress( geometry.mGeometryAllocBuffer ),
                    ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                    ctx.GetDeviceBufferAddress( geometry.mMaterialsBuffer ),
                    ctx.PushSampler( geometry.mMaterialSampler )
                };
                static_assert( offsetof( Parameters, mSampler ) == 40 );
                ctx.PushConstants( params );
                BeginOutlineRender( ctx, outline.mInteriorMask, pipeline, mResolution, rhi::Color{ 0.0f } );
                ctx.DrawIndirect( DrawIndirectState{}.SetBuffer( outline.mHighlightedDraws ).SetDrawCount( outline.mDrawCount ) );
                ctx.EndRender();
            } );
    }

    auto OutlineModule::RegisterSilhouette( FrameGraph& graph, rhi::Multisampling samples ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        auto& info{ graph.GetOrCreate<OutlineModuleInfo>() };

        if ( samples == rhi::Multisampling::eMsaaX1 ) {
            info.mSilhouette = info.mInteriorMask;
            return;
        }

        info.mSilhouette = CreateOutlineTexture( graph, "Outline_Silhouette", mResolution,
            rhi::Format::eRGBA16_FLOAT, rhi::TextureUsageFlagsBits::CopyDest | rhi::TextureUsageFlagsBits::ShaderResource );
        const auto source{ info.mInteriorMask }, target{ info.mSilhouette };
        graph.RegisterPass(
            "Outline_SilhouetteResolve",
            FGPassType::eGraphics,
            [source, target]( FGNodeBuilder& builder, core::Blackboard& ) {
                builder.UseResource( source, FGPipelineStage::eResolve, FGResourceAccess::eRead );
                builder.UseResource( target, FGPipelineStage::eResolve, FGResourceAccess::eWrite );
            },
            [source, target]( CommandContext& ctx, core::Blackboard& b ) {
                if ( b.Get<OutlineModuleInfo>().mEnabled ) {
                    ctx.ResolveTexture( source, target );
                }
            } );
    }

    auto OutlineModule::RegisterSeedInit( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        auto& info{ graph.GetOrCreate<OutlineModuleInfo>() };
        info.mSeeds[0] = CreateOutlineTexture( graph, "Outline_Seeds0", mResolution, rhi::Format::eRG16_SNORM,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource );
        const auto pipeline{ CreateOutlinePipeline( graph, "Outline_SeedPipeline", "OutlineSeed_Frag.slang", rhi::Format::eRG16_SNORM ) };
        graph.RegisterPass(
            "Outline_SeedInit",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& outline{ b.Get<OutlineModuleInfo>() };
                builder.UseResource( outline.mSilhouette, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( outline.mSeeds[0], FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& outline{ b.Get<OutlineModuleInfo>() };

                if ( !outline.mEnabled ) {
                    return;
                }

                struct Parameters {
                    core::u32 mSilhouette;
                } params{ ctx.PushTexture_SRV( outline.mSilhouette ) };
                ctx.PushConstants( params );
                BeginOutlineRender( ctx, outline.mSeeds[0], pipeline, mResolution, rhi::Color{ -1.0f } );
                ctx.Draw( 3 );
                ctx.EndRender();
            } );
    }

    auto OutlineModule::RegisterJumpFlood( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        auto& info{ graph.GetOrCreate<OutlineModuleInfo>() };
        info.mSeeds[1] = CreateOutlineTexture( graph, "Outline_Seeds1", mResolution, rhi::Format::eRG16_SNORM,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource );
        const auto pipeline{ CreateOutlinePipeline( graph, "Outline_JumpPipeline", "OutlineJumpFlood_Frag.slang", rhi::Format::eRG16_SNORM ) };

        for ( core::u32 step{ FirstOutlineJump( kMaxOutlineWidth ) }; step; step /= 2 ) {
            for ( core::u32 axis{}; axis < 2; ++axis ) {
                const auto input{ info.mSeeds[axis] }, output{ info.mSeeds[1 - axis] };
                graph.RegisterPass(
                    string::Format( "Outline_JFA_{}_Step{}", axis == 0 ? "X" : "Y", step ),
                    FGPassType::eGraphics,
                    [input, output]( FGNodeBuilder& builder, core::Blackboard& ) {
                        builder.UseResource( input, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                        builder.UseResource( output, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                    },
                    [this, pipeline, input, output, step, axis]( CommandContext& ctx, core::Blackboard& b ) {
                        const auto& outline{ b.Get<OutlineModuleInfo>() };

                        if ( !outline.mEnabled || step > outline.mFirstJumpStep ) {
                            return;
                        }

                        struct Parameters {
                            core::u32 mSeeds, mStep, mAxis, mPadding;
                        } params{ ctx.PushTexture_SRV( input ), step, axis, 0 };
                        ctx.PushConstants( params );
                        BeginOutlineRender( ctx, output, pipeline, mResolution, rhi::Color{ -1.0f } );
                        ctx.Draw( 3 );
                        ctx.EndRender();
                    } );
            }
        }
        // Every executed X/Y pair ends in seed 0. Skipping larger pairs cannot change parity.
    }

    auto OutlineModule::RegisterOutline( FrameGraph& graph, rhi::Multisampling samples ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        auto& info{ graph.GetOrCreate<OutlineModuleInfo>() };
        info.mResolvedColor = CreateOutlineTexture( graph, "Outline_Color", mResolution, rhi::Format::eRGBA16_FLOAT,
            rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::CopyDest | rhi::TextureUsageFlagsBits::ShaderResource );
        const bool multisampled{ samples != rhi::Multisampling::eMsaaX1 };
        const auto target{ multisampled ? CreateOutlineTexture( graph, "Outline_MSAA_Color", mResolution,
                                              rhi::Format::eRGBA16_FLOAT, rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::CopySource, samples )
                                        : info.mResolvedColor };
        const auto pipeline{ CreateOutlinePipeline( graph, "Outline_ColorPipeline",
            multisampled ? "OutlineCompositeMSAA_Frag.slang" : "OutlineComposite_Frag.slang", rhi::Format::eRGBA16_FLOAT, samples ) };
        graph.RegisterPass(
            "Outline_Render",
            FGPassType::eGraphics,
            [target]( FGNodeBuilder& builder, core::Blackboard& b ) {
                const auto& outline{ b.Get<OutlineModuleInfo>() };
                builder.UseResource( outline.mSeeds[0], FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( outline.mInteriorMask, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( target, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline, target]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& outline{ b.Get<OutlineModuleInfo>() };

                if ( !outline.mEnabled ) {
                    return;
                }

                struct Parameters {
                    core::float4 mColor;
                    core::u32 mSeeds;
                    core::f32 mWidth;
                    core::u32 mInterior;
                    core::u32 mPadding;
                } params{ outline.mColor, ctx.PushTexture_SRV( outline.mSeeds[0] ), outline.mWidth,
                    ctx.PushTexture_SRV( outline.mInteriorMask ), 0 };
                static_assert( offsetof( Parameters, mSeeds ) == 16 );
                ctx.PushConstants( params );
                // Kept separate from scene color so atmosphere cannot erase background outlines.
                BeginOutlineRender( ctx, target, pipeline, mResolution, rhi::kColorTransparent );
                ctx.Draw( 3 );
                ctx.EndRender();
            } );

        if ( multisampled ) {
            const auto resolved{ info.mResolvedColor };
            graph.RegisterPass(
                "Outline_ColorResolve",
                FGPassType::eGraphics,
                [target, resolved]( FGNodeBuilder& builder, core::Blackboard& ) {
                    builder.UseResource( target, FGPipelineStage::eResolve, FGResourceAccess::eRead );
                    builder.UseResource( resolved, FGPipelineStage::eResolve, FGResourceAccess::eWrite );
                },
                [target, resolved]( CommandContext& ctx, core::Blackboard& b ) {
                    if ( b.Get<OutlineModuleInfo>().mEnabled ) {
                        ctx.ResolveTexture( target, resolved );
                    }
                } );
        }
    }

    auto OutlineModule::RegisterComposite( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        const auto pipeline{ CreateOutlinePipeline( graph, "Outline_OverlayPipeline", "OutlineOverlay_Frag.slang",
            rhi::Format::eBGRA8_UNORM, rhi::Multisampling::eMsaaX1, true ) };
        graph.RegisterPass(
            "Outline_Composite",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<MousePickingModuleInfo>().mColorImage, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<OutlineModuleInfo>().mResolvedColor, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PresentationPassData>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& outline{ b.Get<OutlineModuleInfo>() };

                if ( !outline.mEnabled && !mTextureDropHover ) {
                    return;
                }

                struct Parameters {
                    core::u32 mOutline, mIDs, mHover, mShowOutline;
                } params{ ctx.PushTexture_SRV( outline.mResolvedColor ),
                    ctx.PushTexture_SRV( b.Get<MousePickingModuleInfo>().mColorImage ), mTextureDropHover, outline.mEnabled ? 1u : 0u };
                ctx.PushConstants( params );
                BeginOutlineRender( ctx, b.Get<PresentationPassData>().mColorImage, pipeline, mResolution,
                    rhi::Color{}, rhi::LoadOp::eLoad );
                ctx.Draw( 3 );
                ctx.EndRender();
            } );
    }
}// namespace mikoto::renderer
