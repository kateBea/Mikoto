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

#ifndef MIKOTO_ANIMATION_HH
#define MIKOTO_ANIMATION_HH

#include <limits>

#include <EASTL/string.h>
#include <EASTL/numeric.h>

#include <ankerl/unordered_dense.h>

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/Skeleton.hh>

namespace  mikoto::animation {

    static inline constexpr u32 kMaxBonesPerVertex{ 8 };

    class SkinnedAnimation {
    public:
        explicit SkinnedAnimation( ozz::unique_ptr<ozz::animation::Animation>&& data = nullptr );

        MKT_NODISCARD auto GetDuration() const -> f32;

        MKT_NODISCARD auto GetName() const -> const eastl::string&;
        MKT_NODISCARD auto GetOzzAnimation() -> ozz::animation::Animation*;

    private:
        // Duration of the animation in ticks
        eastl::string mName{};

        f32 mDuration{}; // In seconds

        f32 mEnd{ eastl::numeric_limits<f32>::min() };
        f32 mStart{ eastl::numeric_limits<f32>::max() };

        // To construct the animation
        ozz::animation::offline::RawAnimation mRawAnimation{};
        ozz::animation::offline::AnimationBuilder mAnimationBuilder{};

        // Final runtime animation
        ozz::unique_ptr<ozz::animation::Animation> mAnimation{};
    };
}

#endif//MIKOTO_ANIMATION_HH
