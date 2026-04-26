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

#include <cmath>
#include <ranges>

#include <EASTL/memory.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ozz/base/span.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>

#include <Math/Math.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Logging/Assert.hh>

#include <Memory/Allocator.hh>

#include <Animation/Animator.hh>

namespace mikoto::animation {

    using namespace mikoto::core;
    using namespace mikoto::asset;

    Animator::Animator( ModelHandle handle )
        : mModel{ handle }
    {
        MKT_ASSERT( !handle.IsEmpty(), "Invalid model handle for animator" );
        mContext = ozz::make_unique<ozz::animation::SamplingJob::Context>();

        mFinalMatrices.resize( kMaxBonesPerMesh );
    }

    auto Animator::Update( float deltaTime ) -> void {
        if ( m_IsPlaying && mCurrentAnimation ) {
            UpdateOzzAnimation( deltaTime );
        }
	}

    auto Animator::GetFinalBoneMatrices() const -> const eastl::vector<float4x4>& {
        return mFinalMatrices;
    }

    auto Animator::GetInverseBindMatrices() const -> const eastl::vector<float4x4>& {
        return mModel->GetSkeleton()->GetInverseBindMatrices();
    }

    auto Animator::SetAnimatorState( AnimatorState state ) -> void {
        mState = state;
    }

    auto Animator::GetAnimatorState() const -> AnimatorState {
        return mState;
    }

    auto Animator::IsAnimatorState( AnimatorState state ) const -> bool {
        return  mState == state;
    }

    auto Animator::SetCurrentAnimation( eastl::string_view name ) -> void {
        SkinnedAnimation* animation{ mModel->FindAnimation( name ) };
        if ( !animation ) {
            MKT_CORE_LOGGER_ERROR( "Animation {} does not exist. Cannot set current animation.", name );
            return;
        }

        mCurrentAnimation = animation;

        StopCurrentAnimation();
    }

    auto Animator::GetCurrentAnimation() const -> const SkinnedAnimation* {
        return mCurrentAnimation;
    }

    auto Animator::GetAnimationList() const -> const AnimationList& {
        return mModel->GetAnimations();
    }

    auto Animator::PlayCurrentAnimation() -> void {
        if ( mCurrentAnimation ) {
            InitializeOzzAnimation();

            mCurrentTime = 0.0f;
            m_IsPlaying = true;
         } else {
             MKT_CORE_LOGGER_ERROR( "No animation is currently set. Cannot play." );
         }
    }

    auto Animator::PlayAnimation( eastl::string_view name ) -> void {
        if ( SkinnedAnimation* animation{ mModel->FindAnimation( name ) } ) {
            mCurrentAnimation = animation;
            PlayCurrentAnimation();
        } else {
            MKT_CORE_LOGGER_ERROR( "Animation {} does not exist. Cannot play it.", name );
        }
    }

    auto Animator::StopCurrentAnimation() -> void {
        mCurrentTime = 0.0f;
        m_IsPlaying = false;
    }

    auto Animator::IsPlaying() const -> bool {
        return m_IsPlaying;
    }

    auto Animator::UpdateOzzAnimation( float ts ) -> void {
        // Get ozz skeleton and animation
        auto* skeleton{ mModel->GetSkeleton()->GetOzzSkeleton() };
        auto playAnimation{ mCurrentAnimation->GetOzzAnimation() };

        // Update current animation time.
        mCurrentTime += ts;
        float duration{ playAnimation->duration() };
        float ratio{ std::fmod( mCurrentTime, duration ) / duration };// std::fmod because we wanna loop

        // Samples optimized animation at t = animation_time_.
        ozz::animation::SamplingJob samplingJob{};
        samplingJob.animation = playAnimation;
        samplingJob.context = mContext.get();
        samplingJob.ratio = ratio;
        samplingJob.output = make_span( mLocalMatrices );
        if ( !samplingJob.Run() ) {
            MKT_CORE_LOGGER_ERROR( "Animator error on sampling job" );
            return;
        }

        // Converts from local space to model space matrices.
        ozz::animation::LocalToModelJob ltmJob{};
        ltmJob.skeleton = skeleton;
        ltmJob.input = make_span( mLocalMatrices );
        ltmJob.output = make_span( mModelMatrices );
        if ( !ltmJob.Run() ) {
            MKT_CORE_LOGGER_ERROR( "Animator error on local to model job" );
            return;
        }

        const auto& inverseBindMats{ mModel->GetSkeleton()->GetInverseBindMatrices() };

        size_t limit{ math::Min( mFinalMatrices.size(), inverseBindMats.size(), mModelMatrices.size() ) }; // ???
        for ( size_t i{}; i < limit; ++i ) {
            // because ozz uses colum major mat4x4 of floats
            ozz::math::Float4x4& model{ mModelMatrices[i] };
            mFinalMatrices[i] = *reinterpret_cast<float4x4*>( MKT_ADDRESSOF( model ) ) * inverseBindMats[i];
        }

        //Math::DumpMat4FListBeautify( m_FinalMatrices );
    }

    auto Animator::InitializeOzzAnimation() -> void {
        auto skeleton{ mModel->GetSkeleton()->GetOzzSkeleton() };
        auto playAnimation{ mCurrentAnimation->GetOzzAnimation() };

        if ( skeleton->num_joints() != playAnimation->num_tracks() ) {
            MKT_CORE_LOGGER_ERROR( "Animator error skeleton joint count does not match animation track count" );
            return;
        }

        const i32 soaJointsCount{ skeleton->num_soa_joints() };
        mLocalMatrices.resize( soaJointsCount );

        const i32 jointsCount{ skeleton->num_joints() };
        mModelMatrices.resize( jointsCount );

        mContext->Resize( jointsCount );
    }
}
