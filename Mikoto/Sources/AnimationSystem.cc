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
#include <ranges>

#include <Logging/Logger.hh>
#include <Filesystem/FileSystem.hh>
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

        m_Animators.clear();
    }

    auto AnimationSystem::Update( float dt ) -> void {
        for ( auto& animator : m_Animators | std::ranges::views::values ) {
            animator.UpdateAnimation( dt );
        }
    }

    auto AnimationSystem::RegisterAnimation( ModelHandle handle ) -> UInt64 {
        UInt64 animatorID{ m_Animators.size() + 1 };
        m_Animators.try_emplace( animatorID, handle );

        return animatorID;
    }

    auto AnimationSystem::GetAnimator( UInt64 id ) -> Animator * {
        const auto it{ m_Animators.find( id ) };
        if ( it != m_Animators.end() ) {
            return std::addressof( it->second );
        }

        return nullptr;
    }
}// namespace Mikoto
