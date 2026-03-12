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

#ifndef MIKOTO_ANIMATOR_H
#define MIKOTO_ANIMATOR_H

#include <ankerl/unordered_dense.h>

#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/vec_float.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>

#include <ozz/base/log.h>
#include <ozz/options/options.h>

#include <Library/Utility/Types.hh>

#include <Assets/Model.hh>
#include <Animation/SkinnedAnimation.hh>

namespace Mikoto {

    class Animator {
    public:
        explicit Animator( ModelHandle handle );

        auto UpdateAnimation( float deltaTime ) -> void;

        auto GetFinalBoneMatrices() -> auto& { return m_FinalMatrices; }
        auto GetInverseBindMatrices() -> auto& { return m_Model->GetSkeleton().GetInverseBindMatrices(); }

        MKT_NODISCARD auto GetCurrentAnimation() const -> const SkinnedAnimation*;
        MKT_NODISCARD auto GetAnimationList() const -> const auto& { return m_Model->GetAnimations(); }

        auto StopCurrentAnimation() -> void;
        auto PlayCurrentAnimation() -> void;

        auto PlayAnimation( std::string_view name ) -> void;
        auto SetCurrentAnimation( std::string_view name ) -> void;

        MKT_NODISCARD auto IsPlaying() const -> bool;

        auto UpdateOzzAnimation( float ts ) -> void;
        auto InitializeOzzAnimation() -> void;

    private:
        ModelHandle m_Model{};

        UInt64 m_AnimationID{};
        float m_CurrentTime{};

        bool m_IsPlaying{ false };
        SkinnedAnimation* m_CurrentAnimation{};

        // Buffer of local transforms as sampled from the animation.
        ozz::vector<ozz::math::SoaTransform> m_LocalMatrices{};

        // Buffer of model space matrices.
        ozz::vector<ozz::math::Float4x4> m_ModelMatrices{};

        std::vector<Mat4F> m_FinalMatrices{};

        ozz::unique_ptr<ozz::animation::SamplingJob::Context> m_Context{};
    };
}

#endif//MIKOTO_ANIMATOR_H
