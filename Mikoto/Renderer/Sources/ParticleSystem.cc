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

#include <Scene/Scene.hh>
#include <Scene/Component.hh>

#include <Renderer/Particle/ParticleSystem.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;

    ParticleSystem::ParticleSystem( const ParticleSystemCreateInfo& ) {
    }

    auto ParticleSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mIsInitialized = true;
    }

    auto ParticleSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        mScenes.clear();
        mIsInitialized = false;
    }

    auto ParticleSystem::Update( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        // The engine ticks each registered scene once, independent of how many
        // viewports render it. Idle scenes retain their paused particle state.
        for ( scene::Scene* scene: mScenes ) {
            if ( !scene->IsPlaying() && !scene->IsSimulating() ) {
                continue;
            }

            // Physics and scripting finish before this service. Publish any
            // transform edits before spawning world-space particles.
            scene->UpdateWorldTransformations();

            for ( auto [entity, simulator, transform, tag]:
                scene->GetRegistry().view<scene::ParticleSimulatorComponent, scene::TransformComponent, scene::TagComponent>().each() ) {
                if ( tag.IsActive() ) {
                    simulator.Update( timeStep, transform.GetWorldTransform(), tag.GetGuid(), mSimulationMode );
                }
            }
        }
    }

    auto ParticleSystem::RegisterScene( scene::Scene& scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScenes.insert( &scene );
    }

    auto ParticleSystem::UnRegisterScene( scene::Scene& scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScenes.erase( &scene );
    }

    auto ParticleSystem::SetSimulationMode( ParticleSimulationMode mode ) -> void {
        MKT_BEGIN_PROFILER();
        mSimulationMode = mode == ParticleSimulationMode::eGpu ? mode : ParticleSimulationMode::eCpu;
    }

    auto ParticleSystem::GetSimulationMode() const -> ParticleSimulationMode {
        MKT_BEGIN_PROFILER();
        return mSimulationMode;
    }
}// namespace mikoto::renderer
