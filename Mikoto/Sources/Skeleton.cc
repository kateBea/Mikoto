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

#include <Logging/Logger.hh>

#include <Common/String.hh>
#include <Animation/Skeleton.hh>
#include <Library/String/String.hh>

#include <ozz/animation/runtime/skeleton.h>

namespace Mikoto {

    Skeleton::Skeleton( ozz::unique_ptr<ozz::animation::Skeleton> &&data )
        : m_Skeleton{ std::move( data ) } {}

    auto Skeleton::HasJoint( std::string_view name ) const -> bool {
        return false;
    }

    auto Skeleton::FindJoint( std::string_view name ) -> Joint * {
        return nullptr;
    }

    auto Skeleton::FindJoint( std::string_view name ) const -> const Joint* {
        return nullptr;
    }

    auto Skeleton::FindJointByID( UInt32 ID ) -> Joint * {
        return nullptr;
    }

    auto Skeleton::GetOzzSkeleton() -> ozz::animation::Skeleton* {
        return m_Skeleton.get();
    }

    auto Skeleton::GetOzzBondeIndex( UInt32 ID ) const -> Int32 {
        if (!m_JointOzzIndex.contains(ID)) {
            return -1;
        }

        return m_JointOzzIndex.at( ID );
    }

    auto Skeleton::SetInverseBindMatrices( std::vector<Mat4F>&& mats ) -> void {
        m_InverseBindMats = std::move( mats );
    }

    auto Skeleton::GetOzzSkeleton() const -> const ozz::animation::Skeleton* {
        return m_Skeleton.get();
    }

    auto Skeleton::FindJointByID( UInt32 ID ) const -> const Joint * {
        return nullptr;
    }

    auto Skeleton::PrintBoneInfo() const -> void {
    }

    auto Skeleton::SetWeights( std::string_view meshName, std::string_view boneName, UInt64 vertex, float weight ) -> void {

    }

    auto Skeleton::PrintTreeView() -> void {
        
    }
}