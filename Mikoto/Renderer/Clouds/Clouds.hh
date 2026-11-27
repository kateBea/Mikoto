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

#ifndef MIKOTO_RENDERER_CLOUDS_CLOUDS_HH
#define MIKOTO_RENDERER_CLOUDS_CLOUDS_HH

#include <Core/Types.hh>

namespace mikoto::renderer {

    inline constexpr core::u32 kMaxCloudVolumes{ 8 };
    inline constexpr core::u32 kMaxCloudLocalLights{ 8 };
    inline constexpr core::u32 kMaxCloudBoltSegments{ 512 };

    enum class CloudForm : core::u32 {
        eStratus,
        eStratocumulus,
        eCumulus,
        eCirrus
    };

    /** Authored cloud channels in [0,1], with X changing fastest in a grid.
     * Profile is a distance-derived envelope, detail type blends wisps to billows,
     * and density scale controls extinction independently of that envelope.
     */
    // Runtime layout is 12 bytes (profile/detail type/density scale), X fastest.
    // The model texture packs signed distance/detail/density/profile into RGBA16F.
    // Authored/noisy profiles survive acceleration rebuilds; scalar media consumers
    // receive profile * density scale. Sampling is trilinear, with finite [0,1] channels.
    // Reference: Nubis Cubed (SIGGRAPH 2023), independent voxel channels:
    // https://www.guerrilla-games.com/read/nubis-cubed
    struct CloudVoxel {
        core::f32 mProfile{};
        core::f32 mDetailType{};
        core::f32 mDensityScale{};
    };

    static_assert( sizeof( CloudVoxel ) == 3 * sizeof( core::f32 ) );

    enum class CloudsDebugView : core::u32 {
        eLighting,
        eDensity,
        eDistanceField,
        eMarchSteps
    };

    /** Raster voxel-cloud quality. Shapes and optical properties remain in CloudComponent/VolumeMaterial. */
    struct CloudsSettings {
        bool mEnabled{ true };
        bool mHalfResolution{ true };
        bool mCastSurfaceShadows{ true };
        bool mDistanceFieldEnabled{ true };

        core::u32 mFieldResolution{ 128 };
        core::u32 mMaxSteps{ 192 };
        core::u32 mSunSteps{ 16 };
        core::f32 mStepLength{ 8.0f };
        core::f32 mMaxDistance{ 60000.0f };
        core::f32 mDetailStrength{ 1.0f };
        core::f32 mMultipleScattering{ 0.5f };
        core::f32 mAmbientIntensity{ 0.25f };
        core::f32 mHistoryWeight{ 0.9f };
        core::f32 mEdgeDarkening{ 0.4f };
        core::f32 mInnerGlow{ 0.5f };
        core::f32 mDetailMipBias{};
        core::f32 mAnimatedJitterDistance{ 250.0f };
        CloudsDebugView mDebugView{ CloudsDebugView::eLighting };

        auto operator==( const CloudsSettings & ) const -> bool = default;
    };

    /** Internal cloud flashes; time is scene volume time in seconds. */
    struct CloudLightningSettings {
        bool mEnabled{};
        bool mAutomatic{ true };
        bool mBoltsEnabled{ true };
        core::f32 mBoltLength{ 300.0f };
        core::f32 mBoltWidth{ 0.4f };
        core::u32 mBoltBranches{ 4 };
        core::float3 mPosition{};// Fractions of the cloud box, centered at zero.
        core::float3 mColor{ 0.65f, 0.78f, 1.0f };
        core::f32 mIntensity{ 120.0f };
        core::f32 mRadius{ 1000.0f };// World metres.
        core::f32 mInterval{ 6.0f };
        core::f32 mDuration{ 0.35f };
        core::u32 mSeed{ 17 };
    };

    struct CloudLightningState {
        core::u32 mBoltSeed{};
        core::float4 mPositionRadius{};
        core::float4 mColorIntensity{};
    };

    /** Bounded raster-light records. Optical depth is evaluated between each
     * voxel and its light; shadow maps provide opaque/alpha-tested mesh visibility.
     */
    struct CloudLocalLight {
        core::float4 mPositionRange{};
        core::float4 mDirectionOuter{};
        core::float4 mColorIntensity{};
        core::float4 mParameters{};// Inner cone cosine, source radius, reserved.
        core::uint4 mControl{};    // Point/spot type, mesh shadows, shadow record, reserved.
    };

    struct CloudMeshShadow {
        core::float4x4 mViewProjection[6]{};
        core::float4 mCascadeSplits{};
        core::float4 mPositionRange{};
        core::u32 mTextureIDs[6]{};
        core::u32 mFaceCount{};
        core::i32 mLightType{};
        core::float4 mBias{};
    };

    // Shared with the ordinary alpha-tested shadow shaders.
    static_assert( sizeof( CloudMeshShadow ) == 464 );
    static_assert( sizeof( CloudLocalLight ) == 80 );

    enum class CloudInteractionShape : core::u32 {
        eSphere,
        eBox,
        eCapsule
    };

    /** World-space, subtractive density displacement. The underlying authored field remains intact. */
    struct CloudInteractionRecord {
        core::float4x4 mWorldToLocal{ 1.0f };
        core::float4 mParameters{};// Shape, strength, feather, reserved.
    };

    static_assert( sizeof( CloudInteractionRecord ) == 80 );

    struct CloudBoltSegment {
        core::float4 mStartWidth{};
        core::float4 mEndIntensity{};
        core::float4 mColor{};
    };

    // These records mirror Clouds_Helpers.slang. The cached field is authored in
    // local space so moving/wind-advected clouds retain their voxel data.
    struct CloudRecord {
        core::float4x4 mWorldToLocal{ 1.0f };
        core::float4x4 mLocalToWorld{ 1.0f };
        core::float4 mExtinction{};
        core::float4 mScatteringPhase{};
        core::float4 mDensityDetail{};           // density multiplier, base/detail frequency, erosion.
        core::float4 mShape{};                   // coverage, detail type, conservative world scale, profile depth.
        core::float4 mWind{};                    // World velocity; temporal reprojection follows this motion.
        core::uint4 mFlags{};                    // Shape/model/seed; flags: cast, receive, authored cloud channels.
        core::uint4 mGrid{};                     // Imported density dimensions and byte offset.
        core::uint4 mTextures{};                 // Model/light SRVs, packed-distance byte offset and enable flag.
        core::float4 mWeather{};                 // Weather frequency, coverage/type/height variation.
        core::float4 mFormation{ 16, 1, .7f, 0 };// Seed count, cluster scale, variation, reserved.
        core::float4 mProfile{};                 // Cloud form, wind shear in metres, local wind direction XZ.
        core::float4 mLightningPositionRadius{};
        core::float4 mLightningColorIntensity{};
    };

    static_assert( sizeof( CloudRecord ) == 336 );

    struct CloudsFrameData {
        core::float4x4 mInverseViewProjection{ 1.0f };
        core::float4x4 mPreviousViewProjection{ 1.0f };
        core::float4x4 mPreviousInverseViewProjection{ 1.0f };
        core::float4 mCamera{};
        core::float4 mPreviousCamera{};
        core::float4 mBoundsMin{};
        core::float4 mBoundsMax{};
        core::float4 mSunDirection{};
        core::float4 mSunColor{};
        core::float4 mAmbientColor{};
        core::float4 mScreen{};           // Full and trace resolutions.
        core::float4 mMarch{};            // Step length, maximum distance, history weight, multiple scattering.
        core::float4 mClock{};            // Current time, elapsed time, detail strength, transmittance cutoff.
        core::uint4 mControl{};           // Volume count, jitter frame, valid history, debug view.
        core::uint4 mResources{};         // Noise SRV, clamp sampler, repeat sampler, surface shadows.
        core::uint4 mResolution{};        // Field XYZ and sun march budget.
        core::uint4 mFiltering{};         // View march budget, light-grid reset, atmosphere enabled, aerial enabled.
        core::uint4 mAtmosphereTextures{};// Sun transmittance, aerial radiance, aerial transmittance, sampler.
        core::uint4 mFogTextures{};       // Integrated local-fog radiance, transmission, sampler, reserved.
        core::uint4 mNoiseMips{};         // Filtered noise at 64, 32, 16 and 8 cubed.
        core::float4 mDetail{};           // Edge darkening, inner glow, mip bias, animated-jitter range.
        core::uint4 mShapeResources{};    // Base-shape noise and 2D weather map SRVs, reserved.
        core::uint4 mLightingControl{};   // Local lights, bolt segments, active sun maps, interactions.
        core::uint4 mLocalTextures[2]{};
        core::u64 mInteractions{};
        core::u64 mLocalLights{};
        core::u64 mMeshShadows{};
        core::u64 mBolts{};
        core::u64 mModels{};
        core::u64 mAtmosphere{};
        core::u64 mFogFrame{};
        core::u64 mDistanceFields{};// Packed conservative distances, one byte per model voxel.
        core::u64 mTerrainVertices{};
        core::u64 mTerrainRecords{};
        core::u64 mTerrainBounds{};
    };

    static_assert( sizeof( CloudsFrameData ) == 632 );

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CLOUDS_CLOUDS_HH
