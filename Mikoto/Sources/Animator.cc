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
    Animator::Animator( SkinnedAnimation& animator )
        : m_Animation{ std::addressof( animator ) }, m_CurrentTime{ 0.0f }, m_FinalBoneMatrices( 100, glm::mat4( 1.0f ) ) {
    }

	auto Animator::UpdateAnimation( float deltaTime ) -> void {
        if ( m_Animation ) {
            m_CurrentTime += m_Animation->GetTicksPerSecond() * deltaTime;
            m_CurrentTime = fmod( m_CurrentTime, m_Animation->GetDuration() );
            CalculateBoneTransform( &m_Animation->GetRootNode(), glm::mat4( 1.0f ) );
        }
	}

	auto Animator::CalculateBoneTransform(const NodeHierarchy* node, Mat4F parentTransform) -> void {
        std::string nodeName{ node->Name };
        glm::mat4 nodeTransform{ node->Transformation };

        Bone* Bone{ m_Animation->FindBone( nodeName ) };

        if ( Bone ) {
            Bone->Update( m_CurrentTime );
            nodeTransform = Bone->GetLocalTransform();
        }

        glm::mat4 globalTransformation{ parentTransform * nodeTransform };

        auto boneInfoMap{ m_Animation->GetBoneIDMap() };
        if ( boneInfoMap.find( nodeName ) != boneInfoMap.end() ) {
            Int32 index{ boneInfoMap[nodeName].ID };

            glm::mat4 offset{ boneInfoMap[nodeName].Offset };
            m_FinalBoneMatrices[index] = globalTransformation * offset;
        }

        for ( Int32 i{}; i < node->ChildrenCount; i++ ) {
            CalculateBoneTransform( &node->Children[i], globalTransformation );
        }
	}
}
