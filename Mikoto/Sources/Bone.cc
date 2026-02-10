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

#include <Logging/Assert.hh>

#include <Animation/Bone.hh>

namespace Mikoto {

    Bone::Bone( const std::string &name, Int32 ID, const aiNodeAnim *channel )
        : m_Name{ name },
          m_ID{ ID },
          m_LocalTransform{ 1.0f } {
        m_NumPositions = channel->mNumPositionKeys;

        for ( Int32 positionIndex{}; positionIndex < m_NumPositions; ++positionIndex ) {
            aiVector3D aiPosition{ channel->mPositionKeys[positionIndex].mValue };
            double timeStamp{ channel->mPositionKeys[positionIndex].mTime };
            KeyPosition data{
                .Position{ aiPosition.x, aiPosition.y, aiPosition.z },
                .TimeStamp{ static_cast<float>( timeStamp ) }
            };
            m_Positions.push_back( data );
        }

        m_NumRotations = channel->mNumRotationKeys;
        for ( Int32 rotationIndex = 0; rotationIndex < m_NumRotations; ++rotationIndex ) {
            aiQuaternion aiOrientation{ channel->mRotationKeys[rotationIndex].mValue };
            double timeStamp{ channel->mRotationKeys[rotationIndex].mTime };
            KeyRotation data{
                .Orientation{ glm::quat( aiOrientation.w, aiOrientation.x, aiOrientation.y, aiOrientation.z ) },
                .TimeStamp{ static_cast<float>( timeStamp ) }
            };
            m_Rotations.push_back( data );
        }

        m_NumScalings = channel->mNumScalingKeys;
        for ( Int32 keyIndex{}; keyIndex < m_NumScalings; ++keyIndex ) {
            aiVector3D scale{ channel->mScalingKeys[keyIndex].mValue };
            double timeStamp{ channel->mScalingKeys[keyIndex].mTime };
            KeyScale data{
                .Scale{ scale.x, scale.y, scale.z },
                .TimeStamp{ static_cast<float>( timeStamp ) }
            };
            m_Scales.push_back( data );
        }
    }

    auto Bone::Update( float animationTime ) -> void {
        glm::mat4 translation{ InterpolatePosition(animationTime) };
        glm::mat4 rotation{ InterpolateRotation(animationTime) };
        glm::mat4 scale{ InterpolateScaling(animationTime) };

        m_LocalTransform = translation * rotation * scale;
    }

    auto Bone::GetPositionIndex( float animationTime ) -> Int32 {
        for ( Int32 index{}; index < m_NumPositions - 1; ++index ) {
            if ( animationTime < m_Positions[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid position index" );
    }

    auto Bone::GetRotationIndex( float animationTime ) -> Int32 {
        for ( Int32 index{}; index < m_NumRotations - 1; ++index ) {
            if ( animationTime < m_Rotations[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid rotation index" );
    }

    auto Bone::GetScaleIndex( float animationTime ) -> Int32 {
        for ( Int32 index{}; index < m_NumScalings - 1; ++index ) {
            if ( animationTime < m_Scales[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid scale index" );
    }

    auto Bone::GetScaleFactor( float lastTimeStamp, float nextTimeStamp, float animationTime ) -> float {
        float scaleFactor{ 0.0f };
        const float midWayLength{ animationTime - lastTimeStamp };
        const float framesDiff{ nextTimeStamp - lastTimeStamp };

        scaleFactor = midWayLength / framesDiff;

        return scaleFactor;
    }

    auto Bone::InterpolatePosition( float animationTime ) -> Mat4F {
        if ( 1 == m_NumPositions )
            return glm::translate( glm::mat4( 1.0f ), m_Positions[0].Position );

        int p0Index = GetPositionIndex( animationTime );
        int p1Index = p0Index + 1;
        float scaleFactor = GetScaleFactor( m_Positions[p0Index].TimeStamp,
                                            m_Positions[p1Index].TimeStamp, animationTime );
        glm::vec3 finalPosition = glm::mix( m_Positions[p0Index].Position, m_Positions[p1Index].Position, scaleFactor );
        return glm::translate( glm::mat4( 1.0f ), finalPosition );
    }

    auto Bone::InterpolateRotation( float animationTime ) -> Mat4F {
        if ( 1 == m_NumRotations ) {
            auto rotation = glm::normalize( m_Rotations[0].Orientation );
            return glm::toMat4( rotation );
        }

        int p0Index = GetRotationIndex( animationTime );
        int p1Index = p0Index + 1;
        float scaleFactor = GetScaleFactor( m_Rotations[p0Index].TimeStamp,
                                            m_Rotations[p1Index].TimeStamp, animationTime );
        glm::quat finalRotation = glm::slerp( m_Rotations[p0Index].Orientation, m_Rotations[p1Index].Orientation, scaleFactor );
        finalRotation = glm::normalize( finalRotation );
        return glm::toMat4( finalRotation );
    }

    auto Bone::InterpolateScaling( float animationTime ) -> Mat4F {
        if ( 1 == m_NumScalings )
            return glm::scale( glm::mat4( 1.0f ), m_Scales[0].Scale );

        int p0Index = GetScaleIndex( animationTime );
        int p1Index = p0Index + 1;
        float scaleFactor = GetScaleFactor( m_Scales[p0Index].TimeStamp,
                                            m_Scales[p1Index].TimeStamp, animationTime );
        glm::vec3 finalScale = glm::mix( m_Scales[p0Index].Scale, m_Scales[p1Index].Scale, scaleFactor );
        return glm::scale( glm::mat4( 1.0f ), finalScale );
    }
}