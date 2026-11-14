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

#ifndef MIKOTO_MATH_POINT_BOUNDS_TREE_HH
#define MIKOTO_MATH_POINT_BOUNDS_TREE_HH

#include <EASTL/span.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/numeric_limits.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Math/Bounds.hh>

namespace mikoto::math {

    // Immutable mesh vertices are indexed once at import. A transformed bound
    // is six support queries: each rejects subtrees that cannot improve the
    // current extremum. This preserves exact vertex bounds under rotation and
    // shear without visiting the whole mesh for every selected instance.
    class PointBoundsTree {
    public:
        auto Build( eastl::span<const core::float3> points ) -> void;

        // Points must be the unchanged geometry used by Build. The tree owns
        // indices only, so it neither duplicates positions nor retains pointers.
        auto Transform( eastl::span<const core::float3> points, const core::float4x4& transform ) const -> Bounds;

    private:
        struct Node {
            Bounds mBounds;
            core::usize mFirst{};
            core::usize mCount{};
            core::usize mRight{};
        };

    private:
        MKT_NODISCARD auto BuildNode( eastl::span<const core::float3> points, core::usize first, core::usize count ) -> core::usize;
        MKT_NODISCARD auto Support( eastl::span<const core::float3> points, const core::float3& direction ) const -> core::f32;

        MKT_NODISCARD auto SupportNode(
            eastl::span<const core::float3> points,
            const core::float3& direction,
            core::usize index, core::f32& best ) const -> void;

        MKT_NODISCARD static auto UpperBound( const Bounds& bounds, const core::float3& direction ) -> core::f32;

    private:
        eastl::vector<Node> mNodes{};
        eastl::vector<core::usize> mIndices{};
    };
}// namespace mikoto::math

#endif// MIKOTO_MATH_POINT_BOUNDS_TREE_HH
