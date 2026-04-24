//    Copyright 2026 ケイト
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

#include <ranges>

#include <EASTL/vector.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>

#include <ozz/animation/runtime/skeleton.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Logging/Logger.hh>

#include <Animation/Skeleton.hh>

namespace mikoto::animation {

    Skeleton::Skeleton( ozz::unique_ptr<ozz::animation::Skeleton> &&data )
        : mSkeleton{ eastl::move( data ) } {}

    auto Skeleton::HasJoint( eastl::string_view name ) const -> bool {
        return false;
    }

    auto Skeleton::FindJoint( eastl::string_view name ) -> Joint * {
        return nullptr;
    }

    auto Skeleton::FindJoint( eastl::string_view name ) const -> const Joint* {
        return nullptr;
    }

    auto Skeleton::FindJointByID( u32 ID ) -> Joint * {
        return nullptr;
    }

    auto Skeleton::GetOzzSkeleton() -> ozz::animation::Skeleton* {
        return mSkeleton.get();
    }

    auto Skeleton::GetOzzBoneIndex( u32 ID ) const -> i32 {
        return -1;
    }

    auto Skeleton::SetInverseBindMatrices( eastl::vector<float4x4>&& mats ) -> void {
        mInverseBindMats = eastl::move( mats );
    }

    auto Skeleton::GetInverseBindMatrices() const -> const eastl::vector<float4x4>& {
        return mInverseBindMats;
    }

    auto Skeleton::GetOzzSkeleton() const -> const ozz::animation::Skeleton* {
        return mSkeleton.get();
    }

    auto Skeleton::FindJointByID( u32 ID ) const -> const Joint* {
        return nullptr;
    }

    auto Skeleton::IsArmaturePresent() const -> bool {
        return mSkeleton != nullptr;
    }
}