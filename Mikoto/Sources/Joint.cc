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

#include <Common/Common.hh>
#include <Common/String.hh>
#include <Library/String/String.hh>

#include <Animation/Joint.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

    Joint::Joint( const std::string& name, Int32 ID, Mat4F ModelToBoneTransform )
        : m_ID{ ID },
          m_Name{ name },
          m_LocalTransform{ 1.0f },
            m_ModelToBoneTransform{ ModelToBoneTransform }
    {
        MKT_ASSERT( m_ID != INVALID_JOINT_ID, "No valid ID found" );
    }

    auto Joint::Update( float animationTime ) -> void {
        const Mat4F translation{ InterpolatePosition(animationTime) };
        const Mat4F rotation{ InterpolateRotation(animationTime) };
        const Mat4F scale{ InterpolateScaling(animationTime) };

        m_LocalTransform = translation * rotation * scale;
    }

    auto Joint::GetPositionIndex( float animationTime ) const -> Int32 {
        for ( Int32 index{}; index < m_Positions.size() - 1; ++index ) {
            if ( animationTime < m_Positions[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid position index" );
    }

    auto Joint::GetRotationIndex( float animationTime ) const -> Int32 {
        for ( Int32 index{}; index < m_Rotations.size() - 1; ++index ) {
            if ( animationTime < m_Rotations[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid rotation index" );
    }

    auto Joint::GetScaleIndex( float animationTime ) const -> Int32 {
        for ( Int32 index{}; index < m_Scales.size() - 1; ++index ) {
            if ( animationTime < m_Scales[index + 1].TimeStamp ) {
                return index;
            }
        }

        MKT_ASSERT( false, "No valid scale index" );
    }

    auto Joint::SetParentID( Int32 ID ) -> void {
        m_ParentID = ID;
    }

    auto Joint::SetParentRelativeTransform( const Mat4F& mat ) -> void {
        m_ParentRelativeTransform = mat;
    }

    auto Joint::SetParentRelativeTransform() const -> const Mat4F& {
        return m_ParentRelativeTransform;
    }

    auto Joint::GetModelToBoneTransform() const -> const Mat4F& {
        return m_ModelToBoneTransform;
    }

    auto Joint::GetID() const -> Int32 {
        return m_ID;
    }

    auto Joint::GetParentID() const -> Int32 {
        return m_ParentID;
    }

    auto Joint::GetBoneName() const -> const std::string & {
        return m_Name;
    }

    auto Joint::GetLocalTransform() const -> const Mat4F & {
        return m_LocalTransform;
    }

    auto Joint::SetAnimationProperties( AnimationeProperties&& properties ) -> void {
        m_Positions = std::move( properties.Positions );
        m_Rotations = std::move( properties.Rotations );
        m_Scales = std::move( properties.Scales );
    }

    auto Joint::DebugPrintBoneContribution() const -> void {
        MKT_COLOR_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_BLUE_VIOLET,
                "Printing joint vertex contribution\n" );

        for ( const auto& [meshName, contributionMap]: m_VertexWeights ) {
            // Because console output is limited, also log to file
            MKT_FILE_LOGGER_DEBUG( "Joint [{}] contribution to mesh [{}]", m_Name, meshName );
            MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_YELLOW, "Joint [{}] contribution to mesh [{}]\n", m_Name, meshName );

            for (const auto& [vertex, weight] : contributionMap) {
                MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_YELLOW, "\tVertex [{}] Weight [{}]\n", vertex, weight );
                MKT_FILE_LOGGER_DEBUG( "\tVertex [{}] Weight [{}]", vertex, weight );
            }
        }
    }

    auto Joint::SetVertexWeights( std::string_view meshName, UInt64 vertex, float weight ) -> void {
        m_VertexWeights[StringUtil::From( meshName )][vertex] = weight;
    }

    auto Joint::GetScaleFactor( float lastTimeStamp, float nextTimeStamp, float animationTime ) -> float {
        float scaleFactor{ 0.0f };
        const float midWayLength{ animationTime - lastTimeStamp };
        const float framesDiff{ nextTimeStamp - lastTimeStamp };

        scaleFactor = midWayLength / framesDiff;

        return scaleFactor;
    }

    auto Joint::InterpolatePosition( float animationTime ) -> Mat4F {
        if ( m_Positions.size() == 1 ) {
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
        if ( m_Rotations.size() == 1 ) {
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
        if ( m_Scales.size() == 1 )
            return glm::scale( glm::mat4( 1.0f ), m_Scales[0].Scale );

        const Int32 p0Index{ GetScaleIndex( animationTime ) };
        const Int32 p1Index{ p0Index + 1 };
        const float scaleFactor{ GetScaleFactor( m_Scales[p0Index].TimeStamp,
                                            m_Scales[p1Index].TimeStamp, animationTime ) };
        const Vec3F finalScale{ glm::mix( m_Scales[p0Index].Scale, m_Scales[p1Index].Scale, scaleFactor ) };
        return glm::scale( glm::mat4( 1.0f ), finalScale );
    }
}