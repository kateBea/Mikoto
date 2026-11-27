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

#ifndef MIKOTO_RENDERER_GRASS_GRASS_HH
#define MIKOTO_RENDERER_GRASS_GRASS_HH

#include <EASTL/vector.h>

#include <Assets/Asset.hh>

#include <Core/Types.hh>

namespace mikoto::asset {
    class Model;
}

namespace mikoto::renderer {

    inline constexpr core::f32 kMaxGrassViewDistance{ 2000.0f };

    // Procedural blades use two strips; custom meshes retain authored triangles.
    // Mirrored by base/Grass_Helpers.slang.
    inline constexpr core::u32 kGrassLodCount{ 2 };
    inline constexpr core::u32 kGrassHighBladeVertexCount{ 15 };
    inline constexpr core::u32 kGrassLowBladeVertexCount{ 7 };
    inline constexpr core::u32 kGrassTileSide{ 32 };
    inline constexpr core::u32 kGrassTileCandidateCount{ kGrassTileSide * kGrassTileSide };
    inline constexpr core::u32 kGrassMaxTileLevel{ 16 };
    inline constexpr core::u32 kGrassGenerateGroupSize{ 128 };
    inline constexpr core::u32 kMaxGrassBlades{ 1572864 };

    /** A static mesh choice. Positive weights are relative probabilities, not densities. */
    struct GrassMeshVariant {
        asset::AssetRef<asset::Model> mMesh{};
        core::u32 mMeshIndex{};
        core::f32 mWeight{ 1.0f };
        core::f32 mHeightScale{ 1.0f };
        core::f32 mWidthScale{ 1.0f };
    };

    /** Per-landscape GrassFlow-derived placement, blade deformation and lighting.
     * Imported meshes use Y-up, independent texture UVs, and their selected
     * mesh's base-color/alpha material. Their rest height/width are normalized
     * once, so the same metre controls apply to generated blades and custom tufts.
     * World-anchored quadtree leaves control placement detail independently of
     * the two blade geometry LODs. A shared hierarchical root rank makes tile
     * subdivision invisible to density selection. GPU compaction remains bounded.
     * Root AO is a material approximation; it attenuates indirect sky lighting.
     * Reference: https://github.com/Mithzzx/Project-GrassFlow (MIT).
     */
    struct GrassSettings {
        bool mEnabled{};
        core::u32 mLayer{ 1 };
        core::u32 mSeed{ 721 };
        bool mUseCustomMesh{};
        asset::AssetRef<asset::Model> mMesh{};
        core::u32 mMeshIndex{};
        core::f32 mMeshWeight{ 1.0f };
        eastl::vector<GrassMeshVariant> mMeshVariants{};
        bool mOcclusionEnabled{ true };
        bool mCastShadows{ true };
        bool mContactShadows{ true };
        core::f32 mShadowDensity{ 1.0f };
        core::f32 mDensity{ 64.0f };
        core::f32 mHeight{ .7f };
        core::f32 mWidth{ .045f };
        core::f32 mBend{ .3f };
        core::f32 mClumpSize{ 2.5f };
        core::f32 mClumpStrength{ .8f };
        core::f32 mHeightVariation{ .5f };
        core::f32 mViewDistance{ 250.0f };
        // Blade geometry morphs from the high strip to the low strip over this
        // interval. Imported meshes retain their authored topology at both LODs.
        core::f32 mLodDistance{ 40.0f };
        core::f32 mFarLodDistance{ 100.0f };
        core::f32 mMinDensity{ .15f };
        core::f32 mWidthCompensation{ .8f };
        core::f32 mMaxSlope{ 40.0f };
        core::f32 mMinElevation{ .5f };
        core::f32 mMaxElevation{ 10000.0f };
        core::float2 mWindDirection{ 1.0f, .2f };
        core::f32 mWindSpeed{ 2.5f };
        core::f32 mWindStrength{ .35f };
        core::f32 mWindFrequency{ .05f };
        core::f32 mWindFlutter{ .1f };
        core::f32 mWindDistance{ 120.0f };
        core::f32 mAmbientOcclusion{ .85f };
        core::f32 mSelfShadow{ .6f };
        core::f32 mRoughness{ .65f };
        core::f32 mTranslucency{ .6f };
        core::f32 mTranslucencyPower{ 3.0f };
        core::f32 mNormalRounding{ .3f };
        core::f32 mDryAmount{ .15f };
        core::f32 mColorVariation{ .2f };
        core::float3 mRootColor{ .04f, .08f, .02f };
        core::float3 mMidColor{ .15f, .35f, .08f };
        core::float3 mTipColor{ .45f, .55f, .15f };
        core::float3 mDryColor{ .4f, .35f, .15f };
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_GRASS_GRASS_HH
