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

#ifndef MIKOTO_RENDERER_CLOUDS_CLOUD_DENSITY_BAKER_HH
#define MIKOTO_RENDERER_CLOUDS_CLOUD_DENSITY_BAKER_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Clouds/Clouds.hh>

namespace mikoto::renderer {

    enum class CloudSliceLayout : core::u32 {
        eProfileTypeDensity,       // Frostnova/Nubis modeling data: RGB.
        eDistanceProfileTypeDensity// Cumulus NVDF: encoded distance, profile, type, density.
    };

    // Empty channel names select profile/dimensional_profile, detail_type/detail,
    // and density_scale/density aliases. Import resolution is bounded to 16..256.
    // The editor imports an owned snapshot on a worker and installs it on the UI thread.
    struct CloudVolumeImportSettings {
        eastl::string mProfileGrid{}, mDetailGrid{}, mDensityGrid{};// Empty selects recognized channel names.
        core::u32 mMaxResolution{ 256 };
        core::f32 mDetailType{ .75f }, mProfileDepth{ .08f };
        CloudSliceLayout mSliceLayout{ CloudSliceLayout::eProfileTypeDensity };
    };

    // The offline fluid solver advects temperature, vapour and liquid, with cooling,
    // condensation/evaporation, buoyancy, vorticity, pressure projection and wind shear.
    // This is a bounded shape-authoring approximation, not calibrated meteorology.
    // References: GPU Gems 3 chapter 30 and Stam's stable-fluids solver:
    // https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids
    // https://www.dgp.toronto.edu/public_user/stam/reality/Research/pdf/GDC03.pdf
    struct CloudDensityBakeDescription {
        core::u32 mResolution{ 48 };
        core::u32 mSteps{ 48 };
        core::u32 mSeed{ 17 };
        core::f32 mBuoyancy{ 1.1f };
        core::f32 mVorticity{ 0.6f };
        core::f32 mHumidity{ 0.75f };
        core::f32 mCooling{ 0.35f };
        core::f32 mCondensation{ 1.0f };
        core::f32 mEvaporation{ 0.1f };
        core::f32 mWindShear{ 0.8f };
        core::u32 mPressureIterations{ 48 };
        CloudForm mCloudForm{ CloudForm::eCumulus };
        core::f32 mDetailType{ 0.75f };
        core::f32 mProfileDepth{ 0.08f };

        auto operator==( const CloudDensityBakeDescription & ) const -> bool = default;
    };

    struct CloudDensityBake {
        core::uint3 mDimensions{};
        eastl::vector<core::f32> mDensity{};
        CloudForm mCloudForm{ CloudForm::eCumulus };
        eastl::vector<CloudVoxel> mVoxels{};
    };

    enum class CloudLayerSource : core::u32 {
        eFluid,
        eEllipsoid,
        eBox,
        eGrid,
        eMesh,
        ePointCloud,
    };
    enum class CloudLayerOperation : core::u32 {
        eUnion,
        eSubtract,
        eIntersect,
        eReplace,
    };

    struct CloudAuthoringLayer {
        bool mEnabled{ true };
        CloudLayerSource mSource{ CloudLayerSource::eFluid };
        CloudLayerOperation mOperation{ CloudLayerOperation::eUnion };
        core::float3 mOffset{};// Fractions of the output box, centered at zero.
        core::float3 mScale{ 1.0f };
        core::float3 mRotation{};// Euler degrees.
        core::f32 mOpacity{ 1.0f };
        core::f32 mErosion{};
        core::f32 mSoftness{ 0.15f };
        core::f32 mPointRadius{ 0.035f };// Fraction of the source bounding box.
        CloudVolumeImportSettings mImport{};
        eastl::string mAssetPath{};// Absolute in memory, material-relative on disk.
        CloudDensityBakeDescription mFluid{};
    };

    // Layers compose in stack order: Union, Subtract, Intersect or Replace.
    // Offset/scale use fractions of the output box; rotation uses Euler degrees.
    // Opacity, erosion and softness apply to normalized masks before rebuilding the
    // final profile. Direct VDB import preserves coefficients better than Boolean masks.
    // Domain proportions retain thickness in thin sky layers. A larger box at unchanged
    // resolution reduces shape precision: runtime fields are dense, finite voxel grids.
    // Repeated fluid descriptions share their simulation during composition. Workers
    // own inputs; install results on the editor thread and save to retain recipe + grid.
    struct CloudAuthoringDescription {
        core::u32 mResolution{ 64 };// Legacy cubic resolution when dimensions are zero.
        core::uint3 mDimensions{};
        core::float3 mDomainAspect{ 1.0f };// World proportions; normalized to the shortest axis.
        core::f32 mProfileDepth{ 0.08f };
        eastl::vector<CloudAuthoringLayer> mLayers{};
    };

    struct CloudLayerGrid {
        core::uint3 mDimensions{};
        eastl::vector<CloudVoxel> mVoxels{};
    };

    /** Offline shape authoring: advect velocity/density, project pressure and
     * retain independent profile, detail-type and density-scale fields. It owns no scene, GPU or material resources.
     * Call from an authoring task, then install the result on the editor thread.
     */
    class CloudDensityBaker final {
    public:
        /** Editable sky layout built from reused, transformed fluid formations. */
        MKT_NODISCARD static auto CreateCumulusSkyRecipe() -> CloudAuthoringDescription;

        /** Composite transformed layers, then rebuild the final dimensional profile.
         * Grid entries correspond to recipe layers; callers load imported assets.
         * The worker owns all inputs and never changes live material/scene objects.
         */
        MKT_NODISCARD static auto Compose( const CloudAuthoringDescription &description,
            const eastl::vector<CloudLayerGrid> &grids ) -> CloudDensityBake;

        /** CPU voxelization of normalized source geometry. Triangle meshes must
         * be closed; point clouds are splatted using the normalized radius.
         */
        // Closed meshes use BVH ray parity; open planes are not solids. Point sources
        // are splatted using normalized radius. The geometry budget is two million
        // vertices/triangles, with bounded 16..256-axis output (at most 16M voxels).
        MKT_NODISCARD static auto Voxelize( const eastl::vector<core::float3> &positions,
            const eastl::vector<core::u32> &indices, core::uint3 dimensions, core::f32 pointRadius,
            core::f32 detailType, core::f32 profileDepth ) -> CloudLayerGrid;

        /** Build a distance-derived profile while retaining independent density.
         * Empty cells remain empty; profile depth is in normalized grid units.
         */
        // Scalar imports derive their envelope from occupied support using separable
        // Euclidean distance transforms to occupied/empty voxel centres. A half-voxel
        // surface adjustment and analytic box boundary define the sampled envelope.
        // The profile ramp is relative to the shortest physical extent, preserving
        // non-uniform sky-layer thickness. Empty support stays empty; supplied authored
        // profiles are preserved instead of reconstructed.
        // https://cs.brown.edu/people/pfelzens/papers/dt-final.pdf
        MKT_NODISCARD static auto BuildVolume( core::uint3 dimensions, const eastl::vector<core::f32> &density,
            core::f32 detailType = 0.75f, core::f32 profileDepth = 0.08f,
            core::float3 aspect = core::float3{ 1.0f } ) -> eastl::vector<CloudVoxel>;

        MKT_NODISCARD static auto Bake( const CloudDensityBakeDescription &description ) -> CloudDensityBake;
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CLOUDS_CLOUD_DENSITY_BAKER_HH
