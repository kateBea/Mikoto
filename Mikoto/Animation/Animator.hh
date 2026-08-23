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

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/sampling_job.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/Model.hh>
#include <Animation/SkinnedAnimation.hh>

namespace mikoto::animation {

    using namespace mikoto::asset;

    enum class AnimatorState {
        ePlaying,
        eStopped,
    };

    class Animator {
    public:
        explicit Animator( ModelHandle handle );

        auto Update( float deltaTime ) -> void;

        auto GetFinalBoneMatrices() const -> const eastl::vector<float4x4>&;
        auto GetInverseBindMatrices() const -> const eastl::vector<float4x4>&;

        auto SetAnimatorState( AnimatorState state ) -> void;

        MKT_NODISCARD auto GetAnimatorState() const -> AnimatorState;
        MKT_NODISCARD auto IsAnimatorState( AnimatorState state ) const -> bool;

        MKT_NODISCARD auto GetCurrentAnimation() const -> const SkinnedAnimation*;
        MKT_NODISCARD auto GetAnimationList() const -> const AnimationList&;

        auto StopCurrentAnimation() -> void;
        auto PlayCurrentAnimation() -> void;

        auto PlayAnimation( eastl::string_view name ) -> void;
        auto SetCurrentAnimation( eastl::string_view name ) -> void;

        MKT_NODISCARD auto IsPlaying() const -> bool;

    private:

        auto UpdateOzzAnimation( float ts ) -> void;
        auto InitializeOzzAnimation() -> void;

    private:
        ModelHandle mModel{};

        f32 mCurrentTime{};

        SkinnedAnimation* mCurrentAnimation{};

        eastl::vector<float4x4> mFinalMatrices{};

        // Buffer of model space matrices.
        ozz::vector<ozz::math::Float4x4> mModelMatrices{};

        // Buffer of local transforms as sampled from the animation.
        ozz::vector<ozz::math::SoaTransform> mLocalMatrices{};

        ozz::unique_ptr<ozz::animation::SamplingJob::Context> mContext{};

        AnimatorState mState{ AnimatorState::eStopped };

        bool m_IsPlaying{ false };
    };
}

#endif//MIKOTO_ANIMATOR_H
