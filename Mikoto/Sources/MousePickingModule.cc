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

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Renderer/Passes/MousePickingModule.hh>

namespace mikoto::renderer {
    MousePickingModule::MousePickingModule( rhi::RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto MousePickingModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterSelectionBuffer( graph );

    }

    auto MousePickingModule::ReadPixel( u32 x, u32 y ) -> core::u32 {
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
        u32 width{ as<u32>(dimensions.first) };
        return mData[ y * width + x];
    }

    auto MousePickingModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        mGeometryCullModule = MKT_ADDRESSOF( geom );
    }

    auto MousePickingModule::RegisterSelectionBuffer( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MousePickingModuleInfo& info{ graph.GetOrCreate<MousePickingModuleInfo>() };
        auto dimensions{ InferDimensions( mResolution ) };

        Format format{ Format::eR32_UINT };
        auto imageDesc{ FGTextureDescription{}
            .SetName( "ObjectSelection_Color" )
            .SetWidth( as<i32>( dimensions.first ) )
            .SetHeight( as<i32>( dimensions.second ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
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

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "ObjectSelection_Pipeline" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetBlendEnable( false )
            .SetCullMode( CullMode::eNone )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eR32_UINT )
            .PushShader( "MousePick_Vert.slang", FGStageType::eVertex )
            .PushShader( "MousePick_Frag.slang", FGStageType::eFragment ) };

        info.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass(
    "ObjectSelection_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, Blackboard & blackboard ) {
                const auto& prepass{ blackboard.Get<PrepassModuleInfo>() };
                const auto& cameraInfo{ blackboard.Get<CameraModuleInfo>() };
                const auto& mousePicking{ blackboard.Get<MousePickingModuleInfo>() };
                const auto& geometryInfo{ blackboard.Get<GeometryManagementModuleInfo>() };

                builder.Read( cameraInfo.mCameraData, FGResourceState::eShaderResource );

                builder.Read( geometryInfo.mGeometryBuffer, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mSkinningBuffer, FGResourceState::eShaderResource );

                builder.Read( geometryInfo.mVerticesBuffer, FGResourceState::eShaderResource );
                builder.Read( geometryInfo.mIndicesBuffer, FGResourceState::eShaderResource );

                builder.Read( prepass.mDepthPrepassDepthTarget, FGResourceState::eDepthRead );

                builder.Write( mousePicking.mColorImage, FGResourceState::eRenderTarget );
            },
            [this]( CommandContext & ctx, Blackboard &b ) {
                const auto& prepassInfo{ b.Get<PrepassModuleInfo>() };
                const auto& mousePicking{ b.Get<MousePickingModuleInfo>() };
                const auto& cameraPassInfo{ b.Get<CameraModuleInfo>() };
                const auto& geometryInfo{ b.Get<GeometryManagementModuleInfo>() };

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
                    .AddDepthTarget( prepassInfo.mDepthPrepassDepthTarget, LoadOp::eLoad )
                    .AddRenderTarget( mousePicking.mColorImage, Color{ 0.f }, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( as<f32>( dimensions.first ), as<f32>( dimensions.second ) ) ) );

                ctx.BindPipeline( mousePicking.mPipeline );

                mGeometryCullModule->DrawInstancesIndirect( ctx );

                ctx.EndRender();
            } );

        graph.RegisterPass(
    "ObjectSelection_Readback",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, Blackboard & blackboard ) {
                const auto& mousePicking{ blackboard.Get<MousePickingModuleInfo>() };

                builder.Write( mousePicking.mReadBackBuffer, FGResourceState::eCopyDest );
                //builder.Read( mousePicking.mColorImage, FGResourceState::eCopySource );
            },
            []( CommandContext &ctx, Blackboard & b ) {
                const auto& mousePicking{ b.Get<MousePickingModuleInfo>() };
                //ctx.Copy( mousePicking.mReadBackBuffer, mousePicking.mColorImage );
            } );
    }
}// namespace mikoto