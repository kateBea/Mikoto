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

#ifndef MIKOTO_ANIMATION_SYSTEM_HH
#define MIKOTO_ANIMATION_SYSTEM_HH

#include <vector>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Singleton.hh>
#include <Common/Subsystem.hh>

#include <Assets/Model.hh>
#include <Animation/Animator.hh>
#include <Animation/SkinnedAnimation.hh>

namespace Mikoto {

    struct AnimationSystemCreateInfo {

    };

    class AnimationSystem final : public Subsystem, public Singleton<AnimationSystem>{
    public:

        explicit AnimationSystem(const AnimationSystemCreateInfo& createInfo);

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        // Valid IDs start from 1
        auto RegisterAnimation( ModelHandle handle ) -> UInt64;

        MKT_NODISCARD auto GetAnimator( UInt64 id ) -> Animator*;

    private:
        // Not sure if animators should be wrapped into pointers
        // In case we add more animator while reading from this map
        // Reallocations invalidate all references
        ankerl::unordered_dense::map<UInt64, Animator> m_Animators{};
    };

}

#endif//MIKOTO_ANIMATION_SYSTEM_HH
