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

#include <EASTL/fixed_vector.h>
#include <EASTL/optional.h>
#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <Assets/AssetsService.hh>
#include <Assets/ImageProcessor.hh>
#include <Assets/Model.hh>
#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/DebugModule.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

#include "Renderer/Rhi/Swapchain.hh"

namespace mikoto::renderer {

    using namespace mikoto::asset;
    using namespace mikoto::renderer::rhi;

    DebugModule::DebugModule( RenderResolution resolution )
        : mResolution{ resolution } {}

    auto DebugModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterTrianglePass( graph );
        RegisterTexturePass( graph );
        RegisterSimpleComputePass( graph );
    }

    auto DebugModule::RegisterTrianglePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        TrianglePassData& passData{ graph.GetOrCreate<TrianglePassData>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "TrianglePass_ColorImage01" )
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eBGRA8_UNORM ) };
        passData.mColorTarget = graph.Create( colorImage );

        auto depthImage{ FGTextureDescription{}
            .SetName( "TrianglePass_DepthImage01" )
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };
        passData.mDepthTarget = graph.Create( depthImage  );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "TrianglePass_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .PushShader( "HelloTriangleFG_Vert.slang", FGStageType::eVertex )
            .PushShader( "HelloTriangleFG_Frag.slang", FGStageType::ePixel ) };
        passData.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<TrianglePassData>(
            "SimpleTriangle",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, TrianglePassData& data ) {
                b.UseResource( data.mColorTarget, FGResourceStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mDepthTarget, FGResourceStage::eDepthTarget, FGResourceAccess::eWrite );
            },
            []( CommandContext &ctx, Blackboard &b) -> void {
                const auto& data{ b.Get<TrianglePassData>() };
                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ 1920, 1080 } )
                    .AddDepthTarget( data.mDepthTarget )
                    .AddRenderTarget( data.mColorTarget, kColorGreen, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

                ctx.BindPipeline( data.mPipeline );
                ctx.Draw( 3 );

                ctx.EndRender();
            } );
    }

    auto DebugModule::RegisterTexturePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Create the resources
        TexturePassData& trianglePassData{ graph.GetOrCreate<TexturePassData>() };

        auto colorImage{ FGTextureDescription{}
            .SetName( "TexturePass_ColorImage01" )
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kShaderResource )
            .SetFormat( Format::eBGRA8_UNORM ) };

        trianglePassData.mColorTarget = graph.Create( colorImage );

        auto depthImage{ FGTextureDescription{}
            .SetName( "TexturePass_DepthImage01" )
            .SetWidth( as<i32>( 1920 ) )
            .SetHeight( as<i32>( 1080 ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        trianglePassData.mDepthTarget = graph.Create( depthImage );

        auto samplerDes{ FGSamplerDescription{}
            .SetName( "TexturePass_Sampler01" )
            .SetFilter( rhi::SamplerFilter::eNearest )
            .SetWrap( SamplerWrapMode::eRepeat )
            .SetBorderColor( kColorWhite ) };

        trianglePassData.mSampler = graph.Create( samplerDes );

        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "TexturePass_Pipeline01" )
            .SetPipelineType( PipelineType::eGraphics )
            .SetTopology( PrimitiveTopology::eTriangleStrip )
            .SetDepthFormat( Format::eD32 )
            .AddColorFormat( Format::eBGRA8_UNORM )
            .PushShader( "HelloTexture_Vert.slang", FGStageType::eVertex )
            .PushShader( "HelloTexture_Frag.slang", FGStageType::ePixel ) };

        trianglePassData.mPipeline = graph.Create( pipelineBuilder );

        trianglePassData.mImportedTexture = graph.ImportTexture( "Resources/Textures/diffuse.jpg" );

        graph.RegisterPass<TexturePassData>(
            "SimpleTexture",
            FGPassType::eGraphics,
            []( FGNodeBuilder &b, TexturePassData& data ) {
                b.UseResource( data.mColorTarget, FGResourceStage::eRenderTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mDepthTarget, FGResourceStage::eDepthTarget, FGResourceAccess::eWrite );
                b.UseResource( data.mImportedTexture, FGResourceStage::eComputeShader, FGResourceAccess::eRead );
            },
            []( CommandContext &ctx, Blackboard &blackboard ) -> void {
                const auto& data{ blackboard.Get<TexturePassData>() };
                struct DrawParams {
                    u32 mTextureIndex{};
                    u32 mSamplerIndex{};
                } params{
                    .mTextureIndex = ctx.PushTexture_SRV( data.mImportedTexture ),
                    .mSamplerIndex = ctx.PushSampler( data.mSampler ),
                };

                ctx.PushConstants( params );

                const auto graphicsState{ ContextRenderState{}
                    .SetRenderArea( Rect{ 1920, 1080 } )
                    .AddDepthTarget( data.mDepthTarget )
                    .AddRenderTarget( data.mColorTarget, kColorCyan, LoadOp::eClear ) };
                ctx.BeginRender( graphicsState );

                ctx.SetViewportState( ViewportState{}
                    .AddViewportAndScissorRect( Viewport( 1920, 1080 ) ) );

                ctx.BindPipeline( data.mPipeline );
                ctx.Draw( 4 );

                ctx.EndRender();
            } );
    }

    auto DebugModule::RegisterSimpleComputePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct SimpleCompute {
            FGBufferHandle mComputeBuffer{};
            FGBufferHandle mReadbackBuffer{};

            FGPipelineHandle mPipeline{};

            u32 mLocalSize{ 64 };
            u32 mNumbersCount{ 30 };
            u32 mGroupCount{ ( mNumbersCount + mLocalSize - 1 ) / mLocalSize };
        };

        struct MyStruct {
            i32 x{};
            i32 y{};
            i32 prime{};
            i32 _pad{};
        };

        // Create the resources
        SimpleCompute& simpleCompute{ graph.GetOrCreate<SimpleCompute>() };

        // GPU buffer (written by compute shader)
        auto gpuBufferDesc{ FGBufferDescription{}
            .SetName( "SimpleCompute_ComputeBuffer" )
            .SetUsage( BufferUsageFlagsBits::kStorage | BufferUsageFlagsBits::kCopySrc )
            .SetElementsSize( simpleCompute.mNumbersCount, MKT_SIZEOF( MyStruct ) )
            .SetHeapType( HeapType::eDeviceLocal ) };
        simpleCompute.mComputeBuffer = graph.Create( gpuBufferDesc );

        // Readback buffer (CPU visible, copy destination)
        auto readbackDesc{ FGBufferDescription{}
            .SetName( "SimpleCompute_ReadBackBuffer" )
            .SetUsage( BufferUsageFlagsBits::kCopyDst )
            .SetElementsSize( simpleCompute.mNumbersCount, MKT_SIZEOF( MyStruct ) )
            .SetHeapType( HeapType::eReadback ) };
        simpleCompute.mReadbackBuffer = graph.Create( readbackDesc );

        // Pipeline
        auto pipelineBuilder{ FGPipelineDescription{}
            .SetName( "SimpleCompute_Pipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "BasicCompute_Comp.slang", FGStageType::eCompute ) };
        simpleCompute.mPipeline = graph.Create( pipelineBuilder );

        graph.RegisterPass<SimpleCompute>(
            "SimpleCompute",
            FGPassType::eCompute,
            []( FGNodeBuilder &b, SimpleCompute &data ) {
                b.UseResource( data.mComputeBuffer, FGResourceStage::eUnorderedAccess, FGResourceAccess::eWrite );
            },
            []( CommandContext &ctx, Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<SimpleCompute>() };
                struct ComputeParams {
                    u32 mBufferIndex{};
                } params{
                    .mBufferIndex = ctx.PushBuffer_UAV( data.mComputeBuffer )
                };

                ctx.PushConstants( params );
                ctx.BindPipeline( data.mPipeline );

                ctx.Dispatch( data.mGroupCount, 1, 1 );
            } );

        graph.RegisterPass<SimpleCompute>(
            "SimpleCompute_Readback",
            FGPassType::eTransfer,
            []( FGNodeBuilder &b, SimpleCompute &data ) {
                b.UseResource( data.mReadbackBuffer, FGResourceStage::eCopy, FGResourceAccess::eWrite );
                b.UseResource( data.mComputeBuffer, FGResourceStage::eCopy, FGResourceAccess::eRead );
            },
            []( CommandContext &ctx, Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<SimpleCompute>() };
                ctx.CopyBuffer( data.mReadbackBuffer, data.mComputeBuffer );
            } );

        graph.SetExecutionPolicy( "SimpleCompute_Readback", FGExecutionPolicy::eOnce );

        graph.RegisterReadback(
            []( Blackboard &blackboard, const FGResourceManager& manager ) {
                const auto &data{ blackboard.Get<SimpleCompute>() };

                eastl::vector<MyStruct> myStructs( data.mNumbersCount );
                const usize sizeBytes{ data.mNumbersCount * MKT_SIZEOF( MyStruct ) };

                if ( const void *mappedAddress{ manager.GetBufferMappedAddress( data.mReadbackBuffer ) } ) {
                    std::memcpy( myStructs.data(), mappedAddress, sizeBytes );
                }
            } );
    }
}// namespace mikoto::renderer