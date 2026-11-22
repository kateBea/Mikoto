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

#include <EASTL/array.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtx/compatibility.hpp>

#include <ozz/base/maths/simd_quaternion.h>
#include <ozz/animation/runtime/ik_aim_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/ik_two_bone_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>

#include <Logging/Logger.hh>

#include <Animation/Animator.hh>

namespace mikoto::animation {
    using namespace core;


    static auto SampleClip(
        SkinnedAnimation& clip, core::f32 time,
        ozz::animation::SamplingJob::Context& context,
        ozz::vector<ozz::math::SoaTransform>& output ) -> bool 
    {
        MKT_BEGIN_PROFILER();
        auto animation{ clip.GetOzzAnimation() };

        if ( !animation || animation->duration() <= 0 ) {
            return false;
        }

        ozz::animation::SamplingJob job{};
        job.animation = animation;
        job.context = &context;
        job.ratio = time / animation->duration();
        job.output = ozz::make_span( output );

        return job.Run();
    }

    static auto AdvanceClipTime( core::f32 time, core::f32 delta, core::f32 duration ) -> core::f32 {
        // Wrap once for the entire character so every rig samples the same
        // instant and long-running sessions retain sub-frame precision.
        return static_cast<core::f32>( glm::fmod( as<core::f64>( time ) + delta, as<core::f64>( duration ) ) );
    }

    static auto IsDirection( const core::float3& value ) -> bool {
        MKT_BEGIN_PROFILER();
        const core::f32 lengthSquared{ glm::dot( value, value ) };
        return lengthSquared > 1e-10f;
    }

    static auto SimdVector( const core::float3& value ) -> ozz::math::SimdFloat4 {
        MKT_BEGIN_PROFILER();
        return ozz::math::simd_float4::Load( value.x, value.y, value.z, 0 );
    }

    static auto IsInvertible( const ozz::math::Float4x4& matrix ) -> bool {
        MKT_BEGIN_PROFILER();
        core::float4x4 value{};

        for ( core::i32 column{}; column < 4; ++column ) {
            ozz::math::StorePtrU( matrix.cols[column], &value[column][0] );
        }

        const core::f32 determinant{ glm::determinant( core::float3x3{ value } ) };
        return glm::abs( determinant ) > 1e-10f;
    }

    static auto ApplyCorrection( 
        core::i32 index, const ozz::math::SimdQuaternion& correction,
        ozz::vector<ozz::math::SoaTransform>& locals ) -> void
    {
        MKT_BEGIN_PROFILER();

        // Ozz stores four joints per SIMD lane group. Transpose only the
        // rotation block, post-multiply this joint, then pack the block back.
        auto& group{ locals[index / 4] };
        
        eastl::array<ozz::math::SimdQuaternion, 4> rotations{};
        ozz::math::Transpose4x4( MKT_ADDRESSOF( group.rotation.x ), MKT_ADDRESSOF( rotations[0].xyzw ) );

        rotations[index & 3] = rotations[index & 3] * correction;
        ozz::math::Transpose4x4( MKT_ADDRESSOF( rotations[0].xyzw ), MKT_ADDRESSOF( group.rotation.x ) );
    }

    Animator::Animator( asset::ModelHandle model )
        : mModel{ model } {
        MKT_BEGIN_PROFILER();

        if ( !mModel.IsEmpty() && mModel->HasArmature() ) {
            mPoses.resize( mModel->GetRigs().size() );

            for ( core::u32 rig{}; rig < mPoses.size(); ++rig ) {
                auto& pose{ mPoses[rig] };
                pose.mContext = ozz::make_unique<ozz::animation::SamplingJob::Context>();
                pose.mFadeContext = ozz::make_unique<ozz::animation::SamplingJob::Context>();

                const auto* skeleton{ mModel->GetSkeleton( rig )->GetOzzSkeleton() };
                pose.mLocalMatrices.assign( skeleton->joint_rest_poses().begin(), skeleton->joint_rest_poses().end() );
                pose.mModelMatrices.resize( skeleton->num_joints() );
                pose.mFinalMatrices.resize( skeleton->num_joints(), core::float4x4{ 1 } );
            }

            EvaluatePose();
        }
    }

    auto Animator::Update( core::f32 deltaTime ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsSuspended || deltaTime < 0.0f ) {
            return;
        }

        if ( mControllerPlayback.IsPlaying() ) {
            const core::f32 duration{ mCurrentAnimation ? mCurrentAnimation->GetDuration() : 0.0f };

            if ( mControllerPlayback.Advance( mController, deltaTime, duration ) ) {
                const auto state{ mController.FindState( mControllerPlayback.GetState() ) };
                auto clip{ state ? mModel->FindAnimation( state->mClip.c_str() ) : nullptr };
                core::f32 blend{ mTransitionDuration };

                for ( const auto& transition: mController.mTransitions ) {
                    if ( transition.mID == mControllerPlayback.GetLastTransition() ) {
                        blend = transition.mBlendDuration;
                        break;
                    }
                }

                if ( !BeginClip( clip, blend ) ) {
                    StopController();
                }

                // The target begins at zero, do not advance it by the source
                // state's elapsed delta again on this transition frame.
                return;
            }

            if ( !mControllerPlayback.IsPlaying() ) {
                StopCurrentAnimation();
            }
        }

        if ( mIsPlaying && mCurrentAnimation ) {
            UpdateOzzAnimation( deltaTime );
        }
        else {
            const auto enabled{ []( const auto& constraint ) {
                return constraint.mEnabled && constraint.mWeight > 0.0f;
            } };
            // Disabling the last constraint restores the uncorrected pose once.
            // A stopped animator with no active IK then needs no pose jobs.
            for ( core::u32 rig{}; rig < mPoses.size(); ++rig ) {
                const auto& pose{ mPoses[rig] };

                if ( pose.mHadIKConstraints || eastl::any_of( pose.mAimConstraints.begin(), pose.mAimConstraints.end(), enabled ) ||
                     eastl::any_of( pose.mTwoBoneConstraints.begin(), pose.mTwoBoneConstraints.end(), enabled ) ) {
                    if ( !EvaluateRig( rig ) ) {
                        StopCurrentAnimation();
                        return;
                    }
                }
            }
        }
    }

    auto Animator::BeginClip( SkinnedAnimation* clip, core::f32 seconds ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( !clip || mModel.IsEmpty() || !mModel->HasArmature() || !clip->GetOzzAnimation() ||
             clip->GetOzzAnimation()->num_tracks() != mModel->GetSkeleton()->GetOzzSkeleton()->num_joints() ) {
            return false;
        }

        // Validate every rig before changing playback; a partially switched
        // character would leave its attachments following a different clip.
        for ( core::u32 rig{}; rig < mPoses.size(); ++rig ) {
            auto* target{ mModel->FindAnimation( clip->GetName(), rig ) };

            if ( !target || !target->GetOzzAnimation() ||
                 target->GetOzzAnimation()->num_tracks() != mModel->GetSkeleton( rig )->GetOzzSkeleton()->num_joints() ||
                 target->GetDuration() <= 0 ||
                 target->GetDuration() != clip->GetDuration() ) {
                return false;
            }
        }

        const core::f32 duration{ eastl::clamp( seconds, 0.0f, 30.0f ) };
        const bool fade{ mCurrentAnimation && duration > 0 };

        for ( core::u32 rig{}; rig < mPoses.size(); ++rig ) {
            auto& pose{ mPoses[rig] };

            if ( fade ) {
                pose.mFadeFromSnapshot = mFadeDuration > 0 || !mIsPlaying;
                pose.mFadeLocals = pose.mLocalMatrices;
                pose.mFadeAnimation = pose.mClip;

                const core::i32 joints{ mModel->GetSkeleton( rig )->GetOzzSkeleton()->num_joints() };

                if ( pose.mFadeContext->max_tracks() < joints ) {
                    pose.mFadeContext->Resize( joints );
                }

                pose.mFadeContext->Invalidate();
                // Only transitions need a second sampled pose. Retain its
                // capacity for later fades without charging ordinary playback.
                pose.mSampledLocals.resize( pose.mLocalMatrices.size() );
            }

            pose.mClip = mModel->FindAnimation( clip->GetName(), rig );
        }

        mFadeTime = mCurrentTime;
        mCurrentAnimation = clip;
        mCurrentTime = 0;
        mFadeElapsed = 0;
        mFadeDuration = fade ? duration : 0;
        mIsPlaying = InitializeOzzAnimation();
        mState = mIsPlaying ? AnimatorState::ePlaying : AnimatorState::eStopped;

        if ( mIsPlaying ) {
            UpdateOzzAnimation( 0 );
        }

        return mIsPlaying;
    }

    auto Animator::CrossFadeAnimation( eastl::string_view name, core::f32 seconds ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( mModel.IsEmpty() ) {
            return false;
        }

        auto clip{ mModel->FindAnimation( name ) };

        if ( !clip ) {
            return false;
        }

        mControllerPlayback.Stop();
        return BeginClip( clip, seconds );
    }

    auto Animator::PlayAnimation( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !CrossFadeAnimation( name, mTransitionDuration ) ) {
            MKT_CORE_LOGGER_WARN( "Cannot play animation {}", name );
        }
    }

    auto Animator::SetCurrentAnimation( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mModel.IsEmpty() ) {
            return;
        }

        auto clip{ mModel->FindAnimation( name ) };

        if ( !clip ) {
            return;
        }

        mControllerPlayback.Stop();

        if ( BeginClip( clip, 0 ) ) {
            StopCurrentAnimation();
        }
    }

    auto Animator::StartCurrentClip() -> void {
        MKT_BEGIN_PROFILER();
        ( void )BeginClip( mCurrentAnimation, 0 );
    }

    auto Animator::PlayCurrentAnimation() -> void {
        MKT_BEGIN_PROFILER();
        mControllerPlayback.Stop();
        StartCurrentClip();
    }

    auto Animator::StopCurrentAnimation() -> void {
        MKT_BEGIN_PROFILER();
        mControllerPlayback.Stop();
        mIsPlaying = false;
        mState = AnimatorState::eStopped;
        mFadeDuration = 0;

        for ( auto& pose: mPoses ) {
            pose.mFadeAnimation = nullptr;
        }
    }

    auto Animator::SetAnimatorState( AnimatorState state ) -> void {
        MKT_BEGIN_PROFILER();

        if ( state == AnimatorState::ePlaying ) {
            PlayCurrentAnimation();
        }
        else {
            StopCurrentAnimation();
        }
    }

    auto Animator::GetAnimatorState() const -> AnimatorState {
        MKT_BEGIN_PROFILER();
        return mState;
    }

    auto Animator::IsAnimatorState( AnimatorState state ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mState == state;
    }

    auto Animator::IsPlaying() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsPlaying;
    }

    auto Animator::GetCurrentAnimation() const -> const SkinnedAnimation* {
        MKT_BEGIN_PROFILER();
        return mCurrentAnimation;
    }

    auto Animator::GetAnimationList() const -> const asset::AnimationList& {
        MKT_BEGIN_PROFILER();
        return mModel->GetAnimations();
    }

    auto Animator::GetModel() const -> asset::ModelHandle {
        MKT_BEGIN_PROFILER();
        return mModel;
    }

    auto Animator::SetSuspended( bool suspended ) -> void {
        MKT_BEGIN_PROFILER();
        mIsSuspended = suspended;
    }

    auto Animator::IsSuspended() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsSuspended;
    }

    auto Animator::GetUnconstrainedJointModelMatrices( core::u32 rig ) const -> const eastl::vector<core::float4x4>& {
        static const eastl::vector<core::float4x4> empty{};

        if ( rig >= mPoses.size() ) {
            return empty;
        }

        const auto& pose{ mPoses[rig] };
        return pose.mUnconstrainedMatrices.empty() ? pose.mFinalMatrices : pose.mUnconstrainedMatrices;
    }

    auto Animator::GetJointModelMatrices( core::u32 rig ) const -> const eastl::vector<core::float4x4>& {
        MKT_BEGIN_PROFILER();
        static const eastl::vector<core::float4x4> empty{};
        return rig < mPoses.size() ? mPoses[rig].mFinalMatrices : empty;
    }

    auto Animator::SetTransitionDuration( core::f32 seconds ) -> void {
        MKT_BEGIN_PROFILER();
        mTransitionDuration = eastl::clamp( seconds, 0.0f, 30.0f );
    }

    auto Animator::GetTransitionDuration() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mTransitionDuration;
    }

    auto Animator::GetAimConstraints( core::u32 rig ) -> eastl::vector<AimIKConstraint>& {
        MKT_BEGIN_PROFILER();
        return mPoses.at( rig ).mAimConstraints;
    }

    auto Animator::GetAimConstraints( core::u32 rig ) const -> const eastl::vector<AimIKConstraint>& {
        MKT_BEGIN_PROFILER();
        return mPoses.at( rig ).mAimConstraints;
    }

    auto Animator::GetTwoBoneConstraints( core::u32 rig ) -> eastl::vector<TwoBoneIKConstraint>& {
        MKT_BEGIN_PROFILER();
        return mPoses.at( rig ).mTwoBoneConstraints;
    }

    auto Animator::GetTwoBoneConstraints( core::u32 rig ) const -> const eastl::vector<TwoBoneIKConstraint>& {
        MKT_BEGIN_PROFILER();
        return mPoses.at( rig ).mTwoBoneConstraints;
    }

    auto Animator::FindJoint( eastl::string_view name, core::u32 rig ) const -> core::i32 {
        MKT_BEGIN_PROFILER();

        if ( mModel.IsEmpty() || !mModel->HasArmature() ) {
            return -1;
        }

        const auto* skeleton{ mModel->GetSkeleton( rig ) };
        const auto* joint{ skeleton ? skeleton->FindJoint( name ) : nullptr };

        if ( joint ) {
            return joint->GetID();
        }

        // Assimp adds a node suffix to preserve duplicate names. Authoring scripts
        // may use the source name only when it resolves to exactly one joint.
        const auto* runtime{ skeleton ? skeleton->GetOzzSkeleton() : nullptr };
        const auto prefix{ string::Format( "{}__node_", name ) };
        core::i32 result{ -1 };

        for ( core::i32 index{}; runtime && index < runtime->num_joints(); ++index ) {
            const eastl::string_view candidate{ runtime->joint_names()[index] };

            if ( candidate.starts_with( prefix ) ) {
                if ( result >= 0 ) {
                    return -1;
                }

                result = index;
            }
        }

        return result;
    }

    auto Animator::PlayController() -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mController.Validate().empty() || mModel.IsEmpty() ) {
            return false;
        }

        for ( const auto& state: mController.mStates ) {
            if ( !mModel->FindAnimation( state.mClip.c_str() ) ) {
                return false;
            }
        }

        if ( !mControllerPlayback.Start( mController ) ) {
            return false;
        }

        const auto state{ mController.FindState( mControllerPlayback.GetState() ) };

        if ( !BeginClip( mModel->FindAnimation( state->mClip.c_str() ), 0 ) ) {
            mControllerPlayback.Stop();
            return false;
        }

        return true;
    }

    auto Animator::StopController() -> void {
        MKT_BEGIN_PROFILER();
        StopCurrentAnimation();
    }

    auto Animator::GetController() -> AnimationController& {
        MKT_BEGIN_PROFILER();
        return mController;
    }

    auto Animator::GetController() const -> const AnimationController& {
        MKT_BEGIN_PROFILER();
        return mController;
    }

    auto Animator::GetControllerPlayback() const -> const ControllerPlayback& {
        MKT_BEGIN_PROFILER();
        return mControllerPlayback;
    }

    auto Animator::InitializeOzzAnimation() -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mCurrentAnimation || mModel.IsEmpty() || !mModel->HasArmature() ) {
            return false;
        }

        for ( core::u32 rig{}; rig < mPoses.size(); ++rig ) {
            auto& pose{ mPoses[rig] };
            const auto skeleton{ mModel->GetSkeleton( rig )->GetOzzSkeleton() };
            const core::i32 joints{ skeleton->num_joints() };
            pose.mLocalMatrices.resize( skeleton->num_soa_joints() );
            pose.mModelMatrices.resize( joints );
            pose.mFinalMatrices.resize( joints, core::float4x4{ 1 } );

            // Resize is a no-op when capacity already fits. SIMD pose storage uses
            // Ozz's allocator, which supplies the alignment required by its jobs.
            if ( pose.mContext->max_tracks() < joints ) {
                pose.mContext->Resize( joints );
            }

            pose.mContext->Invalidate();
        }

        return true;
    }

    auto Animator::UpdateOzzAnimation( core::f32 delta ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mFadeDuration > 0 ) {
            mFadeElapsed = eastl::min( mFadeElapsed + delta, mFadeDuration );
        }

        mCurrentTime = AdvanceClipTime( mCurrentTime, delta, mCurrentAnimation->GetDuration() );

        if ( mFadeDuration > 0 && !mPoses.front().mFadeFromSnapshot ) {
            mFadeTime = AdvanceClipTime( mFadeTime, delta, mPoses.front().mFadeAnimation->GetDuration() );
        }

        for ( core::u32 rig{}; rig < mPoses.size(); ++rig ) {
            auto& pose{ mPoses[rig] };
            const auto skeleton{ mModel->GetSkeleton( rig )->GetOzzSkeleton() };
            auto& output{ mFadeDuration > 0 ? pose.mSampledLocals : pose.mLocalMatrices };

            if ( !SampleClip( *pose.mClip, mCurrentTime, *pose.mContext, output ) ) {
                StopCurrentAnimation();
                return;
            }

            if ( mFadeDuration > 0 ) {
                if ( !pose.mFadeFromSnapshot && !SampleClip( *pose.mFadeAnimation, mFadeTime, *pose.mFadeContext, pose.mFadeLocals ) ) {
                    pose.mFadeFromSnapshot = true;
                }

                const core::f32 weight{ mFadeElapsed / mFadeDuration };
                eastl::array<ozz::animation::BlendingJob::Layer, 2> layers{};
                layers[0].weight = 1.0f - weight;
                layers[0].transform = ozz::make_span( pose.mFadeLocals );
                layers[1].weight = weight;
                layers[1].transform = ozz::make_span( pose.mSampledLocals );

                ozz::animation::BlendingJob blend{};
                blend.layers = ozz::span<const ozz::animation::BlendingJob::Layer>{ layers.data(), layers.size() };
                blend.rest_pose = skeleton->joint_rest_poses();
                blend.output = ozz::make_span( pose.mLocalMatrices );

                if ( !blend.Run() ) {
                    StopCurrentAnimation();
                    return;
                }
            }
        }

        if ( mFadeDuration > 0 && mFadeElapsed >= mFadeDuration ) {
            mFadeDuration = 0;

            for ( auto& pose: mPoses ) {
                pose.mFadeAnimation = nullptr;
            }
        }

        EvaluatePose();
    }

    auto Animator::EvaluatePose() -> void {
        MKT_BEGIN_PROFILER();

        if ( mModel.IsEmpty() || !mModel->HasArmature() ) {
            return;
        }

        for ( core::u32 rig{}; rig < mPoses.size(); ++rig ) {
            if ( !EvaluateRig( rig ) ) {
                StopCurrentAnimation();
                return;
            }
        }
    }

    auto Animator::CopyAnimationPose( eastl::vector<core::float4x4>& matrices, core::u32 rig ) const -> void {
        MKT_BEGIN_PROFILER();

        if ( rig >= mPoses.size() ) {
            matrices.clear();
            return;
        }

        const auto& pose{ mPoses[rig] };
        matrices.resize( pose.mModelMatrices.size() );

        for ( core::usize i{}; i < matrices.size(); ++i ) {
            for ( int column{}; column < 4; ++column ) {
                ozz::math::StorePtrU( pose.mModelMatrices[i].cols[column], &matrices[i][column][0] );
            }
        }
    }

    auto Animator::PublishPhysicsPose( eastl::span<const core::float4x4> matrices, core::f32 weight, core::u32 rig ) -> void {
        MKT_BEGIN_PROFILER();

        if ( rig >= mPoses.size() || matrices.size() != mPoses[rig].mFinalMatrices.size() ) {
            return;
        }

        auto& pose{ mPoses[rig] };
        weight = glm::clamp( weight, 0.0f, 1.0f );

        for ( core::usize i{}; i < matrices.size(); ++i ) {
            core::float4x4 animation{ 1 };

            for ( int column{}; column < 4; ++column ) {
                ozz::math::StorePtrU( pose.mModelMatrices[i].cols[column], &animation[column][0] );
            }
            // Blend against the sampled pose, never last frame's physics result.
            const auto ar{ glm::normalize( glm::quat_cast( core::float3x3{ animation } ) ) };
            const auto pr{ glm::normalize( glm::quat_cast( core::float3x3{ matrices[i] } ) ) };
            const core::float3 scale{ glm::length( core::float3{ animation[0] } ), glm::length( core::float3{ animation[1] } ), glm::length( core::float3{ animation[2] } ) };
            pose.mFinalMatrices[i] = 
                glm::translate( 
                    core::float4x4{ 1 }, 
                    glm::mix( core::float3{ animation[3] }, 
                    core::float3{ matrices[i][3] }, weight ) ) *
                    glm::mat4_cast( glm::slerp( ar, pr, weight ) ) * 
                    glm::scale( core::float4x4{ 1 }, scale );
        }

        ++pose.mRevision;
    }

    auto Animator::GetPoseRevision( core::u32 rig ) const -> core::u64 {
        return rig < mPoses.size() ? mPoses[rig].mRevision : 0;
    }

    auto Animator::EvaluateRig( core::u32 rig ) -> bool {
        MKT_BEGIN_PROFILER();

        auto& pose{ mPoses[rig] };
        const auto skeleton{ mModel->GetSkeleton( rig )->GetOzzSkeleton() };

        ozz::animation::LocalToModelJob model{};
        model.skeleton = skeleton;
        model.input = ozz::make_span( pose.mLocalMatrices );
        model.output = ozz::make_span( pose.mModelMatrices );

        if ( !model.Run() ) {
            return false;
        }

        // Ground probes must follow the animated feet, rather than feed last frame's
        // IK correction back into the next target and gradually pin the walk cycle.
        if ( !pose.mTwoBoneConstraints.empty() || !pose.mAimConstraints.empty() ) {
            pose.mUnconstrainedMatrices.resize( pose.mModelMatrices.size() );

            for ( core::usize joint{}; joint < pose.mModelMatrices.size(); ++joint ) {
                for ( core::i32 column{}; column < 4; ++column ) {
                    ozz::math::StorePtrU( pose.mModelMatrices[joint].cols[column], &pose.mUnconstrainedMatrices[joint][column][0] );
                }
            }
        } else {
            // Animators without IK already expose the unmodified pose through their final matrices.
            pose.mUnconstrainedMatrices.clear();
        }

        ApplyInverseKinematics( rig );

        for ( core::usize i{}; i < pose.mModelMatrices.size(); ++i ) {
            // Preserve animated ancestors; each mesh applies its own inverse binds.
            for ( core::i32 column{}; column < 4; ++column ) {
                ozz::math::StorePtrU( pose.mModelMatrices[i].cols[column], &pose.mFinalMatrices[i][column][0] );
            }
        }

        ++pose.mRevision;
        return true;
    }


    auto Animator::ApplyInverseKinematics( core::u32 rig ) -> void {
        MKT_BEGIN_PROFILER();
        auto& pose{ mPoses[rig] };
        bool enabled{};

        for ( auto& constraint: pose.mAimConstraints ) {
            constraint.mReached = false;
            enabled |= constraint.mEnabled && constraint.mWeight > 0;
        }

        for ( auto& constraint: pose.mTwoBoneConstraints ) {
            constraint.mReached = false;
            enabled |= constraint.mEnabled && constraint.mWeight > 0;
        }

        pose.mHadIKConstraints = enabled;

        if ( !enabled ) {
            return;
        }

        const auto skeleton{ mModel->GetSkeleton( rig )->GetOzzSkeleton() };
        const auto validJoint{ [&]( core::i32 joint ) {
            return joint >= 0 && joint < skeleton->num_joints();
        } };

        const auto isAncestor{ [&]( core::i32 parent, core::i32 child ) {
            for ( core::i32 joint{ skeleton->joint_parents()[child] }; 
                joint != ozz::animation::Skeleton::kNoParent; 
                joint = skeleton->joint_parents()[joint] ) 
            {
                if ( joint == parent ) {
                    return true;
                }
            }

            return false;
        } };

        // Preserve the blended pose for interrupted fades. IK never feeds its
        // own correction back into next frame's sampled/blended animation.
        pose.mIKLocals = pose.mLocalMatrices;
        const auto updateModels{ [&]( core::i32 from ) {
            ozz::animation::LocalToModelJob job{};
            job.skeleton = skeleton;
            job.input = ozz::make_span( pose.mIKLocals );
            job.output = ozz::make_span( pose.mModelMatrices );
            job.from = from;
            return job.Run();
        } };

        // Rebuild model matrices after each constraint so later constraints see
        // the corrected chain rather than the original pose. Bone constraints
        // run first, followed by aim constraints (hands/head can refine a chain).
        for ( auto& constraint: pose.mTwoBoneConstraints ) {
            if ( !constraint.mEnabled || constraint.mWeight <= 0 ||
                 !validJoint( constraint.mStart ) || !validJoint( constraint.mMiddle ) || !validJoint( constraint.mEnd ) ||
                 !isAncestor( constraint.mStart, constraint.mMiddle ) || !isAncestor( constraint.mMiddle, constraint.mEnd ) ||
                 !IsDirection( constraint.mMiddleAxis ) || !IsDirection( constraint.mPole ) ||
                 !IsInvertible( pose.mModelMatrices[constraint.mStart] ) || !IsInvertible( pose.mModelMatrices[constraint.mMiddle] ) ) {
                continue;
            }

            ozz::animation::IKTwoBoneJob job{};
            job.target = SimdVector( constraint.mTarget );
            job.mid_axis = SimdVector( glm::normalize( constraint.mMiddleAxis ) );
            job.pole_vector = SimdVector( glm::normalize( constraint.mPole ) );
            job.weight = eastl::clamp( constraint.mWeight, 0.0f, 1.0f );
            job.soften = eastl::clamp( constraint.mSoften, 0.0f, 1.0f );
            job.twist_angle = constraint.mTwist;
            job.start_joint = &pose.mModelMatrices[constraint.mStart];
            job.mid_joint = &pose.mModelMatrices[constraint.mMiddle];
            job.end_joint = &pose.mModelMatrices[constraint.mEnd];

            ozz::math::SimdQuaternion start{}, middle{};
            job.start_joint_correction = &start;
            job.mid_joint_correction = &middle;
            job.reached = &constraint.mReached;

            if ( job.Run() ) {
                ApplyCorrection( constraint.mStart, start, pose.mIKLocals );
                ApplyCorrection( constraint.mMiddle, middle, pose.mIKLocals );
                ( void )updateModels( constraint.mStart );
            }
        }

        for ( auto& constraint: pose.mAimConstraints ) {
            if ( !constraint.mEnabled || constraint.mWeight <= 0 ||
                 !validJoint( constraint.mJoint ) ||
                 !IsDirection( constraint.mForward ) || !IsDirection( constraint.mUp ) || !IsDirection( constraint.mPole ) ||
                 !IsInvertible( pose.mModelMatrices[constraint.mJoint] ) ) {
                continue;
            }

            ozz::animation::IKAimJob job{};
            job.target = SimdVector( constraint.mTarget );
            job.forward = SimdVector( glm::normalize( constraint.mForward ) );
            job.up = SimdVector( glm::normalize( constraint.mUp ) );
            job.pole_vector = SimdVector( glm::normalize( constraint.mPole ) );
            job.offset = SimdVector( constraint.mOffset );
            job.weight = eastl::clamp( constraint.mWeight, 0.0f, 1.0f );
            job.twist_angle = constraint.mTwist;
            job.joint = &pose.mModelMatrices[constraint.mJoint];

            ozz::math::SimdQuaternion correction{};
            job.joint_correction = &correction;
            job.reached = &constraint.mReached;

            if ( job.Run() ) {
                ApplyCorrection( constraint.mJoint, correction, pose.mIKLocals );
                ( void )updateModels( constraint.mJoint );
            }
        }
    }
}// namespace mikoto::animation
