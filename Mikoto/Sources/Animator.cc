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

#include <Animation/Animator.hh>

namespace Mikoto {

    Animator::Animator( ModelHandle handle )
        : m_CurrentTime{ 0.0f }, m_Model{ handle }, m_FinalBoneMatrices( MAX_BONES_PER_MESH, glm::mat4( 1.0f ) )
    {}

    auto Animator::UpdateAnimation( float deltaTime ) -> void {
        if ( m_CurrentAnimation ) {
            m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * deltaTime;
            m_CurrentTime = fmod( m_CurrentTime, m_CurrentAnimation->GetDuration() );
            CalculateBoneTransform( &m_CurrentAnimation->GetSkeleton().GetHierarchy(), glm::mat4( 1.0f ) );
        }

        // DEBUG if it has any set to first one
        if ( !m_CurrentAnimation ) {
            for (auto& animation : m_Model->GetAnimations()) {
                m_CurrentAnimation = std::addressof( animation.second );
                break;
            }
        }
	}

    auto Animator::SetCurrentAnimation( std::string_view name ) -> void {
        m_CurrentAnimationName = name;

        auto result{ m_Model->FindAnimation( name ) };

        if (!result) {
            const std::string resultMessage{ m_CurrentAnimation ? m_CurrentAnimation->GetName() : "NULL" };
            MKT_CORE_LOGGER_ERROR( "Animation does not exist {}. Current animation is {}", name, resultMessage );
        } else {
            m_CurrentAnimation = result;
        }
    }

    auto Animator::GetCurrentAnimation() const -> const SkinnedAnimation* {
        return m_CurrentAnimation;
    }

    auto Animator::CalculateBoneTransform(const NodeHierarchy* node, Mat4F parentTransform) -> void {
        std::string nodeName{ node->Name };
        glm::mat4 nodeTransform{ node->Transformation };

        Joint* Bone{ m_CurrentAnimation->FindBone( nodeName ) };

        if ( Bone ) {
            Bone->Update( m_CurrentTime );
            nodeTransform = Bone->GetLocalTransform();
        }

        glm::mat4 globalTransformation{ parentTransform * nodeTransform };

        const auto& boneInfoMap{ m_CurrentAnimation->GetBoneInfoMap() };
        if ( boneInfoMap.find( nodeName ) != boneInfoMap.end() ) {
            Int32 index{ boneInfoMap.at(nodeName).ID };

            glm::mat4 offset{ boneInfoMap.at(nodeName).Offset };
            m_FinalBoneMatrices[index] = globalTransformation * offset;
        }

        for ( UInt32 i{}; i < node->ChildrenCount; i++ ) {
            CalculateBoneTransform( &node->Children[i], globalTransformation );
        }
	}
}
