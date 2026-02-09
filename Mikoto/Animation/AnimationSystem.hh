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

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Subsystem.hh>
#include <Common/Singleton.hh>

#include <Animation/SkeletalAnimation.hh>

namespace Mikoto {

    struct AnimationSystemCreateInfo {

    };

    class AnimationSystem final : public Subsystem, public Singleton<AnimationSystem>{
    public:

        explicit AnimationSystem(const AnimationSystemCreateInfo& createInfo);

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void override;

        MKT_NODISCARD auto RegisterAnimation( /*TODO: params*/ ) -> UInt64;

    private:
        ankerl::unordered_dense::map<UInt64, SkeletalAnimation> m_Animations{};
    };

}

#endif//MIKOTO_ANIMATION_SYSTEM_HH
