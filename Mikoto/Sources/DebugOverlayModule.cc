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

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>

#include <Renderer/Passes/CameraModule.hh>
#include <Renderer/Passes/PrepassModule.hh>
#include <Renderer/Passes/PostProcessModule.hh>
#include <Renderer/Passes/GeometryShadingModule.hh>

#include <Renderer/Passes/DebugOverlayModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    DebugOverlayModule::DebugOverlayModule( rhi::RenderResolution resolution )
        : mResolution{ resolution }
    {
    }

    auto DebugOverlayModule::SetScene( const scene::Scene *scene ) -> void {
        mScene = scene;
    }

    auto DebugOverlayModule::SetCamera( const scene::Camera *camera ) -> void {
        mCamera = camera;
    }

    auto DebugOverlayModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        mGeometryCullModule = MKT_ADDRESSOF( geom );
    }

    auto DebugOverlayModule::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterDebugFilter( graph );
        RegisterIconsDisplay( graph );
        RegisterPhysicsDebugger( graph );
    }
    auto DebugOverlayModule::RegisterDebugFilter( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // We filter what items need their collider rendered,
        // what items need their icons rendered, etc
        graph.RegisterPass(
            "DebugOverlay_Filter",
            FGPassType::eTransfer,
            []( FGNodeBuilder &, Blackboard & ) {

            },
            []( CommandContext &, Blackboard & ) {
                // Nothing
            } );
    }

    auto DebugOverlayModule::RegisterIconsDisplay( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "DebugOverlayIcons_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );
    }

    auto DebugOverlayModule::RegisterPhysicsDebugger( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "DebugOverlayColliders_Render",
            FGPassType::eGraphics,
            []( FGNodeBuilder&, Blackboard & ) {

            },
            []( CommandContext&, Blackboard & ) {
                // Nothing
            } );
    }
}// namespace mikoto::renderer