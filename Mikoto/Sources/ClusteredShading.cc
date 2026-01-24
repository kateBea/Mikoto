//    Copyright 2025 ケイト
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

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ClusteredShading.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>

namespace Mikoto {

    ClusteredShading::ClusteredShading( RenderResolution resolution )
        : m_Resolution{ resolution }
    {}

    auto ClusteredShading::SetScene( const Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto ClusteredShading::SetCamera( const Camera *camera ) -> void {
        m_Camera = camera;
    }

    auto ClusteredShading::RegisterPasses(FrameGraph &graph) -> void {
        m_Lights.resize( MAX_LIGHTS );

        BuildAABB( graph );
        BuildLightCulling( graph );
        BuildShadowMapping( graph );
    }

    auto ClusteredShading::BuildAABB( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "GenerateAABB",
                []( FramePassBuilder &b ) {
                    b.Create<Buffer>( "SimpleCompute_Results", BufferUsage::SSBO, 30 * sizeof( float ) );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE );
                    b.Create<Pipeline>( "SimpleCompute_Pipeline", ComputePipelineDescription{} );

                    b.Write( "SimpleCompute_Results", FrameResourceState::Transfer_Src );
                    b.UseSrg( SRGType::SRG_PerPass, "SimpleCompute_Results", 0 );
                },
                []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    ctx.BindPipeline( "SimpleCompute_Pipeline" );

                    // Prime numbers up until this value
                    constexpr UInt32 limitNumbers{ 30 };

                    // matches shader's local_size_x
                    constexpr UInt32 localSize{ 64 };
                    constexpr UInt32 groupCount{ ( limitNumbers + localSize - 1 ) / localSize };

                    ctx.Dispatch( groupCount, 1, 1 );
                } );
    }

    auto ClusteredShading::BuildLightCulling( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                        "LightCulling",
                        []( FramePassBuilder &b ) {
                            b.Create<Buffer>( "SimpleCompute_Results", BufferUsage::SSBO, 30 * sizeof( float ) );

                            b.UseShader( "Resources/Shaders/vulkan-spirv/BasicCompute_Comp.sprv", ShaderStage::COMPUTE );
                            b.Create<Pipeline>( "SimpleCompute_Pipeline", ComputePipelineDescription{} );

                            b.Write( "SimpleCompute_Results", FrameResourceState::Transfer_Src );
                            b.UseSrg( SRGType::SRG_PerPass, "SimpleCompute_Results", 0 );
                        },
                        []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                            ctx.BindPipeline( "SimpleCompute_Pipeline" );

                            // Prime numbers up until this value
                            constexpr UInt32 limitNumbers{ 30 };

                            // matches shader's local_size_x
                            constexpr UInt32 localSize{ 64 };
                            constexpr UInt32 groupCount{ ( limitNumbers + localSize - 1 ) / localSize };

                            ctx.Dispatch( groupCount, 1, 1 );
                        } );
    }

    auto ClusteredShading::BuildShadowMapping( FrameGraph &graph ) -> void {

    }
}
