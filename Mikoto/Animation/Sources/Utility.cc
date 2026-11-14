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

#include <algorithm>
#include <string_view>

#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <Core/Profiler.hh>

#include <Animation/Utility.hh>

namespace mikoto::animation {

    // https://www.boost.org/doc/libs/latest/libs/container_hash/doc/html/hash.html#notes_hash_combine
    auto HashAnimationData( core::u64 seed, const void* bytes, core::usize size ) -> core::u64 {
        MKT_BEGIN_PROFILER();
        const std::string_view data{ static_cast<const char*>( bytes ), size };
        const core::u64 hash{ ankerl::unordered_dense::hash<std::string_view>{}( data ) };

        return seed ^ ( hash + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 ) );
    }

    auto SkinVertexPosition( const SkinningVertex& vertex, eastl::span<const core::float4x4> palette ) -> core::float3 {
        MKT_BEGIN_PROFILER();
        core::float4 position{ 0.0f };
        float totalWeight{};

        for ( int influence = 0; influence < 8; ++influence ) {
            const float joint = influence < 4 ? vertex.mJoints0[influence] : vertex.mJoints1[influence - 4];
            const float weight = influence < 4 ? vertex.mWeights0[influence] : vertex.mWeights1[influence - 4];

            // Match ApplySkinning in Animation_Helpers.slang, including the
            // second influence set and the unweighted-vertex fallback.
            if ( weight > 0 && joint >= 0 && joint < palette.size() ) {
                position += weight * ( palette[static_cast<size_t>( joint )] * core::float4{ vertex.mPosition, 1.0f } );
                totalWeight += weight;
            }
        }

        return totalWeight > 0.000001f ? core::float3{ position / totalWeight } : vertex.mPosition;
    }

    auto NormalizeSkinWeights( core::float4& joints0, core::float4& weights0,
        core::float4& joints1, core::float4& weights1, core::usize jointCount ) -> void {
        MKT_BEGIN_PROFILER();
        float total{};

        for ( int set = 0; set < 2; ++set ) {
            auto& joints{ set == 0 ? joints0 : joints1 };
            auto& weights{ set == 0 ? weights0 : weights1 };

            for ( int i = 0; i < 4; ++i ) {
                if ( weights[i] <= 0.0f ||
                     joints[i] < 0.0f || joints[i] >= jointCount || glm::floor( joints[i] ) != joints[i] ) {
                    weights[i] = 0.0f;
                    joints[i] = 0.0f;// Unused influences must never become an unsigned -1 index.
                }

                total += weights[i];
            }
        }

        if ( total > 0.000001f ) {
            weights0 /= total;
            weights1 /= total;
        }
        else {
            weights0 = weights1 = core::float4{};
        }
    }

    auto InitializeSkinningSpace( core::float4x4& meshTransform, MeshSkinBinding& binding ) -> void {
        MKT_BEGIN_PROFILER();
        binding.mBindPoseTransform = core::float4x4{ 1.0f };
        const float determinant{ glm::determinant( meshTransform ) };

        if ( determinant != 0.0f ) {
            const auto inverse{ glm::inverse( meshTransform ) };
            binding.mInverseMeshBind = inverse;
            return;
        }
        // Zero/tiny scale is also used to hide animated attachments. An inverse
        // cannot recover a collapsed bind pose; keep source vertices unchanged and
        // evaluate animation in model space instead, allowing them to become visible.
        binding.mBindPoseTransform = meshTransform;
        binding.mInverseMeshBind = core::float4x4{ 1.0f };
        meshTransform = core::float4x4{ 1.0f };
    }

    auto BuildSkinningPalette( const MeshSkinBinding& binding, eastl::span<const core::float4x4> jointModels, eastl::span<core::float4x4> palette ) -> bool {
        MKT_BEGIN_PROFILER();
        eastl::fill( palette.begin(), palette.end(), core::float4x4{ 1.0f } );

        if ( binding.mJoints.empty() || binding.mJoints.size() > palette.size() ) {
            return false;
        }

        for ( const auto& joint: binding.mJoints ) {
            if ( joint.mSkeletonJoint >= jointModels.size() ) {
                return false;
            }
        }

        for ( core::usize i = 0; i < binding.mJoints.size(); ++i ) {
            const auto& joint{ binding.mJoints[i] };
            // The entity already includes the imported mesh transform. Remove it here,
            // so scene ancestor transforms (especially unit scales) are applied exactly once.
            palette[i] = binding.mInverseMeshBind * jointModels[joint.mSkeletonJoint] * joint.mInverseBind;
        }

        return true;
    }
}// namespace mikoto::animation
