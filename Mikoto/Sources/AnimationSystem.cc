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

#include <array>
#include <cstdlib>

#include "ozz/base/log.h"

// Provides files abstraction.
#include "ozz/base/io/stream.h"

// Provides serialization/deserialization mechanism.
#include "ozz/base/io/archive.h"

// Uses the skeleton as an example of object to read.
#include "ozz/animation/runtime/skeleton.h"

#include <cstdlib>
#include <ranges>

#include <Logging/Logger.hh>
#include <Animation/AnimationSystem.hh>

namespace Mikoto {

    AnimationSystem::AnimationSystem( const AnimationSystemCreateInfo & ) {}

    auto AnimationSystem::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing AnimationSystem...");

        m_IsInitialized = true;
    }

    auto AnimationSystem::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down AnimationSystem..." );
    }

    auto AnimationSystem::Update( float dt ) -> void {
        for ( auto& animator : m_Animators ) {
            animator.UpdateAnimation( dt );
        }
    }

    auto AnimationSystem::RegisterAnimation(SkinnedAnimation& animation) -> void {
        m_Animators.emplace_back( animation );
    }
}
