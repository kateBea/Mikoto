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

#include <Memory/Allocator.hh>

#include <Renderer/Passes/IndirectLightingModule.hh>

namespace mikoto::renderer {

    IndirectLightingModule::IndirectLightingModule( rhi::RenderResolution resolution )
        : mResolution{ resolution }
    {

    }

    auto IndirectLightingModule::SetScene( const scene::Scene *scene ) -> void {
        mScene = scene;
    }

    auto IndirectLightingModule::SetCamera( const scene::Camera *camera ) -> void {
        mCamera = camera;
    }

    auto IndirectLightingModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        mGeometryCullModule = MKT_ADDRESSOF( geom );
    }

    auto IndirectLightingModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }
}// namespace mikoto::renderer