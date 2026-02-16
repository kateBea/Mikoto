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

#include <Animation/Joint.hh>
#include <Logging/Assert.hh>

namespace Mikoto {

    Joint::Joint( const std::string &name, Int32 ID, const aiNodeAnim *channel )
        : m_LocalTransform{ 1.0f },
          m_Name{ name },
          m_ID{ ID } {
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

    auto Joint::Update( float animationTime ) -> void {
        const Mat4F translation{ InterpolatePosition(animationTime) };
        const Mat4F rotation{ InterpolateRotation(animationTime) };
        const Mat4F scale{ InterpolateScaling(animationTime) };

        m_LocalTransform = translation * rotation * scale;
    }

    auto Joint::GetPositionIndex( float animationTime ) const -> Int32 {
        for ( Int32 index{}; index < m_NumPositions - 1; ++index ) {
            if ( animationTime < m_Positions[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid position index" );
    }

    auto Joint::GetRotationIndex( float animationTime ) const -> Int32 {
        for ( Int32 index{}; index < m_NumRotations - 1; ++index ) {
            if ( animationTime < m_Rotations[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid rotation index" );
    }

    auto Joint::GetScaleIndex( float animationTime ) const -> Int32 {
        for ( Int32 index{}; index < m_NumScalings - 1; ++index ) {
            if ( animationTime < m_Scales[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid scale index" );
    }

    auto Joint::GetScaleFactor( float lastTimeStamp, float nextTimeStamp, float animationTime ) -> float {
        float scaleFactor{ 0.0f };
        const float midWayLength{ animationTime - lastTimeStamp };
        const float framesDiff{ nextTimeStamp - lastTimeStamp };

        scaleFactor = midWayLength / framesDiff;

        return scaleFactor;
    }

    auto Joint::InterpolatePosition( float animationTime ) -> Mat4F {
        if ( m_NumPositions == 1 ) {
            return glm::translate( glm::mat4( 1.0f ), m_Positions[0].Position );
        }

        const Int32 p0Index{ GetPositionIndex( animationTime ) };
        const Int32 p1Index{ p0Index + 1 };
        const float scaleFactor{ GetScaleFactor( m_Positions[p0Index].TimeStamp,
                                            m_Positions[p1Index].TimeStamp, animationTime ) };
        const Vec3F finalPosition{ glm::mix( m_Positions[p0Index].Position, m_Positions[p1Index].Position, scaleFactor ) };
        return glm::translate( glm::mat4( 1.0f ), finalPosition );
    }

    auto Joint::InterpolateRotation( float animationTime ) -> Mat4F {
        if ( m_NumRotations == 1 ) {
            auto rotation = glm::normalize( m_Rotations[0].Orientation );
            return glm::toMat4( rotation );
        }

        const Int32 p0Index = GetRotationIndex( animationTime );
        const Int32 p1Index = p0Index + 1;
        const float scaleFactor = GetScaleFactor( m_Rotations[p0Index].TimeStamp,
                                            m_Rotations[p1Index].TimeStamp, animationTime );
        Quat finalRotation{ glm::slerp( m_Rotations[p0Index].Orientation, m_Rotations[p1Index].Orientation, scaleFactor ) };
        finalRotation = glm::normalize( finalRotation );
        return glm::toMat4( finalRotation );
    }

    auto Joint::InterpolateScaling( float animationTime ) -> Mat4F {
        if ( m_NumScalings == 1 )
            return glm::scale( glm::mat4( 1.0f ), m_Scales[0].Scale );

        const Int32 p0Index{ GetScaleIndex( animationTime ) };
        const Int32 p1Index{ p0Index + 1 };
        const float scaleFactor{ GetScaleFactor( m_Scales[p0Index].TimeStamp,
                                            m_Scales[p1Index].TimeStamp, animationTime ) };
        const Vec3F finalScale{ glm::mix( m_Scales[p0Index].Scale, m_Scales[p1Index].Scale, scaleFactor ) };
        return glm::scale( glm::mat4( 1.0f ), finalScale );
    }
}