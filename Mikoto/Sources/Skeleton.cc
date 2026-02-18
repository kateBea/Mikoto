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

#include <utility>
#include <memory>
#include <ranges>

#include <Common/String.hh>
#include <Animation/Skeleton.hh>

namespace Mikoto {

    auto Skeleton::RegisterJoint( const std::string &name, Int32 ID, Mat4F ModelToBoneTransform ) -> void {
        m_Joints.try_emplace( name, name, ID, std::move( ModelToBoneTransform ) );
        m_JointsByID.try_emplace( ID, name );
    }

    auto Skeleton::GetBoneCount() const -> UInt32 {
        return m_Joints.size();
    }

    auto Skeleton::SetBoneMap( JointsMap &&boneMap ) -> void {
        m_Joints = std::move( boneMap );
    }

    auto Skeleton::GetBoneMap() -> JointsMap & {
        return m_Joints;
    }

    auto Skeleton::GetBoneMap() const -> const JointsMap & {
        return m_Joints;
    }

    auto Skeleton::HasJoint( std::string_view name ) const -> bool {
        return m_Joints.contains( StringUtil::From( name ) );
    }

    auto Skeleton::FindJoint( std::string_view name ) -> Joint* {
        const std::string str{ StringUtil::From( name ) };
        const auto iter{ m_Joints.find( str ) };
        if ( iter != m_Joints.end() ) {
            return std::addressof( iter->second );
        }

        return nullptr;
    }

    auto Skeleton::FindJointByID( UInt32 ID ) -> Joint * {
        const auto iter{ m_JointsByID.find( ID ) };
        if ( iter != m_JointsByID.end() ) {
            return std::addressof( m_Joints.at( iter->second ) );
        }

        return nullptr;
    }

    auto Skeleton::begin() -> JointsMapIterator {
        return m_Joints.begin();
    }

    auto Skeleton::end() -> JointsMapIterator {
        return m_Joints.end();
    }

    auto Skeleton::begin() const -> JointsMapConstIterator {
        return m_Joints.begin();
    }

    auto Skeleton::end() const -> JointsMapConstIterator {
        return m_Joints.end();
    }

    auto Skeleton::cbegin() const -> JointsMapConstIterator {
        return m_Joints.cbegin();
    }

    auto Skeleton::cend() const -> JointsMapConstIterator {
        return m_Joints.cend();
    }

    auto Skeleton::DebugPrintBoneContribution() const -> void {
        for (const auto& joint : m_Joints | std::ranges::views::values) {
            joint.DebugPrintBoneContribution();
        }
    }

    auto Skeleton::SetVertexWeights( std::string_view meshName, std::string_view boneName, UInt64 vertex, float weight ) -> void {
        if (auto joint{ FindJoint( boneName ) }) {
            joint->SetVertexWeights( meshName, vertex, weight );
        }
    }
}