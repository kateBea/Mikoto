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

#include <Core/Profiler.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ParticleSimulationModule.hh>

namespace mikoto::renderer {

    ParticleSimulationModule::ParticleSimulationModule( RenderResolution resolution )
        : mResolution{ resolution }
    {}

    auto ParticleSimulationModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterParticleRenderPass( graph );
    }

    auto ParticleSimulationModule::RegisterParticleRenderPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "ParticleRendering",
            FGPassType::eCompute,
            []( FGNodeBuilder&, Blackboard& ) {
            },
            []( CommandContext&, Blackboard& ) -> void {
            } );
    }
}// namespace mikoto