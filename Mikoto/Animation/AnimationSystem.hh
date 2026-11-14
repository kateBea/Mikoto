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

#ifndef MIKOTO_ANIMATION_ANIMATION_SYSTEM_HH
#define MIKOTO_ANIMATION_ANIMATION_SYSTEM_HH

#include <atomic>

#include <ankerl/unordered_dense.h>

#include <taskflow/taskflow.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Animation/Animator.hh>

namespace mikoto::animation {

    struct AnimationSystemCreateInfo {

    };

    class AnimationSystem final : public core::ISubsystem, public core::Singleton<AnimationSystem> {
    public:
        explicit AnimationSystem( const AnimationSystemCreateInfo& createInfo );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float dt ) -> void override;

        auto UnregisterAnimation( core::u32 id ) -> void;

        // Valid IDs start from 1
        MKT_NODISCARD auto RegisterAnimation( asset::ModelHandle handle ) -> core::u32;

        MKT_NODISCARD auto GetAnimator( core::u32 id ) -> Animator*;

    private:
        eastl::vector<Animator*> mUpdateList{};
        std::atomic<core::usize> mNextUpdate{};

        core::f32 mUpdateDelta{};
        core::u32 mNextAnimatorID{ 1 };
        core::usize mUpdateTaskCount{};

        tf::Taskflow mUpdateTasks{};

        // Keep animator addresses stable when the ankerl table grows or compacts.
        ankerl::unordered_dense::map<core::u64, eastl::unique_ptr<Animator>> mAnimators{};
    };
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_ANIMATION_SYSTEM_HH
