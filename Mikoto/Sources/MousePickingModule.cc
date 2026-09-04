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
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>

#include <Assets/ImageProcessor.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Renderer/Passes/MousePickingModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    MousePickingModule::MousePickingModule( rhi::RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto MousePickingModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterSelectionBuffer( graph );

    }

    auto MousePickingModule::SetReadRegion( f32 x, f32 y, f32 width, f32 height ) -> void {

    }

    auto MousePickingModule::ReadPixel( u32 x, u32 y ) const -> core::u32 {
        if (mData.empty()) {
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
        if (x > dimensions.first || y > dimensions.second ) {
            return 0;
        }

        u32 width{ as<u32>(dimensions.first) };
        return mData[ y * width + x];
    }

    auto MousePickingModule::ReadPixel( const ReadPixelViewportInfo& viewport ) const -> core::u32 {
        u32 x{ (u32)(viewport.mX) };
        u32 y{ (u32)(viewport.mY) };

        auto dimensions{ InferDimensions( mResolution ) };
        if (x > dimensions.first || y > dimensions.second ) {
            return 0;
        }

        f32 localX{ (viewport.mX - viewport.mViewportX) };
        f32 localY{ (viewport.mY - viewport.mViewportY) };

        u32 uvX{ (u32)((localX - viewport.mViewportWidth) / dimensions.first) };
        u32 uvY{ (u32)((localY - viewport.mViewportHeight) / dimensions.second ) };

        u32 width{ as<u32>(dimensions.first) };
        return mData[ uvY * width + uvX];
    }

    auto MousePickingModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        mGeometryCullModule = MKT_ADDRESSOF( geom );
    }

    auto MousePickingModule::RegisterSelectionBuffer( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& info{ graph.GetOrCreate<MousePickingModuleInfo>() };
        auto dimensions{ InferDimensions( mResolution ) };

        Format format{ Format::eR32_UINT };
        auto imageDesc{ FGTextureDescription{}
            .SetName( "ObjectSelection_Color" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kCopySrc )
            .SetFormat( format ) };
        info.mColorImage = graph.Create( imageDesc );

        const auto& formatInfo{ rhi::GetFormatInfo( format ) };
        auto bufferDesc{ FGBufferDescription{}
            .SetName( "ObjectSelection_ReadbackBuffer" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopyDst )
            .SetSizeBytes( dimensions.first * dimensions.second * formatInfo.mBytesPerBlock )
            .SetHeapType( HeapType::eReadback ) };
        info.mReadBackBuffer = graph.Create( bufferDesc );

        core::usize totalBytes{ (usize)(dimensions.first * dimensions.second * formatInfo.mBytesPerBlock) };
        mData.resize( totalBytes / sizeof(core::u32) );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ObjectSelection_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetBlendEnable( false )
            .SetDepthWrite( false )
            .SetDepthTest( true )
            .SetCullMode( CullMode::eNone )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eR32_UINT )
            .PushShader( "MousePick_Vert.slang", FGStageType::eVertex )
            .PushShader( "MousePick_Frag.slang", FGStageType::ePixel ) };
        info.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
            "ObjectSelection_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard & blackboard ) {
                const auto& prePassData{ blackboard.Get<PrepassModuleInfo>() };
                const auto& cameraData{ blackboard.Get<CameraModuleInfo>() };
                const auto& mousePickingData{ blackboard.Get<MousePickingModuleInfo>() };
                const auto& geometryData{ blackboard.Get<GeometryCullModuleInfo>() };

                builder.UseResource( cameraData.mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( geometryData.mSkinningBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( geometryData.mGeometryAllocBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );

                builder.UseResource( prePassData.mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );

                builder.UseResource( mousePickingData.mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext & ctx, Blackboard &b ) {
                const auto& prePassData{ b.Get<PrepassModuleInfo>() };
                const auto& mousePickingData{ b.Get<MousePickingModuleInfo>() };
                const auto& cameraPassData{ b.Get<CameraModuleInfo>() };
                const auto& geometryData{ b.Get<GeometryCullModuleInfo>() };

                struct DrawParams {
                    SPointer mGeometryBuffer{};
                    SPointer mSkinningBuffer{};
                    SPointer mGeometryAllocationBufferID{};

                    SPointer mCameraBuffer{};
                } params{
                    .mGeometryBuffer = ctx.GetDeviceBufferAddress( geometryData.mGeometryBuffer ),
                    .mSkinningBuffer = ctx.GetDeviceBufferAddress( geometryData.mSkinningBuffer ),

                    .mGeometryAllocationBufferID = ctx.GetDeviceBufferAddress( geometryData.mGeometryAllocBuffer ),
                    .mCameraBuffer = ctx.GetDeviceBufferAddress( cameraPassData.mCameraData ) };
                ctx.PushConstants( params );

                const auto dimensions{ InferDimensions( mResolution ) };

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ as<i32>( dimensions.first ), as<i32>( dimensions.second ) } )
                    .AddDepthTarget( prePassData.mPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( mousePickingData.mColorImage, Color{ 0.f }, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( mousePickingData.mPipeline );

                mGeometryCullModule->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );

        // Maybe change this to compute
        graph.RegisterPass(
            "ObjectSelection_Readback",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard & blackboard ) {
                const auto& mousePicking{ blackboard.Get<MousePickingModuleInfo>() };

                builder.UseResource( mousePicking.mReadBackBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( mousePicking.mColorImage, FGPipelineStage::eCopy, FGResourceAccess::eRead );
            },
            []( CommandContext &ctx, Blackboard & b ) {
                const auto& mousePicking{ b.Get<MousePickingModuleInfo>() };

                // Try doing this with a compute shader instead
                // This takes a lot, in Nsight Graphics this shows 0.32ms
                ctx.Copy( mousePicking.mReadBackBuffer, mousePicking.mColorImage );
            } );

        graph.RegisterReadback(
            [this]( Blackboard &blackboard, const FGResourceManager& manager ) {
                const auto &data{ blackboard.Get<MousePickingModuleInfo>() };

                if ( const void *mappedAddress{ manager.GetBufferMappedAddress( data.mReadBackBuffer ) } ) {
                    core::usize bytesToCopy{ mData.size() * sizeof(core::u32) };
                    std::memcpy( mData.data(), mappedAddress, bytesToCopy );
                }
            }, true );
    }
}// namespace mikoto