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

#ifndef MIKOTO_ANIMATION_ANIMATOR_HH
#define MIKOTO_ANIMATION_ANIMATOR_HH

#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <ozz/base/maths/simd_math.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/sampling_job.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/AssetModel.hh>

#include <Animation/SkinnedAnimation.hh>
#include <Animation/AnimationController.hh>

namespace mikoto::animation {

    // Targets and pole directions use skeleton model space. Joint axes and
    // offsets use the corresponding joint's local space, matching Ozz jobs.
    struct AimIKConstraint {
        core::i32 mJoint{ -1 };
        core::float3 mTarget{};
        core::float3 mForward{ 1, 0, 0 };
        core::float3 mUp{ 0, 1, 0 };
        core::float3 mPole{ 0, 1, 0 };
        core::float3 mOffset{};
        core::f32 mWeight{ 1.0f };
        core::f32 mTwist{};

        bool mEnabled{ true };
        bool mReached{};
    };

    struct TwoBoneIKConstraint {
        core::i32 mStart{ -1 };
        core::i32 mMiddle{ -1 };
        core::i32 mEnd{ -1 };
        core::float3 mTarget{};
        core::float3 mMiddleAxis{ 0, 0, 1 };
        core::float3 mPole{ 0, 1, 0 };
        core::f32 mWeight{ 1.0f };
        core::f32 mSoften{ 1.0f };
        core::f32 mTwist{};

        bool mEnabled{ true };
        bool mReached{};
    };

    enum class AnimatorState {
        ePlaying,
        eStopped,
    };

    class Animator {
    public:
        explicit Animator( asset::ModelHandle handle );

        auto Update( core::f32 deltaTime ) -> void;

        // Rebuild model-space matrices from the current local pose, then apply IK.
        // This also supports a stationary/rest pose without advancing a clip.
        auto EvaluatePose() -> void;

        auto SetSuspended( bool suspended ) -> void;
        auto SetAnimatorState( AnimatorState state ) -> void;
        
        auto CopyAnimationPose( eastl::vector<core::float4x4> &matrices, core::u32 rig = 0 ) const -> void;
        auto PublishPhysicsPose( eastl::span<const core::float4x4> matrices, core::f32 weight, core::u32 rig = 0 ) -> void;

        MKT_NODISCARD auto GetModel() const -> asset::ModelHandle;
        MKT_NODISCARD auto IsSuspended() const -> bool;
        MKT_NODISCARD auto GetJointModelMatrices( core::u32 rig = 0 ) const -> const eastl::vector<core::float4x4>&;

        MKT_NODISCARD auto GetPoseRevision( core::u32 rig = 0 ) const -> core::u64;

        MKT_NODISCARD auto GetAnimatorState() const -> AnimatorState;
        MKT_NODISCARD auto IsAnimatorState( AnimatorState state ) const -> bool;

        MKT_NODISCARD auto GetCurrentAnimation() const -> const SkinnedAnimation*;
        MKT_NODISCARD auto GetAnimationList() const -> const asset::AnimationList&;

        auto StopController() -> void;

        auto StopCurrentAnimation() -> void;
        auto PlayCurrentAnimation() -> void;

        auto PlayAnimation( eastl::string_view name ) -> void;
        auto SetTransitionDuration( core::f32 seconds ) -> void;
        auto SetCurrentAnimation( eastl::string_view name ) -> void;

        MKT_NODISCARD auto GetTransitionDuration() const -> core::f32;
        MKT_NODISCARD auto CrossFadeAnimation( eastl::string_view name, core::f32 seconds ) -> bool;

        /** IK indices are local to the selected rig; rig must exist in the model. */
        MKT_NODISCARD auto GetAimConstraints( core::u32 rig = 0 ) -> eastl::vector<AimIKConstraint>&;
        MKT_NODISCARD auto GetAimConstraints( core::u32 rig = 0 ) const -> const eastl::vector<AimIKConstraint>&;
        MKT_NODISCARD auto GetTwoBoneConstraints( core::u32 rig = 0 ) -> eastl::vector<TwoBoneIKConstraint>&;
        MKT_NODISCARD auto GetTwoBoneConstraints( core::u32 rig = 0 ) const -> const eastl::vector<TwoBoneIKConstraint>&;

        MKT_NODISCARD auto FindJoint( eastl::string_view name, core::u32 rig = 0 ) const -> core::i32;

        MKT_NODISCARD auto GetController() -> AnimationController&;
        MKT_NODISCARD auto GetController() const -> const AnimationController&;
        MKT_NODISCARD auto GetControllerPlayback() const -> const ControllerPlayback&;
        MKT_NODISCARD auto PlayController() -> bool;

        MKT_NODISCARD auto IsPlaying() const -> bool;

    private:
        // [Internal usage]
        MKT_NODISCARD auto InitializeOzzAnimation() -> bool;
        MKT_NODISCARD auto EvaluateRig( core::u32 rig ) -> bool;

        MKT_NODISCARD auto BeginClip( SkinnedAnimation* clip, core::f32 seconds ) -> bool;

        auto StartCurrentClip() -> void;
        auto UpdateOzzAnimation( core::f32 ts ) -> void;

        auto ApplyInverseKinematics( core::u32 rig ) -> void;

    private:
        // Playback time/controller are shared. Sampling contexts, poses and IK
        // belong to each rig and each character instance, never to the asset.
        struct RigPose {
            eastl::vector<core::float4x4> mFinalMatrices{};
            ozz::vector<ozz::math::Float4x4> mModelMatrices{};
            ozz::vector<ozz::math::SoaTransform> mLocalMatrices{};
            ozz::vector<ozz::math::SoaTransform> mSampledLocals{};
            ozz::vector<ozz::math::SoaTransform> mFadeLocals{};
            ozz::vector<ozz::math::SoaTransform> mIKLocals{};
            ozz::unique_ptr<ozz::animation::SamplingJob::Context> mContext{};
            ozz::unique_ptr<ozz::animation::SamplingJob::Context> mFadeContext{};

            SkinnedAnimation* mClip{};
            SkinnedAnimation* mFadeAnimation{};

            bool mFadeFromSnapshot{};
            bool mHadIKConstraints{};

            core::u64 mRevision{};

            eastl::vector<AimIKConstraint> mAimConstraints{};
            eastl::vector<TwoBoneIKConstraint> mTwoBoneConstraints{};
        };

    private:
        asset::ModelHandle mModel{};

        AnimationController mController{};
        ControllerPlayback mControllerPlayback{};

        core::f32 mCurrentTime{};

        SkinnedAnimation* mCurrentAnimation{};

        eastl::vector<RigPose> mPoses{};

        core::f32 mFadeTime{};
        core::f32 mFadeElapsed{};
        core::f32 mFadeDuration{};
        core::f32 mTransitionDuration{ 0.2f };

        AnimatorState mState{ AnimatorState::eStopped };

        bool mIsSuspended{};
        bool mIsPlaying{ false };
    };
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_ANIMATOR_HH
