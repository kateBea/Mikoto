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

#ifndef MIKOTO_ANIMATION_UTILITY_HH
#define MIKOTO_ANIMATION_UTILITY_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Animation/SkinningPalette.hh>

namespace mikoto::animation {

    /**
     * @brief Removes invalid influences and normalizes both sets of skinning weights.
     * @param[in,out] joints0 First four joint indices; invalid influences are removed.
     * @param[in,out] weights0 First four weights, normalized together with weights1.
     * @param[in,out] joints1 Second four joint indices; invalid influences are removed.
     * @param[in,out] weights1 Second four weights, normalized together with weights0.
     * @param jointCount Number of valid joint indices.
     */
    auto NormalizeSkinWeights(
        core::float4& joints0, core::float4& weights0,
        core::float4& joints1, core::float4& weights1,
        core::usize jointCount ) -> void;

    /**
     * Hashes a byte range and combines it with an existing animation fingerprint.
     *
     * @param seed Previous fingerprint, or zero to start a new one.
     * @param bytes Readable data containing at least size bytes.
     * @param size Number of bytes to hash, including any embedded nulls.
     * @returns Combined 64-bit fingerprint.
     */
    MKT_NODISCARD auto HashAnimationData( core::u64 seed, const void* bytes, core::usize size ) -> core::u64;

    /**
     * @brief Applies up to eight skinning influences to a vertex.
     * @param vertex Source position and up to eight joint influences.
     * @param palette Mesh-local joint matrices used by the skin.
     * @returns Skinned position, or the source position when no valid influence remains.
     */
    MKT_NODISCARD auto SkinVertexPosition( const SkinningVertex& vertex,
        eastl::span<const core::float4x4> palette ) -> core::float3;

    /**
     * @brief Establishes mesh bind space, retaining collapsed bind transforms.
     * @param[in,out] meshTransform Mesh bind transform; collapsed bind transforms may be rebased to identity.
     * @param[in,out] binding Skin binding updated with the resolved mesh bind transform.
     */
    MKT_NODISCARD auto InitializeSkinningSpace( core::float4x4& meshTransform, MeshSkinBinding& binding ) -> void;

    /**
     * @brief Builds mesh-local joint matrices from a model-space skeleton pose.
     * @param binding Mesh-to-skeleton joint mapping and inverse bind data.
     * @param jointModels Model-space matrices of the skeleton pose.
     * @param[out] palette Destination palette; must fit every joint in the binding.
     * @returns True when the joint mapping and output capacity are valid.
     */
    MKT_NODISCARD auto BuildSkinningPalette(
        const MeshSkinBinding& binding,
        eastl::span<const core::float4x4> jointModels,
        eastl::span<core::float4x4> palette ) -> bool;
}// namespace mikoto::animation

#endif// MIKOTO_ANIMATION_UTILITY_HH
