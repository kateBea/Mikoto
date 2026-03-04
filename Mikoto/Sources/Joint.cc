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
        : m_Name{ name },
          m_ID{ ID },
          m_ModelToBoneTransform{ ModelToBoneTransform }
    {
        MKT_ASSERT( m_ID != INVALID_JOINT_ID, "No valid ID found" );
    }

    auto Joint::SetParentID( Int32 ID ) -> void {
        m_ParentID = ID;
    }

    auto Joint::SetParentRelativeTransform( const Mat4F& mat ) -> void {
        m_ParentRelativeTransform = mat;
    }

    auto Joint::GetParentRelativeTransform() const -> const Mat4F& {
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

    auto Joint::SetAnimationProperties( AnimationProperties&& properties ) -> void {
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
}