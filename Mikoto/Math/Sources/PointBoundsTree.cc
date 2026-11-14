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

#include <EASTL/sort.h>
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
#include <Math/PointBoundsTree.hh>

namespace mikoto::math {

    auto PointBoundsTree::Build( eastl::span<const core::float3> points ) -> void {
        MKT_BEGIN_PROFILER();

        mNodes.clear();
        mIndices.clear();
        mIndices.reserve( points.size() );

        for ( core::usize i{}; i < points.size(); ++i ) {
            mIndices.push_back( i );
        }

        if ( !mIndices.empty() ) {
            // Balanced splits stop at 32 points. Reserve an upper bound
            // once instead of relocating nodes during construction.
            mNodes.reserve( 2 * ( ( mIndices.size() + 15 ) / 16 ) - 1 );

            BuildNode( points, 0, mIndices.size() );
        }
    }


    auto PointBoundsTree::Transform( eastl::span<const core::float3> points, const core::float4x4& transform ) const -> Bounds {
        MKT_BEGIN_PROFILER();
        Bounds result{};

        if ( mNodes.empty() ) {
            return result;
        }

        for ( core::i32 row = 0; row < 3; ++row ) {
            const core::float3 direction{ transform[0][row], transform[1][row], transform[2][row] };
            result.mMinimum[row] = -Support( points, -direction ) + transform[3][row];
            result.mMaximum[row] = Support( points, direction ) + transform[3][row];
        }

        result.mIsValid = true;
        return result;
    }


    auto PointBoundsTree::BuildNode( eastl::span<const core::float3> points, core::usize first, core::usize count ) -> core::usize {
        MKT_BEGIN_PROFILER();
        const auto index{ mNodes.size() };
        mNodes.emplace_back();

        Bounds bounds{};

        for ( core::usize i = first; i < first + count; ++i ) {
            bounds.Include( points[mIndices[i]] );
        }

        mNodes[index] = { bounds, first, count, 0 };
        const auto extent{ bounds.mMaximum - bounds.mMinimum };

        core::i32 axis{};

        if ( extent.y > extent.x ) {
            axis = 1;
        }

        if ( extent.z > extent[axis] ) {
            axis = 2;
        }

        if ( count <= 32 || extent[axis] == 0 ) {
            return index;
        }

        const auto middle = first + count / 2;
        eastl::nth_element( mIndices.begin() + first, mIndices.begin() + middle, mIndices.begin() + first + count,
            [&]( core::usize a, core::usize b ) {
            return points[a][axis] < points[b][axis];
        } );

        BuildNode( points, first, middle - first );// Left child follows its parent.

        const auto right = BuildNode( points, middle, first + count - middle );
        mNodes[index].mRight = right;
        mNodes[index].mCount = 0;
        return index;
    }


    auto PointBoundsTree::UpperBound( const Bounds& bounds, const core::float3& direction ) -> core::f32 {
        MKT_BEGIN_PROFILER();
        const core::float3 corner{
            direction.x >= 0 ? bounds.mMaximum.x : bounds.mMinimum.x,
            direction.y >= 0 ? bounds.mMaximum.y : bounds.mMinimum.y,
            direction.z >= 0 ? bounds.mMaximum.z : bounds.mMinimum.z
        };

        return glm::dot( corner, direction );
    }


    auto PointBoundsTree::Support( eastl::span<const core::float3> points, const core::float3& direction ) const -> core::f32 {
        MKT_BEGIN_PROFILER();
        const core::i32 axes = static_cast<core::i32>( direction.x != 0 ) +
                               static_cast<core::i32>( direction.y != 0 ) + static_cast<core::i32>( direction.z != 0 );

        // Translation, axis-aligned scaling and axis permutations can use
        // the imported bounds directly, with no vertex reads at selection.
        if ( axes <= 1 ) {
            return UpperBound( mNodes.front().mBounds, direction );
        }

        core::f32 best = std::numeric_limits<core::f32>::lowest();

        SupportNode( points, direction, 0, best );
        return best;
    }


    auto PointBoundsTree::SupportNode( eastl::span<const core::float3> points, const core::float3& direction, core::usize index, core::f32& best ) const -> void {
        MKT_BEGIN_PROFILER();
        const auto& node{ mNodes[index] };

        if ( UpperBound( node.mBounds, direction ) <= best ) {
            return;
        }

        if ( node.mCount ) {
            for ( auto i = node.mFirst; i < node.mFirst + node.mCount; ++i ) {
                best = eastl::max( best, glm::dot( points[mIndices[i]], direction ) );
            }

            return;
        }

        const auto left{ index + 1 };
        const auto right{ node.mRight };

        // Finding a good extremum first makes the other subtree cheaper.
        if ( UpperBound( mNodes[left].mBounds, direction ) >= UpperBound( mNodes[right].mBounds, direction ) ) {
            SupportNode( points, direction, left, best );
            SupportNode( points, direction, right, best );
        }
        else {
            SupportNode( points, direction, right, best );
            SupportNode( points, direction, left, best );
        }
    }
}// namespace mikoto::math