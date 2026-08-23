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

#ifndef MIKOTO_SIMULATIONS_MODULE_HH
#define MIKOTO_SIMULATIONS_MODULE_HH


#include <Renderer/Rhi/Types.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/GeometryCullModule.hh>


namespace mikoto::renderer {

    struct SimulationsModuleInfo {

    };

    class SimulationsModule final {
    public:
        explicit SimulationsModule( rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryCullModule* mGeometryCullModule{};

        rhi::RenderResolution mResolution { rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_SIMULATIONS_MODULE_HH
