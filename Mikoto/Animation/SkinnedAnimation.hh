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

#include <limits>
#include <numeric>

#include <ankerl/unordered_dense.h>

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/base/memory/unique_ptr.h"

#include <Common/Common.hh>
#include <Animation/Skeleton.hh>
#include <Library/Utility/Types.hh>

namespace  Mikoto {

    enum InterpolationType { LINEAR,
                             STEP,
                             CUBICSPLINE };

    enum PathType { TRANSLATION,
                    ROTATION,
                    SCALE };

    struct AnimationSampler {
        InterpolationType Interpolation{ InterpolationType::LINEAR };

        std::vector<Vec3F> Scales{};
        std::vector<Vec3F> Positions{};
        std::vector<Quat> Rotations{};

        // This is kept because of how GLTF stores sampler values
        std::vector<float> Outputs{};           // Key frame values (for rotations)
        std::vector<glm::vec4> OutputsVec4{};   // Key frame values (for translations and scales)

        std::vector<float> TimeStamps{};
    };

    struct AnimationChannel {
        PathType Path{ PathType::TRANSLATION };
        Int32 SamplerIndex{};

        std::string NodeName{};
        Int32 JointIndex{};
    };

    struct AnimationDescription {
        // Duration of the animation in ticks
        std::string Name{};

        std::vector<AnimationSampler> Samplers{};
        std::vector<AnimationChannel> Channels{};

        float Start{ std::numeric_limits<float>::max() };
        float End{ std::numeric_limits<float>::min() };
    };

    class SkinnedAnimation {
    public:
        explicit SkinnedAnimation();

        MKT_NODISCARD auto GetDuration() const -> float;

        MKT_NODISCARD auto GetName() const -> const std::string&;
        MKT_NODISCARD auto GetOzzAnimation() -> ozz::animation::Animation*;

    private:
        auto ResolveSamplers() -> void;

    private:
        // Duration of the animation in ticks
        std::string m_Name{};

        float m_Duration{}; // In seconds

        std::vector<AnimationSampler> m_Samplers{};
        std::vector<AnimationChannel> m_Channels{};

        float m_End{ std::numeric_limits<float>::min() };
        float m_Start{ std::numeric_limits<float>::max() };

        // To construct the animation
        ozz::animation::offline::RawAnimation m_RawAnimation{};
        ozz::animation::offline::AnimationBuilder m_AnimationBuilder{};

        // Final runtime animation
        ozz::unique_ptr<ozz::animation::Animation> m_Animation{};
    };
}

#endif//MIKOTO_ANIMATION_HH
