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

#include <Common/String.hh>

#include <Logging/Assert.hh>
#include <Animation/Animator.hh>

namespace Mikoto {

    Animator::Animator( ModelHandle handle )
        : m_CurrentTime{ 0.0f }, m_Model{ handle }
    {
        MKT_ASSERT( !handle.IsEmpty(), "Invalid model handle for animator" );

        m_LocalTransform.resize( MAX_BONES_PER_MESH, Mat4F( 1.0f ) );
        m_GlobalTransform.resize( MAX_BONES_PER_MESH );
        m_FinalMatrices.resize( MAX_BONES_PER_MESH );
    }

    auto Animator::UpdateAnimation( float deltaTime ) -> void {
        if ( m_IsPlaying && m_CurrentAnimation ) {
            m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * deltaTime;
            m_CurrentTime = std::fmod( m_CurrentTime, m_CurrentAnimation->GetDuration() ); // std::fmod is used to loop the animation

            const Skeleton& skeleton{ m_Model->GetSkeleton() };
            CalculateTransform( skeleton.GetHierarchy(), Mat4F{ 1.0f }, m_CurrentTime, skeleton );
        }
	}

    auto Animator::CalculateTransform( const Node& node, glm::mat4 parentTransform, float animationTime, const Skeleton& skeleton ) -> void {
        Mat4F localTransform{ node.Transformation };

        if ( const Joint* joint{ skeleton.FindJoint( node.Name ) } ) {
            UpdateLocalTransform(*joint, animationTime, localTransform );
            const Mat4F globalTransform{ parentTransform * localTransform };

            m_FinalMatrices[joint->GetID()] = globalTransform * joint->GetModelToBoneTransform();

            for ( const auto& child: node.Children ) {
                CalculateTransform( child, globalTransform, animationTime, skeleton );
            }

            return;
        }

        const Mat4F globalTransform{ parentTransform * localTransform };

        for ( const auto& child: node.Children ) {
            CalculateTransform( child, globalTransform, animationTime, skeleton );
        }
    }

    auto Animator::UpdateLocalTransform( const Joint& joint, float animationTime, Mat4F& localTransform ) -> void {
        const Mat4F translation{ joint.InterpolatePosition( animationTime ) };
        const Mat4F rotation{ joint.InterpolateRotation( animationTime ) };
        const Mat4F scale{ joint.InterpolateScaling( animationTime ) };

        localTransform = translation * rotation * scale;
    }

    auto Animator::SetCurrentAnimation( std::string_view name ) -> void {
        SkinnedAnimation* animation{ m_Model->FindAnimation( name ) };
        if ( !animation ) {
            MKT_CORE_LOGGER_ERROR( "Animation {} does not exist. Cannot set current animation.", name );
            return;
        }
        // Reset time to start the animation from the beginning
        m_CurrentTime = 0.0f;

        m_CurrentAnimation = animation;

        m_IsPlaying = false;
    }

    auto Animator::GetCurrentAnimation() const -> const SkinnedAnimation* {
        return m_CurrentAnimation;
    }

    auto Animator::PlayCurrentAnimation() -> void {
        if ( m_CurrentAnimation ) {
             m_IsPlaying = true;
         } else {
             MKT_CORE_LOGGER_ERROR( "No animation is currently set. Cannot play." );
         }
    }
    auto Animator::PlayAnimation( std::string_view name ) -> void {
        SkinnedAnimation* animation{ m_Model->FindAnimation( name ) };
        if (animation) {
            m_CurrentAnimation = animation;

            // Reset time to start the animation from the beginning
            m_CurrentTime = 0.0f;

            m_IsPlaying = true;
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
}
