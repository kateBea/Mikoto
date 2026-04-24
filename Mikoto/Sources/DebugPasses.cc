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
            ResourceID mBuffer{};
            ResourceID mPipeline{};
        };

        graph.RegisterPass<TrianglePassData>(
            "SimpleTriangle",
            FrameGraphNodeType::eGraphics,
            []( FrameGraphNodeBuilder &b, TrianglePassData &data ) {
                auto bufferBuilder{ FrameGraphBufferDescription{}
                    .SetUsage( BufferUsageFlagsBits::kStructured ) // Type of buffer
                    .SetSizeBytes( 128 )
                    .SetCpuAccess( HeapType::eReadback )
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
            ResourceID mBuffer{};
            ResourceID mPipeline{};
        };

        graph.RegisterPass<TexturePassData>(
            "SimpleTexture",
            FrameGraphNodeType::eCompute,
            []( FrameGraphNodeBuilder &b, TexturePassData &data ) {
                auto bufferBuilder{ FrameGraphBufferDescription{}
                    .SetUsage( BufferUsageFlagsBits::kConstant ) // Type of buffer
                    .SetSizeBytes( 128 )
                    .SetCpuAccess( HeapType::eReadback )
                    .SetResourceType( ResourceType::eConstantBuffer ) // How it is used in the shader
                };

                data.mBuffer = b.Create( bufferBuilder );

                auto pipelineBuilder{ FrameGraphPipelineDescription{}
                    .SetName( "Texture_Pipeline" )
                    .SetPipelineType( PipelineType::eGraphics )
                    .PushShader( "HelloTexture_Vert.slang", FrameGraphStageType::eVertex )
                    .PushShader( "HelloTexture_Frag.slang", FrameGraphStageType::eFragment )
                };

                data.mPipeline = b.Create( pipelineBuilder );

                b.Write( data.mBuffer, FrameGraphResourceAccessType::eWrite, FrameGraphStageType::eVertex );
            },
            []( CommandContext &ctx, Blackboard &blackboard ) -> void {
                const auto &data{ blackboard.Get<TexturePassData>() };
                ctx.BindPipeline( data.mPipeline );
            } );
    }

    auto DebugPasses::RegisterSimpleComputePass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        struct SimpleCompute {
            ResourceID mBuffer{};
            ResourceID mPipeline{};

            u32 mLocalSize{ 64 };
            u32 mNumbersCount{ 30 };
            u32 mGroupCount{ ( mNumbersCount + mLocalSize - 1 ) / mLocalSize };
        };

        graph.RegisterPass<SimpleCompute>(
            "SimpleCompute",
            FrameGraphNodeType::eCompute,
            []( FrameGraphNodeBuilder &b, SimpleCompute &data ) {
                auto bufferBuilder{ FrameGraphBufferDescription{}
                    .SetUsage( BufferUsageFlagsBits::kStructured ) // Type of buffer
                    .SetElementsSize( MKT_SIZEOF( u32 ), data.mLocalSize )
                    .SetCpuAccess( HeapType::eReadback )
                    .SetResourceType( ResourceType::eStructuredBuffer_UAV ) // How it is used in the shader
                };

                data.mBuffer = b.Create( bufferBuilder );

                auto pipelineBuilder{ FrameGraphPipelineDescription{}
                    .SetName( "SimpleCompute_Pipeline" )
                    .SetPipelineType( PipelineType::eCompute )
                    .PushShader( "BasicCompute_Comp.slang", FrameGraphStageType::eCompute )
                };

                data.mPipeline = b.Create( pipelineBuilder );

                b.Write( data.mBuffer, FrameGraphResourceAccessType::eWrite, FrameGraphStageType::eCompute );
            },
            []( CommandContext &ctx, Blackboard &blackboard ) -> void {
                const auto &data{ blackboard.Get<SimpleCompute>() };

                ctx.PushConstants( data.mBuffer );
                ctx.BindPipeline( data.mPipeline );
                ctx.Dispatch( data.mGroupCount, 1, 1 );
            } );
    }
}// namespace mikoto::renderer