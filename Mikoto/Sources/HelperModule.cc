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

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/HelperModule.hh>

namespace mikoto::renderer {

    auto HelperModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterImageBlitPass( graph );
    }

    auto HelperModule::RegisterImageBlitPass(FrameGraph &graph) -> void {
        graph.RegisterPass(
            "ImageBlitPass",
            FGPassType::eGraphics,
            []( FGNodeBuilder &, Blackboard & ) {
            },
            []( CommandContext &ctx, Blackboard &b ) {
                // const auto &data{ b.Get<ImageBlitModuleInfo>() };
                // struct ComputeParams {
                //     u32 mInputImageID{};
                //     u32 mOutPutImageID{};
                // } params{
                //     //.mBufferIndex = ctx.PushBuffer_UAV( data.mComputeBuffer )
                // };
                //
                // ctx.PushConstants( params );
                // ctx.BindPipeline( data.mPipelineHandle );
                //
                // ctx.Dispatch( 1, 1, 1 );
            } );
    }
}// namespace mikoto::renderer
