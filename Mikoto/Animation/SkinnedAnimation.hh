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

#ifndef MIKOTO_ANIMATION_HH
#define MIKOTO_ANIMATION_HH

#include <ankerl/unordered_dense.h>

#include <Animation/Skeleton.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/base/memory/unique_ptr.h"

namespace  Mikoto {

    class SkinnedAnimation {
    public:
        explicit SkinnedAnimation( std::string_view name, float duration, UInt32 ticksPerSecond );

        auto GetDuration() const -> float;
        auto GetTicksPerSecond() const -> float;

        MKT_NODISCARD auto GetName() const -> const std::string&;
        MKT_NODISCARD auto GetOzzAnimation() -> ozz::animation::Animation*;

    private:
        // Duration of the animation in ticks
        float m_Duration{};
        UInt32 m_TicksPerSecond{};
        std::string m_Name{};

        // To construct the animation
        ozz::animation::offline::RawAnimation m_RawAnimation{};
        ozz::animation::offline::AnimationBuilder m_AnimationBuilder{};

        // Final runtime animation
        ozz::unique_ptr<ozz::animation::Animation> m_Animation{};
    };
}

#endif//MIKOTO_ANIMATION_HH
