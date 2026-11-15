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

#include <EASTL/optional.h>
#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>

#include <Assets/Utility.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/ForwardRenderer/Passes/MousePickingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    auto MousePickingModule::MapPickingPixel( const ReadPixelViewportInfo& viewport,
        core::u32 width, core::u32 height ) -> eastl::optional<PickingPixel> {
        MKT_BEGIN_PROFILER();

        const float x = viewport.mX - viewport.mViewportX;
        const float y = viewport.mY - viewport.mViewportY;

        if ( width == 0 || height == 0 ||
             viewport.mViewportWidth <= 0 || viewport.mViewportHeight <= 0 || x < 0 || y < 0 ||
             x >= viewport.mViewportWidth || y >= viewport.mViewportHeight ) {
            return eastl::nullopt;
        }

        // The displayed image uses top-left UV coordinates, matching the selection attachment.
        return PickingPixel{ glm::min( width - 1, core::as<core::u32>( x / viewport.mViewportWidth * width ) ),
            glm::min( height - 1, core::as<core::u32>( y / viewport.mViewportHeight * height ) ) };
    }

    MousePickingModule::MousePickingModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto MousePickingModule::ReadPixel( core::u32 x, core::u32 y ) const -> core::u32 {
        MKT_BEGIN_PROFILER();

        if ( !mIsReadbackDone || mData.empty() ) {
            return 0;
        }

        // Assumes full width and height viewport
        // ReadPixel(x=2, y=1)
        // Array: [1 1 5 5 1 3 [3] 5 7 7 3 9]
        //                      ^
        // +----+----+----+----+
        // |  1 |  1 |  5 |  5 |
        // +----+----+----+----+
        // |  1 |  3 | [3]|  5 |
        // +----+----+----+----+
        // |  7 |  7 |  3 |  9 |
        // +----+----+----+----+
        auto dimensions{ InferDimensions( mResolution ) };

        if ( x >= dimensions.first || y >= dimensions.second ) {
            return 0;
        }

        core::u32 width{ core::as<core::u32>( dimensions.first ) };
        return mData[y * width + x];
    }

    auto MousePickingModule::ReadPixel( const ReadPixelViewportInfo& viewport ) const -> core::u32 {
        MKT_BEGIN_PROFILER();
        const auto dimensions{ InferDimensions( mResolution ) };
        const auto pixel{ MapPickingPixel( viewport, core::as<core::u32>( dimensions.first ), core::as<core::u32>( dimensions.second ) ) };

        return pixel ? ReadPixel( pixel->mX, pixel->mY ) : 0;
    }

    auto MousePickingModule::RequestPixel( const ReadPixelViewportInfo& viewport ) -> bool {
        MKT_BEGIN_PROFILER();
        const auto dimensions{ InferDimensions( mResolution ) };
        const auto pixel{ MapPickingPixel( viewport, core::as<core::u32>( dimensions.first ), core::as<core::u32>( dimensions.second ) ) };

        if ( !pixel ) {
            return false;
        }

        BumpRevision();
        mQueuedPick = pixel;
        mPickResult.reset();
        return true;
    }

    auto MousePickingModule::TakePickResult() -> eastl::optional<core::u32> {
        MKT_BEGIN_PROFILER();

        auto result = mPickResult;
        mPickResult.reset();
        
        return result;
    }

    auto MousePickingModule::ReadRectangle( const ReadPixelViewportInfo& first, const ReadPixelViewportInfo& last ) const -> eastl::vector<u32> {
        MKT_BEGIN_PROFILER();

        const auto [width, height]{ InferDimensions( mResolution ) };
        const auto a{ MapPickingPixel( first, as<u32>( width ), as<u32>( height ) ) };
        const auto b{ MapPickingPixel( last, as<u32>( width ), as<u32>( height ) ) };
        eastl::vector<u32> ids{};

        if ( !a || !b || !mIsReadbackDone || mData.empty() ) {
            return ids;
        }

        // The ID attachment's depth test leaves only the nearest surface at each pixel.
        // https://webgl2fundamentals.org/webgl/lessons/webgl-picking.html
        const u32 stride{ as<u32>( width ) };
        const u32 left{ glm::min( a->mX, b->mX ) };
        const u32 right{ glm::max( a->mX, b->mX ) };
        const u32 top{ glm::min( a->mY, b->mY ) };
        const u32 bottom{ glm::max( a->mY, b->mY ) };
        ankerl::unordered_dense::set<u32> seen{};

        // Scan the completed snapshot directly instead of profiling a point query per pixel.
        for ( u32 y{ top }; y <= bottom; ++y ) {
            const auto* row{ mData.data() + static_cast<usize>( y ) * stride };

            for ( u32 x{ left }; x <= right; ++x ) {
                const auto id{ row[x] };

                if ( id && seen.insert( id ).second ) {
                    ids.push_back( id );
                }
            }
        }

        eastl::sort( ids.begin(), ids.end() );
        return ids;
    }

    auto MousePickingModule::Invalidate() -> void {
        MKT_BEGIN_PROFILER();
        CancelPendingPick();
        mIsReadbackDone = false;
    }

    auto MousePickingModule::CancelPendingPick() -> void {
        MKT_BEGIN_PROFILER();

        BumpRevision();
        mQueuedPick.reset();
        mPickResult.reset();

        // An outstanding GPU copy must finish before this staging buffer is reused.
    }

    auto MousePickingModule::SetGeometryManager( GeometryCullModule& geom ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mGeometryCullModule != MKT_ADDRESSOF( geom ) ) {
            Invalidate();
            mGeometryCullModule = MKT_ADDRESSOF( geom );
        }
    }

    auto MousePickingModule::RegisterSelectionBuffer( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<MousePickingModuleInfo>() };
        auto dimensions{ InferDimensions( mResolution ) };

        renderer::rhi::Format format{ renderer::rhi::Format::eR32_UINT };
        auto imageDesc{ FGTextureDescription{}
            .SetName( "ObjectSelection_Color" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | 
                renderer::rhi::TextureUsageFlagsBits::ShaderResource | 
                renderer::rhi::TextureUsageFlagsBits::CopySource )
            .SetFormat( format ) };
        info.mColorImage = graph.Create( imageDesc );

        // Selection includes translucent surfaces, which do not all write the
        // scene prepass. Resolve their nearest visible layer independently.
        info.mDepthImage = graph.Create( FGTextureDescription{}
            .SetName( "ObjectSelection_Depth" )
            .SetWidth( core::as<core::i32>( dimensions.first ) )
            .SetHeight( core::as<core::i32>( dimensions.second ) )
            .SetFormat( renderer::rhi::Format::eD32 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget ) );

        const auto& formatInfo{ rhi::GetFormatInfo( format ) };
        auto bufferDesc{ FGBufferDescription{}
            .SetName( "ObjectSelection_ReadbackBuffer" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | 
                renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( dimensions.first * dimensions.second * formatInfo.mBytesPerBlock )
            .SetHeapType( renderer::rhi::HeapType::eReadback ) };
        info.mReadBackBuffer = graph.Create( bufferDesc );

        core::usize totalBytes{ ( core::usize )( dimensions.first * dimensions.second * formatInfo.mBytesPerBlock ) };
        mData.resize( totalBytes / sizeof( core::u32 ) );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ObjectSelection_Pipeline" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetBlendEnable( false )
            .SetDepthWrite( true )
            .SetDepthTest( true )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eR32_UINT )
            .PushShader( "MousePick_Vert.slang", FGStageType::eVertex )
            .PushShader( "MousePick_Frag.slang", FGStageType::ePixel ) };
        info.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ObjectSelection_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& mousePickingData{ blackboard.Get<MousePickingModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };

                builder.UseResource( cameraData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mDrawInstanceBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningCacheBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mAttributeBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mIndexBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mMaterialsBuffer, FGPipelineStage::ePixelShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mIndirectBuffer, FGPipelineStage::eIndirectArgument, FGResourceAccess::eRead );

                builder.UseResource( mousePickingData.mDepthImage, FGPipelineStage::eDepthTarget, FGResourceAccess::eWrite );

                builder.UseResource( mousePickingData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                const auto& mousePickingData{ b.Get<MousePickingModuleInfo>() };
                const auto& cameraPassData{ b.Get<CameraModuleInfo>() };
                const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };

                struct DrawParams {
                    SPointer mGeometryBuffer{};
                    SPointer mSkinningBuffer{};
                    SPointer mGeometryAllocationBufferID{};

                    SPointer mCameraBuffer{};
                    SPointer mMaterialsBuffer{};
                    core::u32 mBasicSamplerID{};
                } params{
                    .mGeometryBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mSkinningBuffer = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),

                    .mGeometryAllocationBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mCameraBuffer = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData ),
                    .mMaterialsBuffer = ctx.GetDeviceBufferAddress( geometryData.mMaterialsBuffer ),
                    .mBasicSamplerID = ctx.PushSampler( geometryData.mMaterialSampler )
                };

                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( dimensions.first ), core::as<core::i32>( dimensions.second ) } )
                    .AddDepthTarget( mousePickingData.mDepthImage, renderer::rhi::LoadOp::eClear )
                    .AddRenderTarget( mousePickingData.mColorImage, renderer::rhi::Color{ 0.f }, renderer::rhi::LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<core::f32>( dimensions.first ), core::as<core::f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( mousePickingData.mPipeline );

                mGeometryCullModule->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );

        // Maybe change this to compute
        graph.RegisterPass(
            "ObjectSelection_Readback",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& blackboard ) {
                const auto& mousePicking{ blackboard.Get<MousePickingModuleInfo>() };

                builder.UseResource( mousePicking.mReadBackBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mousePicking.mColorImage, FGPipelineStage::eCopy, FGResourceAccess::eRead );
            },
            [this, &graph]( CommandContext& ctx, core::Blackboard& b ) {
                if ( !mQueuedPick || mIsReadbackInFlight ) {
                    return;
                }

                const auto pixel{ *mQueuedPick };
                const auto revision{ GetRevision() };
                const auto &data{ b.Get<MousePickingModuleInfo>() };

                mQueuedPick.reset();
                mIsReadbackInFlight = true;

                ctx.Copy( data.mReadBackBuffer, data.mColorImage );

                // Register only after recording a copy. This is an asynchronous operation
                // we do not know when the above copy will be done for us to memcpy back.
                graph.RegisterReadback(
                    [this, pixel, revision]( core::Blackboard& blackboard, const FGResourceManager& manager ) {
                        const auto &data{ blackboard.Get<MousePickingModuleInfo>() };

                        if ( const void* address{ manager.GetBufferMappedAddress( data.mReadBackBuffer ) } ) {
                            if ( IsRevision( revision ) ) {
                                std::memcpy( mData.data(), address, mData.size() * MKT_SIZEOF( core::u32 ) );

                                mIsReadbackDone = true;
                                mPickResult = ReadPixel( pixel.mX, pixel.mY );
                            }
                        }

                        mIsReadbackInFlight = false;
                    } );
            } );
    }
}// namespace mikoto
