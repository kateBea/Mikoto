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

#ifndef MIKOTO_SKELETON_HH
#define MIKOTO_SKELETON_HH

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>
#include <ozz/base/memory/unique_ptr.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/Joint.hh>

namespace mikoto::animation {

    // These 2 are vec4 because they need to match the bone influence
    // which is maximum bones influence a vertex ( from what we support now )
    inline constexpr u32 kMaxBoneInfluence{ 4 };
    inline constexpr u32 kMaxBonesPerMesh{ 256 }; // Needs to match shader's
    inline constexpr u32 kMaxSkinnedMeshes{ 1000 };

    class Skeleton final {
    public:
        explicit Skeleton( ozz::unique_ptr<ozz::animation::Skeleton>&& data = nullptr );

        // Allow move.
        Skeleton( Skeleton&& ) = default;
        auto operator=( Skeleton&& ) noexcept -> Skeleton& = default;

        DISABLE_COPY_FOR( Skeleton );

        MKT_NODISCARD auto HasJoint( eastl::string_view name ) const -> bool;

        MKT_NODISCARD auto FindJoint( eastl::string_view name ) -> Joint*;
        MKT_NODISCARD auto FindJoint( eastl::string_view name ) const -> const Joint*;

        MKT_NODISCARD auto FindJointByID( u32 ID ) -> Joint*;
        MKT_NODISCARD auto FindJointByID( u32 ID ) const -> const Joint*;

        MKT_NODISCARD auto IsArmaturePresent() const -> bool;
        MKT_NODISCARD auto GetOzzBoneIndex( u32 ID ) const -> i32;
        MKT_NODISCARD auto GetOzzSkeleton() -> ozz::animation::Skeleton*;
        MKT_NODISCARD auto GetOzzSkeleton() const -> const ozz::animation::Skeleton*;

        auto SetInverseBindMatrices( eastl::vector<float4x4>&& mats ) -> void;
        MKT_NODISCARD auto GetInverseBindMatrices() const -> const eastl::vector<float4x4>&;

    private:
        eastl::vector<float4x4> mInverseBindMats{};

        // To construct the skeleton
        ozz::animation::offline::SkeletonBuilder mBuilder{};
        ozz::animation::offline::RawSkeleton mRawSkeleton{};

        // Run time skeleton
        ozz::unique_ptr<ozz::animation::Skeleton> mSkeleton{};
    };
}

#endif // MIKOTO_SKELETON_HH