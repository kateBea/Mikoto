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

#include <memory>
#include <ranges>
#include <cmath>

#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/log.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/span.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/vec_float.h>
#include <ozz/options/options.h>

#include <Common/String.hh>

#include <Math/Math.hh>

#include <Logging/Assert.hh>
#include <Animation/Animator.hh>

namespace Mikoto {

    Animator::Animator( ModelHandle handle )
        : m_CurrentTime{ 0.0f }, m_Model{ handle }
    {
        MKT_ASSERT( !handle.IsEmpty(), "Invalid model handle for animator" );
        m_Context = ozz::make_unique<ozz::animation::SamplingJob::Context>();

        m_FinalMatrices.resize( MAX_BONES_PER_MESH );
    }

    auto Animator::UpdateAnimation( float deltaTime ) -> void {
        if ( m_IsPlaying && m_CurrentAnimation ) {
            UpdateOzzAnimation( deltaTime );
        }
	}

    auto Animator::SetCurrentAnimation( std::string_view name ) -> void {
        SkinnedAnimation* animation{ m_Model->FindAnimation( name ) };
        if ( !animation ) {
            MKT_CORE_LOGGER_ERROR( "Animation {} does not exist. Cannot set current animation.", name );
            return;
        }

        m_CurrentAnimation = animation;

        StopCurrentAnimation();
    }

    auto Animator::GetCurrentAnimation() const -> const SkinnedAnimation* {
        return m_CurrentAnimation;
    }

    auto Animator::PlayCurrentAnimation() -> void {
        if ( m_CurrentAnimation ) {
            InitializeOzzAnimation();

            m_CurrentTime = 0.0f;
            m_IsPlaying = true;
         } else {
             MKT_CORE_LOGGER_ERROR( "No animation is currently set. Cannot play." );
         }
    }
    auto Animator::PlayAnimation( std::string_view name ) -> void {
        SkinnedAnimation* animation{ m_Model->FindAnimation( name ) };
        if (animation) {
            m_CurrentAnimation = animation;

            PlayCurrentAnimation();
        } else {
            MKT_CORE_LOGGER_ERROR( "Animation {} does not exist. Cannot play.", name );
        }
    }

    auto Animator::StopCurrentAnimation() -> void {
        m_CurrentTime = 0.0f;
        m_IsPlaying = false;
    }

    auto Animator::IsPlaying() const -> bool {
        return m_IsPlaying;
    }

    auto Animator::UpdateOzzAnimation( float ts ) -> void {
        // Get ozz skeleton and animation
        auto* skeleton{ m_Model->GetSkeleton().GetOzzSkeleton() };
        auto playAnimation{ m_CurrentAnimation->GetOzzAnimation() };
        
        // Update current animation time.
        m_CurrentTime += ts;
        float duration{ playAnimation->duration() };
        float ratio{ std::fmod( m_CurrentTime, duration ) / duration };// std::fmod because we wanna loop

        // Samples optimized animation at t = animation_time_.
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = playAnimation;
        sampling_job.context = m_Context.get();
        sampling_job.ratio = ratio;
        sampling_job.output = make_span( m_LocalMatrices );
        if ( !sampling_job.Run() ) {
            MKT_CORE_LOGGER_ERROR( "Animator error on sampling job" );
            return;
        }

        // Converts from local space to model space matrices.
        ozz::animation::LocalToModelJob ltm_job;
        ltm_job.skeleton = skeleton;
        ltm_job.input = make_span( m_LocalMatrices );
        ltm_job.output = make_span( m_ModelMatrices );
        if ( !ltm_job.Run() ) {
            MKT_CORE_LOGGER_ERROR( "Animator error on local to model job" );
            return;
        }
        
        const Size jointCount{ m_ModelMatrices.size() };
        const auto& inverseBindMats{ m_Model->GetSkeleton().GetInverseBindMatrices() };

        for ( Size i{}; i < jointCount && i < inverseBindMats.size(); ++i ) {
            // because ozz uses colum major mat4x4 of floats
            ozz::math::Float4x4& model{ m_ModelMatrices[i] };
            m_FinalMatrices[i] = *reinterpret_cast<Mat4F*>( MKT_ADDRESSOF( model ) ) * inverseBindMats[i];
        }

        //Math::DumpMat4FListBeautify( m_FinalMatrices );
    }

    auto Animator::InitializeOzzAnimation() -> void {
        // Get ozz skeleton and animation
        auto skeleton{ m_Model->GetSkeleton().GetOzzSkeleton() };
        auto playAnimation{ m_CurrentAnimation->GetOzzAnimation() };

        // Skeleton and animation needs to match.
        if ( skeleton->num_joints() != playAnimation->num_tracks() ) {
            MKT_CORE_LOGGER_ERROR( "Animator error skeleton joint count does not match animation track count" );
            return;
        }

        // Allocates runtime buffers.
        const Int32 soaJointsCount{ skeleton->num_soa_joints() };
        m_LocalMatrices.resize( soaJointsCount );
        
        const Int32 jointsCount{ skeleton->num_joints() };
        m_ModelMatrices.resize( jointsCount );

        // Allocates a context that matches animation requirements.
        m_Context->Resize( jointsCount );
    }
}
