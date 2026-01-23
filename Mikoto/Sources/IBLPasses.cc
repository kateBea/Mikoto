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

#include <Math/Math.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>
#include <Renderer/Core/FrameResource.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>
#include <Renderer/Passes/IBLPasses.hh>

namespace Mikoto {

    auto RegisterIrradiance( FrameGraph &graph ) -> void {

        struct IrradianceData {
            TextureHandle ColorTarget;
            PipelineHandle Pipeline;
            UInt32 MipLevels;
            UInt32 Dimensions;

            // Ubo data
        };

        // graph.RegisterPass<IrradianceData>(
        //         "IrradiancePass",
        //
        //         [&]( FramePassBuilder &b, IrradianceData &data ) {
        //             // Compute mip levels
        //             data.MipLevels = static_cast<UInt32>( Math::Floor( Math::Log2( data.Dimensions ) ) ) + 1;
        //
        //             // Create target
        //             data.ColorTarget =
        //                     b.CreateCubeTexture( "IrradiancePass_ColorTarget",
        //                                          data.Dimensions,
        //                                          TextureFormat::TEXTURE_FORMAT_RGBA32_FLOAT,
        //                                          data.MipLevels );
        //
        //             // Create pipeline
        //             PipelineDescription desc{};
        //             desc.AddShader( "...Vert.sprv", ShaderStage::VERTEX_STAGE );
        //             desc.AddShader( "...Frag.sprv", ShaderStage::FRAGMENT_STAGE );
        //             desc.ColorRenderTargets.emplace_back( "IrradiancePass_ColorTarget" );
        //             data.Pipeline = b.CreateNamedPipeline( "IrradiancePass_Pipeline", desc );
        //
        //             // Declare framegraph usages
        //             b.Write( "IrradiancePass_ColorTarget", FrameResourceState::RenderTarget_Color );
        //             b.Read( "SkyboxPass_TextureCube", FrameResourceState::ShaderResource_Read );
        //
        //             b.BindBuffer( "IrradiancePass_CameraInfo", SRGType::SRG_PerPass, 0 );
        //             b.BindBuffer( "IrradiancePass_Parameters", SRGType::SRG_PerPass, 1 );
        //             b.BindTexture( "SkyboxPass_TextureCube", SRGType::SRG_PerPass, 2, "SkyboxPass_Sampler" );
        //         },
        //
        //         [&]( CommandContext &ctx, FrameBlackboard &blackboard ) {
        //             const IrradianceData &data{ blackboard.Get<IrradianceData>() };
        //
        //             ctx.BeginPass( "IrradiancePass" );
        //
        //             ctx.BindPipeline( data.Pipeline );
        //             ctx.SetViewport( 0, 0, 1920, 1080 );
        //             ctx.SetScissor( 0, 0, 1920, 1080 );
        //
        //             for (uint32_t mip = 0; mip < data.MipLevels; ++mip) {
        //                 for (uint32_t face = 0; face < 6; ++face) {
        //                     ctx.SetColorRenderTarget( data.ColorTarget, face, mip );
        //
        //                     // update UBOs
        //                     ctx.FillBuffer( "IrradiancePass_CameraInfo", &ubo, sizeof( ubo ) );
        //
        //                     ctx.BeginRender();
        //                     ctx.Draw( 6 );
        //                     ctx.EndRender();
        //                 }
        //             }
        //
        //             ctx.EndPass();
        //         } );

    }

}// namespace Mikoto
