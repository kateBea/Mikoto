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

#ifndef MIKOTO_ANIMATION_SKINNED_ANIMATION_HH
#define MIKOTO_ANIMATION_SKINNED_ANIMATION_HH

#include <EASTL/string.h>

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/animation/runtime/animation.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/Skeleton.hh>

namespace mikoto::animation {

    static inline constexpr core::u32 kMaxBonesPerVertex{ 8 };

    class SkinnedAnimation {
    public:
        explicit SkinnedAnimation( ozz::unique_ptr<ozz::animation::Animation>&& data = nullptr );

        MKT_NODISCARD auto GetDuration() const -> core::f32;

        MKT_NODISCARD auto GetName() const -> const eastl::string&;
        MKT_NODISCARD auto GetOzzAnimation() -> ozz::animation::Animation*;

    private:
        // Runtime clip metadata; offline builders belong to the importer.
        eastl::string mName{};
        core::f32 mDuration{};// In seconds

        // Final runtime animation
        ozz::unique_ptr<ozz::animation::Animation> mAnimation{};
    };
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_SKINNED_ANIMATION_HH
