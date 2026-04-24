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

#include <ranges>

#include <Core/Core.hh>
#include <Logging/Logger.hh>
#include <Animation/AnimationSystem.hh>

namespace mikoto::animation {

    AnimationSystem::AnimationSystem( const AnimationSystemCreateInfo & ) {}

    auto AnimationSystem::Initialize() -> void {
        MKT_CORE_LOGGER_INFO("Initializing AnimationSystem...");
        mIsInitialized = true;
    }

    auto AnimationSystem::Shutdown() -> void {
        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down AnimationSystem..." );

        mAnimators.clear();
    }

    auto AnimationSystem::Update( float dt ) -> void {
        for ( auto& animator : mAnimators | std::ranges::views::values ) {
            animator.Update( dt );;
        }
    }

    auto AnimationSystem::RegisterAnimation( ModelHandle handle ) -> u64 {
        u64 animatorID{ mAnimators.size() + 1 };
        mAnimators.try_emplace( animatorID, handle );

        return animatorID;
    }

    auto AnimationSystem::GetAnimator( u64 id ) -> Animator * {
        const auto it{ mAnimators.find( id ) };
        if ( it != mAnimators.end() ) {
            return std::addressof( it->second );
        }

        return nullptr;
    }
}// namespace Mikoto