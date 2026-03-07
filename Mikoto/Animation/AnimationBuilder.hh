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

#ifndef MIKOTO_ANIMATION_BUILDER_HH
#define MIKOTO_ANIMATION_BUILDER_HH

#include <vector>

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/base/memory/unique_ptr.h>

namespace Mikoto {

    using AnimationList = std::vector<ozz::unique_ptr<ozz::animation::Animation>>;

    class AnimationBuilder {
    public:
        virtual ~AnimationBuilder() = default;

        auto GetAnimations() -> AnimationList&;

        virtual auto Build() -> bool = 0;

    private:
        // To construct the animations
        ozz::animation::offline::RawAnimation m_RawAnimation{};
        ozz::animation::offline::AnimationBuilder m_AnimationBuilder{};

        std::vector<ozz::unique_ptr<ozz::animation::Animation>> m_Animations{};

    };
}


#endif // MIKOTO_ANIMATION_BUILDER_HH
