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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/WeatherModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

namespace mikoto::renderer {

    WeatherModule::WeatherModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto WeatherModule::RegisterWindSimulation( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        // Filter and upload for all wind simulation entities
        // Could cull simulation for entities that are not visible
        graph.RegisterPass(
            "WindSimulationInit_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Traverse all WindSimulationComponent
                // components and upload their data to a buffer
            } );

        graph.RegisterPass(
            "WindSimulationInit_Compute",
            FGPassType::eCompute,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );

        graph.RegisterPass(
            "WindSimulationFieldUpdate",
            FGPassType::eCompute,
            []( FGNodeBuilder&, core::Blackboard& ) {

            },
            []( CommandContext&, core::Blackboard& ) {
                // Nothing
            } );
    }

    auto WeatherModule::SetScene( const scene::Scene* scene ) -> void {
        mScene = scene;
    }

    auto WeatherModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }
}// namespace mikoto::renderer