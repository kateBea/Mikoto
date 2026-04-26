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
#include <Core/String.hh>
#include <Core/Profiler.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/DebugPasses.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

namespace mikoto::renderer {

    using namespace mikoto::scene;

    DebugPasses::DebugPasses( RenderResolution resolution )
        : mResolution{ resolution } {}

    auto DebugPasses::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
        RegisterTrianglePass( graph );
        RegisterTexturePass( graph );
        RegisterSimpleComputePass( graph );
    }

    auto DebugPasses::RegisterTrianglePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct TrianglePassData {
            BufferID mBuffer{};
            PipelineID mPipeline{};
        };

        graph.RegisterPass<TrianglePassData>(
            "SimpleTriangle",
            FrameGraphNodeType::eGraphics,
            []( FrameGraphNodeBuilder &b, TrianglePassData &data ) {
                auto bufferBuilder{ FrameGraphBufferDescription{}
                    .SetUsage( BufferUsageFlagsBits::kStructured ) // Type of buffer
                    .SetSizeBytes( 128 )
                    .SetHeapType( HeapType::eReadback )
                    .SetResourceType( ResourceType::eConstantBuffer ) // How it is used in the shader
                };

                data.mBuffer = b.Create( bufferBuilder );

                auto pipelineBuilder{ FrameGraphPipelineDescription{}
                    .SetName( "Triangle_Pipeline" )
                    .SetPipelineType( PipelineType::eGraphics )
                    .PushShader( "HelloTriangleFG_Vert.slang", FrameGraphStageType::eVertex )
                    .PushShader( "HelloTriangleFG_Frag.slang", FrameGraphStageType::eFragment )
                };

                data.mPipeline = b.Create( pipelineBuilder );
                b.Write( data.mBuffer, FrameGraphResourceAccessType::eWrite, FrameGraphStageType::eVertex );
            },
            []( CommandContext &ctx, Blackboard &blackboard ) -> void {
                const auto &data{ blackboard.Get<TrianglePassData>() };
                ctx.BindPipeline( data.mPipeline );
            } );
    }

    auto DebugPasses::RegisterTexturePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct TexturePassData {
            TextureID mTexture{};
            SamplerID mSampler{};
            PipelineID mPipeline{};
        };

        graph.RegisterPass<TexturePassData>(
                "SimpleTexture",
                FrameGraphNodeType::eGraphics,
                []( FrameGraphNodeBuilder &b, TexturePassData &data ) {
                    auto pipelineBuilder{ FrameGraphPipelineDescription{}
                        .SetName( "Texture_Pipeline" )
                        .SetPipelineType( PipelineType::eGraphics )
                        .PushShader( "HelloTexture_Vert.slang", FrameGraphStageType::eVertex )
                        .PushShader( "HelloTexture_Frag.slang", FrameGraphStageType::eFragment ) };

                    data.mPipeline = b.Create( pipelineBuilder );

                    b.Read( data.mTexture, FrameGraphResourceAccessType::eRead, FrameGraphStageType::eFragment );
                    b.Read( data.mSampler, FrameGraphResourceAccessType::eRead, FrameGraphStageType::eFragment );
                },
                []( CommandContext &ctx, Blackboard &blackboard ) -> void {
                    const auto &data{ blackboard.Get<TexturePassData>() };

                    struct DrawParams {
                        u32 mTextureIndex{};
                        u32 mSamplerIndex{};
                    };

                    DrawParams params{
                        .mTextureIndex = ctx.GetIndex( data.mTexture ),
                        .mSamplerIndex = ctx.GetIndex( data.mSampler ),
                    };

                    ctx.PushConstants( params );
                    ctx.BindPipeline( data.mPipeline );
                    ctx.Draw( 4 );
                } );
    }

    auto DebugPasses::RegisterSimpleComputePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct SimpleCompute {
            BufferID mBuffer{};        // GPU buffer (UAV, device local)
            BufferID mReadbackBuffer{};// CPU-visible buffer
            PipelineID mPipeline{};

            u32 mLocalSize{ 64 };
            u32 mNumbersCount{ 30 };
            u32 mGroupCount{ ( mNumbersCount + mLocalSize - 1 ) / mLocalSize };
        };

        graph.RegisterPass<SimpleCompute>(
            "SimpleCompute",
            FrameGraphNodeType::eCompute,

            []( FrameGraphNodeBuilder &b, SimpleCompute &data ) {
                // GPU buffer (fast, written by compute shader)
                auto gpuBufferDesc{ FrameGraphBufferDescription{}
                     .SetUsage( BufferUsageFlagsBits::kStructured | BufferUsageFlagsBits::kUnorderedAccess )
                     .SetElementsSize( MKT_SIZEOF( u32 ), data.mNumbersCount )
                     .SetHeapType( HeapType::eDeviceLocal )
                     .SetResourceType( ResourceType::eStructuredBuffer_UAV ) };

                data.mBuffer = b.Create( gpuBufferDesc );

                // Readback buffer (CPU visible, copy destination)
                auto readbackDesc{ FrameGraphBufferDescription{}
                    .SetUsage( BufferUsageFlagsBits::kCopyDst )
                    .SetElementsSize( MKT_SIZEOF( u32 ), data.mNumbersCount )
                    .SetHeapType( HeapType::eReadback )
                    .SetResourceType( ResourceType::eStructuredBuffer_UAV ) };

                data.mReadbackBuffer = b.Create( readbackDesc );

                // Pipeline
                auto pipelineBuilder{ FrameGraphPipelineDescription{}
                   .SetName( "SimpleCompute_Pipeline" )
                   .SetPipelineType( PipelineType::eCompute )
                   .PushShader( "BasicCompute_Comp.slang", FrameGraphStageType::eCompute ) };

                data.mPipeline = b.Create( pipelineBuilder );

                // Resource usage
                b.Read( data.mBuffer, FrameGraphResourceAccessType::eRead, FrameGraphStageType::eTransfer );
                b.Write( data.mBuffer, FrameGraphResourceAccessType::eWrite, FrameGraphStageType::eCompute );
                b.Write( data.mReadbackBuffer, FrameGraphResourceAccessType::eWrite, FrameGraphStageType::eTransfer );
            },

            []( CommandContext &ctx, Blackboard &blackboard ) {
                const auto &data{ blackboard.Get<SimpleCompute>() };

                // Push bindless index (NOT raw ResourceID)
                struct ComputeParams {
                    u32 mBufferIndex{};
                };

                ComputeParams params{};
                params.mBufferIndex = ctx.GetIndex( data.mBuffer );

                ctx.PushConstants( params );
                ctx.BindPipeline( data.mPipeline );

                // Dispatch compute
                ctx.Dispatch( data.mGroupCount, 1, 1 );

                // Copy GPU -> CPU buffer
                ctx.CopyBuffer( data.mBuffer, data.mReadbackBuffer );
            } );
    }
}// namespace mikoto::renderer