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

#ifndef MIKOTO_RENDERER_TERRAN_GENERATION_HH
#define MIKOTO_RENDERER_TERRAN_GENERATION_HH

#include <EASTL/array.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/vector.h>

#include <Assets/Asset.hh>
#include <Assets/AssetTexture.hh>
#include <Assets/Image.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Grass/Grass.hh>

namespace mikoto::renderer {

    using TerrainSector = core::float2;

    struct FBMSettings {
        core::f32 mFrequency{ 1.0f };
        core::f32 mAmplitude{ 1.0f };
        core::f32 mPersistence{ 0.5f };
        core::f32 mLacunarity{ 2.7f };
        core::u32 mOctaves{ 7 };

        MKT_NODISCARD auto operator==( const FBMSettings & ) const -> bool = default;
    };

    struct TerrainNoiseSettings {
        FBMSettings mFBM{};
        core::f32 mPerlinFrequency{ 8.0f };
        core::f32 mPerlinAmplitude{ 1.0f };

        // 0 = normalized fBm; 1 = the separate single-octave Perlin layer.
        core::f32 mPerlinBlend{ 0.25f };
        core::float2 mOffset{};

        MKT_NODISCARD auto IsValid() const -> bool;
        MKT_NODISCARD auto operator==( const TerrainNoiseSettings & ) const -> bool = default;
    };

    enum class LayerBlendType {
        eWeight,
        eAlpha,
        eHeight,
    };

    struct TerrainLayerDescription : public core::ReferenceCounted<TerrainLayerDescription> {
        asset::AssetRef<asset::Texture> mAlbedo{};
        asset::AssetRef<asset::Texture> mNormal{};
        asset::AssetRef<asset::Texture> mHeight{};

        core::float2 mTiling{ 1.0f, 1.0f };
        core::f32 mRoughness{ 0.8f };

        LayerBlendType mBlendType{ LayerBlendType::eWeight };
    };

    using TerrainLayerHandle = core::Ref<TerrainLayerDescription>;

    inline constexpr core::u32 kTerrainSurfaceLayerCount{ 4 };

    /** A weight-blended landscape layer. UV scale is repeats per world metre. */
    struct TerrainSurfaceLayer {
        eastl::string mName{ "Layer" };
        asset::AssetRef<asset::Texture> mAlbedo{};
        asset::AssetRef<asset::Texture> mNormal{};
        asset::AssetRef<asset::Texture> mHeight{};
        core::float3 mTint{ 1.0f };
        core::float2 mUVScale{ .1f };
        core::float2 mHeightRange{ 0.0f, 1.0f };
        core::f32 mRoughness{ .8f };
        core::f32 mNormalStrength{ 1.0f };
        core::f32 mSlopeInfluence{};
    };

    struct TerrainSurfaceSettings {
        TerrainSurfaceSettings();
        GrassSettings mGrass{};
        eastl::array<TerrainSurfaceLayer, kTerrainSurfaceLayerCount> mLayers{};
        core::f32 mBlendWidth{ .1f };
        core::f32 mHeightBlendWidth{ .2f };
        bool mHeightBlendEnabled{ true };
        bool mShowElevationColors{};
    };

    /** Immutable normalized RGBA layer weights shared by scene copies and undo. */
    struct TerrainPaintMap {
        core::u32 mWidth{};
        core::u32 mHeight{};
        eastl::vector<core::float4> mWeights{};
    };

    struct WorldDescription {
        // Terrain origin (XZ)
        TerrainSector mOrigin{ 0.0f, 0.0f };

        // Terrain topology
        core::u32 mQuadsPerSection{ 63 };
        core::u32 mSectionsPerComponent{ 1 };
        core::u32 mComponentsX{ 8 };
        core::u32 mComponentsY{ 8 };

        // Terrain scale
        float mVertexSpacing{ 1.0f };
        float mHeightScale{ 100.0f };
        float mBaseElevation{ 0.0f };

        // Linear RGB colors for the unlit elevation ramp.
        core::float3 mLowColor{ 0.06f, 0.22f, 0.08f };
        core::float3 mMiddleColor{ 0.42f, 0.32f, 0.14f };
        core::float3 mHighColor{ 0.90f, 0.93f, 0.96f };

        // Editing
        bool mEnableEditLayers{ false };

        MKT_NODISCARD auto GetQuadsPerComponent() const -> core::u32;
        MKT_NODISCARD auto GetResolutionX() const -> core::u32;
        MKT_NODISCARD auto GetResolutionY() const -> core::u32;
        MKT_NODISCARD auto GetTotalComponents() const -> core::u32;
        MKT_NODISCARD auto GetWorldSize() const -> TerrainSector;
    };

    // Bound the resident grid before allocation or resolution calculations.
    MKT_NODISCARD auto IsTerrainDescriptionValid( const WorldDescription &description ) -> bool;

    /** Immutable image shared by scene copies, terrain sampling and its preview.
     * Red supplies height: LDR is linear [0, 1]; HDR retains its float values.
     * Image row zero maps to the terrain's minimum local Z. */
    struct TerrainHeightmap {
        explicit TerrainHeightmap( asset::ImageHandle image );

        core::u32 mWidth{};
        core::u32 mHeight{};
        asset::ImageHandle mImage{};

        MKT_NODISCARD auto Sample( core::float2 uv ) const -> core::f32;
    };

    /** Builds an owned float height image; no source asset is modified. */
    MKT_NODISCARD auto CreateTerrainHeightmap( core::u32 width, core::u32 height,
        const eastl::vector<core::f32> &heights ) -> eastl::shared_ptr<const TerrainHeightmap>;

    MKT_NODISCARD auto TerrainAutomaticWeights( core::f32 height, core::f32 normalY,
        const TerrainSurfaceSettings &surface ) -> core::float4;

    /** Intersects the same two triangles per cell as the GPU terrain generator.
     * Direction is a finite segment in terrain local space; returns its fraction.
     */
    MKT_NODISCARD auto IntersectTerrain( const WorldDescription &world,
        const eastl::shared_ptr<const TerrainHeightmap> &heights, core::float3 origin,
        core::float3 direction, core::float3 &position ) -> bool;

    /** A generation request owns its immutable settings and eventual image.
     * Readback completes on the render thread. Holding this request in the callback
     * avoids accessing a removed entity or replacing newer editor settings.
     */
    class TerrainNoiseGeneration : public core::RevisionTracked {
    public:
        explicit TerrainNoiseGeneration( const TerrainNoiseSettings &settings );

        auto Fail( const eastl::string &error ) -> void;
        auto Complete( asset::ImageHandle image ) -> void;

        MKT_NODISCARD auto GetError() const -> const eastl::string &;
        MKT_NODISCARD auto GetSettings() const -> TerrainNoiseSettings;
        MKT_NODISCARD auto GetHeightmap() const -> eastl::shared_ptr<const TerrainHeightmap>;

    private:
        TerrainNoiseSettings mSettings{};
        eastl::shared_ptr<const TerrainHeightmap> mHeightmap{};
        eastl::string mError{};
    };

}// namespace mikoto::renderer

#endif//MIKOTO_RENDERER_TERRAN_GENERATION_HH
