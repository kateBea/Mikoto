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

#ifndef MIKOTO_ANIMATION_SYSTEM_HH
#define MIKOTO_ANIMATION_SYSTEM_HH

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Singleton.hh>
#include <Core/Subsystem.hh>

#include <Animation/Animator.hh>

namespace mikoto::animation {

    using namespace mikoto::core;
    using namespace mikoto::asset;

    struct AnimationSystemCreateInfo {};

    class AnimationSystem final : public ISubsystem, public Singleton<AnimationSystem> {
    public:

        explicit AnimationSystem(const AnimationSystemCreateInfo& createInfo);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        // Valid IDs start from 1
        auto RegisterAnimation( ModelHandle handle ) -> u64;

        MKT_NODISCARD auto GetAnimator( u64 id ) -> Animator*;

    private:
        // Not sure if animators should be wrapped into pointers
        // In case we add more animator while reading from this map
        // Reallocations invalidate all references
        ankerl::unordered_dense::map<u64, Animator> mAnimators{};
    };
}

#endif//MIKOTO_ANIMATION_SYSTEM_HH
