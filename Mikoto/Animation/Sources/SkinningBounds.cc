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

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>

#include <Animation/SkinningBounds.hh>
#include <Animation/SkinningPalette.hh>

namespace mikoto::animation {
    auto SkinningBounds::Build( eastl::span<const SkinningVertex> vertices, core::usize paletteSize ) -> void {
        MKT_BEGIN_PROFILER();
        mJoints.clear();
        mUnweighted = {};
        eastl::vector<math::Bounds> bounds( paletteSize );

        for ( const auto& vertex: vertices ) {
            core::f32 totalWeight{};

            for ( core::u32 influence{}; influence < 8; ++influence ) {
                const core::f32 joint{ influence < 4 ? vertex.mJoints0[influence] : vertex.mJoints1[influence - 4] };
                const core::f32 weight{ influence < 4 ? vertex.mWeights0[influence] : vertex.mWeights1[influence - 4] };

                if ( weight > 0.0f &&
                     joint >= 0.0f && joint < paletteSize ) {
                    totalWeight += weight;
                }
            }
            // Match the skinning shader's threshold and unweighted fallback.
            if ( totalWeight <= 0.000001f ) {
                mUnweighted.Include( vertex.mPosition );
                continue;
            }

            for ( core::u32 influence{}; influence < 8; ++influence ) {
                const core::f32 joint{ influence < 4 ? vertex.mJoints0[influence] : vertex.mJoints1[influence - 4] };
                const core::f32 weight{ influence < 4 ? vertex.mWeights0[influence] : vertex.mWeights1[influence - 4] };

                if ( weight > 0.0f &&
                     joint >= 0.0f && joint < paletteSize ) {
                    bounds[static_cast<core::usize>( joint )].Include( vertex.mPosition );
                }
            }
        }

        for ( core::usize joint{}; joint < bounds.size(); ++joint ) {
            const auto& bound{ bounds[joint] };

            if ( bound.mIsValid ) {
                mJoints.push_back( { static_cast<core::u32>( joint ),
                    bound.mMinimum * 0.5f + bound.mMaximum * 0.5f,
                    bound.mMaximum * 0.5f - bound.mMinimum * 0.5f } );
            }
        }
    }

    auto SkinningBounds::Transform( eastl::span<const core::float4x4> palette,
        const core::float4x4& transform ) const -> math::Bounds {
        MKT_BEGIN_PROFILER();
        math::Bounds result{};
        const auto include = [&]( const core::float3& center, const core::float3& extent,
                                 const core::float4x4& matrix ) {
            const core::float3 transformedCenter{ matrix * core::float4{ center, 1.0f } };
            // An affine AABB transforms as c'=M*c, e'=abs(M)*e. This includes
            // negative/nonuniform scale and shear without visiting eight corners.
            const core::float3 transformedExtent{
                glm::abs( core::float3{ matrix[0] } ) * extent.x +
                glm::abs( core::float3{ matrix[1] } ) * extent.y +
                glm::abs( core::float3{ matrix[2] } ) * extent.z
            };
            result.Include( transformedCenter - transformedExtent );
            result.Include( transformedCenter + transformedExtent );
        };
        // Every normalized positive-weight skinning result is a convex combination
        // of joint-transformed positions. The union AABB contains those positions
        // and their combinations, so it encloses the whole deformed mesh.
        // It is conservative, not the tight per-vertex box; cost is O(joints).
        for ( const auto& joint: mJoints ) {
            if ( joint.mJoint >= palette.size() ) {
                // The bounds were built for a different palette capacity. Do not
                // return an apparently valid partial box.
                return {};
            }

            include( joint.mCenter, joint.mExtent, transform * palette[joint.mJoint] );
        }

        if ( mUnweighted.mIsValid ) {
            include( mUnweighted.mMinimum * 0.5f + mUnweighted.mMaximum * 0.5f,
                mUnweighted.mMaximum * 0.5f - mUnweighted.mMinimum * 0.5f, transform );
        }

        return result;
    }
}// namespace mikoto::animation
