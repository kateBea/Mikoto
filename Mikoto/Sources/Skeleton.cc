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

#include <Common/String.hh>
#include <Animation/Skeleton.hh>

namespace Mikoto {

    Skeleton::Skeleton( NodeHierarchy &&hierarchy )
        : m_Hierarchy{ std::move( hierarchy ) }
    {}

    auto Skeleton::GetHierarchy() -> NodeHierarchy & {
        return m_Hierarchy;
    }

    auto Skeleton::SetHierarchy( NodeHierarchy &&hierarchy ) -> void {
        m_Hierarchy = std::move( hierarchy );
    }

    auto Skeleton::SetBoneMapInfo( BoneInfoMap &&info ) -> void {
        m_BoneInfoMap = std::move(info);
    }

    auto Skeleton::GetBoneInfoMap() const -> const BoneInfoMap & {
        return m_BoneInfoMap;
    }

    auto Skeleton::GetBoneInfoMap() -> BoneInfoMap & {
        return m_BoneInfoMap;
    }

    auto Skeleton::GetBoneCount() const -> UInt32 {
        return m_Joints.size();
    }

    auto Skeleton::SetBoneMap( BoneMap &&boneMap ) -> void {
        m_Joints = std::move( boneMap );
    }

    auto Skeleton::GetBoneMap() -> BoneMap & {
        return m_Joints;
    }

    auto Skeleton::GetBoneMap() const -> const BoneMap & {
        return m_Joints;
    }

    auto Skeleton::FindBone( std::string_view name ) -> Joint* {
        const std::string str{ StringUtil::From( name ) };
        const auto iter{ m_Joints.find( str ) };
        if ( iter != m_Joints.end() ) {
            return std::addressof( iter->second );
        }

        return nullptr;
    }
}