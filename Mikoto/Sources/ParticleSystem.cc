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
#include <Logging/Logger.hh>

#include <Renderer/Particle/ParticleSystem.hh>

namespace mikoto {

    renderer::ParticleSystem::ParticleSystem( const ParticleSystemCreateInfo & ) {

    }

    auto renderer::ParticleSystem::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_DEBUG( "Initializing ParticleSystem" );

        mIsInitialized = true;
    }

    auto renderer::ParticleSystem::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down ParticleSystem" );
    }

    auto renderer::ParticleSystem::Update( float ts ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }
}