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

#include <EASTL/utility.h>
#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ozz/animation/runtime/skeleton.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>
#include <Logging/Assert.hh>

#include <Animation/Skeleton.hh>

namespace mikoto::animation {

    using namespace mikoto::core;


    Skeleton::Skeleton( ozz::unique_ptr<ozz::animation::Skeleton>&& data )
        : mSkeleton{ eastl::move( data ) } {
        MKT_BEGIN_PROFILER();

        if ( !mSkeleton ) {
            return;
        }

        const auto names{ mSkeleton->joint_names() };
        const auto parents{ mSkeleton->joint_parents() };

        mJoints.reserve( names.size() );
        mJointIndices.reserve( names.size() );

        for ( core::usize index{}; index < names.size(); ++index ) {
            const auto id{ static_cast<core::i32>( index ) };
            mJoints.emplace_back( eastl::string{ names[index] }, id ).SetParentID( parents[index] );
            mJointIndices.try_emplace( eastl::string_view{ names[index] }, id );
        }
    }

    auto Skeleton::HasJoint( eastl::string_view name ) const -> bool {
        MKT_BEGIN_PROFILER();
        return FindJoint( name ) != nullptr;
    }

    auto Skeleton::FindJoint( eastl::string_view name ) -> Joint* {
        MKT_BEGIN_PROFILER();
        const auto found{ mJointIndices.find( name ) };
        return found != mJointIndices.end() ? &mJoints[found->second] : nullptr;
    }

    auto Skeleton::FindJoint( eastl::string_view name ) const -> const Joint* {
        MKT_BEGIN_PROFILER();
        const auto found{ mJointIndices.find( name ) };
        return found != mJointIndices.end() ? &mJoints[found->second] : nullptr;
    }

    auto Skeleton::FindJointByID( core::u32 ID ) -> Joint* {
        MKT_BEGIN_PROFILER();
        return ID < mJoints.size() ? &mJoints[ID] : nullptr;
    }

    auto Skeleton::GetOzzSkeleton() -> ozz::animation::Skeleton* {
        MKT_BEGIN_PROFILER();
        return mSkeleton.get();
    }

    auto Skeleton::GetOzzBoneIndex( core::u32 ID ) const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return ID < mJoints.size() ? static_cast<core::i32>( ID ) : -1;
    }

    auto Skeleton::GetInverseBindMatrices() const -> const eastl::vector<core::float4x4>& {
        MKT_BEGIN_PROFILER();
        return mInverseBindMats;
    }

    auto Skeleton::GetOzzSkeleton() const -> const ozz::animation::Skeleton* {
        MKT_BEGIN_PROFILER();
        return mSkeleton.get();
    }

    auto Skeleton::FindJointByID( core::u32 ID ) const -> const Joint* {
        MKT_BEGIN_PROFILER();
        return ID < mJoints.size() ? &mJoints[ID] : nullptr;
    }

    auto Skeleton::IsArmaturePresent() const -> bool {
        MKT_BEGIN_PROFILER();
        return mSkeleton != nullptr;
    }

}// namespace mikoto::animation