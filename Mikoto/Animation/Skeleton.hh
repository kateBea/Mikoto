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

#ifndef MIKOTO_ANIMATION_SKELETON_HH
#define MIKOTO_ANIMATION_SKELETON_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/animation/runtime/skeleton.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/Joint.hh>

namespace mikoto::animation {

    class Skeleton final : public core::INonCopyable {
    public:
        explicit Skeleton( ozz::unique_ptr<ozz::animation::Skeleton>&& data = nullptr );

        Skeleton( Skeleton&& ) = default;
        auto operator=( Skeleton&& ) noexcept -> Skeleton& = default;

        MKT_NODISCARD auto HasJoint( eastl::string_view name ) const -> bool;

        MKT_NODISCARD auto FindJoint( eastl::string_view name ) -> Joint*;
        MKT_NODISCARD auto FindJoint( eastl::string_view name ) const -> const Joint*;

        MKT_NODISCARD auto FindJointByID( core::u32 ID ) -> Joint*;
        MKT_NODISCARD auto FindJointByID( core::u32 ID ) const -> const Joint*;

        MKT_NODISCARD auto IsArmaturePresent() const -> bool;
        MKT_NODISCARD auto GetOzzBoneIndex( core::u32 ID ) const -> core::i32;
        MKT_NODISCARD auto GetOzzSkeleton() -> ozz::animation::Skeleton*;
        MKT_NODISCARD auto GetOzzSkeleton() const -> const ozz::animation::Skeleton*;
        MKT_NODISCARD auto GetInverseBindMatrices() const -> const eastl::vector<core::float4x4>&;

    private:
        // IDs are Ozz runtime joint indices,
        // shared by lookup, IK and palettes.
        eastl::vector<Joint> mJoints{};

        eastl::vector<core::float4x4> mInverseBindMats{};

        // Views point into the owned Ozz skeleton, whose storage stays stable on move.
        ankerl::unordered_dense::map<eastl::string_view, core::i32> mJointIndices{};

        // Run time skeleton
        ozz::unique_ptr<ozz::animation::Skeleton> mSkeleton{};
    };
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_SKELETON_HH
