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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_SHADOW_MAPPING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_SHADOW_MAPPING_MODULE_HH

#include <array>

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>

#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

namespace mikoto::renderer {

    // A bounded shared command arena avoids allocating worst-case storage for every map.
    static constexpr core::u32 kMaxShadowCommands{ 1'000'000 };

    // Shared light budgets used when assigning shadow records and graph resources.
    constexpr core::u32 kMaxShadowLightsPerType{ 5 };
    constexpr core::u32 kDirectionalCascades{ 4 };
    constexpr core::u32 kMaxShadowRecords{ kMaxShadowLightsPerType * 3 };
    constexpr core::usize kMaxShadowViews{ kMaxShadowLightsPerType * ( kDirectionalCascades + 6 + 1 ) };

    enum class ShadowMapQuality {
        eLow,
        eMedium,
        eHigh,
    };

    struct ShadowDiagnostics {
        std::array<core::u32, 3> mActiveLights{};
        std::array<core::u32, 3> mMappedLights{};
        core::u32 mSubmittedMaps{};
        core::u32 mCasterBatches{};
        core::u32 mSubmittedBatches{};
        core::u32 mTestedInstances{};
        core::u32 mCulledInstances{};
        core::u32 mUnculledViews{};
    };

    struct ShadowMapInfo {
        core::u32 mRecordCount{};
        FGBufferHandle mRecordsBuffer{};
        FGBufferHandle mIndirectBuffer{};
        FGPipelineHandle mPipeline{};
        eastl::array<FGTextureHandle, kMaxShadowLightsPerType> mSpot{};

        eastl::fixed_vector<FGTextureHandle, kMaxShadowViews, false> mMaps{};

        eastl::array<eastl::array<FGTextureHandle, 6>, kMaxShadowLightsPerType> mPoint{};
        eastl::array<eastl::array<FGTextureHandle, kDirectionalCascades>, kMaxShadowLightsPerType> mDirectional{};
    };

    class ShadowMappingModule final {
    public:
        explicit ShadowMappingModule( ShadowMapQuality quality = ShadowMapQuality::eMedium );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetGeometryManager( GeometryCullModule& geometry ) -> void;

        auto SetShadowMappDistance( core::f32 distance = 200 ) -> void;

        // Configure before graph registration, just like the renderer's resolution.
        auto RegisterUploadPass( FrameGraph& graph ) -> void;
        auto RegisterShadowMaps( FrameGraph& graph ) -> void;

        auto SetShadowMapsQuality( ShadowMapQuality quality ) -> void;
        auto SetCascadedShadowsEnabled( bool enabled ) -> void;
        auto SetSoftShadowsEnabled( bool enabled ) -> void { mSoftShadowsEnabled = enabled; }

        MKT_NODISCARD auto GetDiagnostics() const -> const ShadowDiagnostics&;

    private:
        struct alignas( 16 ) ShadowMapParameters {
            core::float4x4 mViewProjection[6]{};
            core::float4 mCascadeSplits{};
            core::float4 mPositionRange{};
            core::u32 mTextureIDs[6]{};
            core::u32 mFaceCount{};
            core::i32 mLightType{};
            // Constant/slope offsets: meters for directional lights, normalized
            // radial distance for local lights. The shader converts per cascade.
            core::float4 mBias{};
        };

        struct View {
            FGTextureHandle mTarget{};
            core::u32 mRecord{};
            core::u32 mFace{};
            core::u32 mSize{};
            core::u32 mFirstDraw{};
            core::u32 mDrawCount{};
            bool mUseUnculled{};
        };

        struct DirectionalCascadeData {
            eastl::array<core::float4x4, kDirectionalCascades> matrices{};
            core::float4 splits{};
        };

    private:
        auto SafeLightDirection( core::float3 direction ) -> core::float3;
        auto LightView( core::float3 position, core::float3 direction ) -> core::float4x4;
        auto PointShadowMatrices( core::float3 position, core::f32 radius ) -> eastl::array<core::float4x4, 6>;
        auto SpotShadowMatrix( core::float3 position, core::float3 direction, core::f32 outerCosine, core::f32 radius ) -> core::float4x4;

        // Fit the whole visible range when cascades are disabled, or split it
        // into four depth ranges. Near ranges get
        // more shadow texels per world unit, while distant ranges cover more space.
        // Reconstruct the frustum with Vulkan's forward depth [0,1], fit each slice
        // in light space, and return orthographic matrices plus camera-depth splits.
        // The practical split mixes logarithmic (near detail) and uniform spacing
        // (far coverage). Overlap supports PBR's cross-fade at cascade boundaries.
        // Quantized radius and light-space texel snapping keep camera translation
        // from sliding the shadow sample grid across stationary surfaces. Depth
        // padding includes upstream casters outside the visible camera slice.
        auto BuildDirectionalCascades(
            const core::float4x4 &inverseViewProjection,
            core::f32 nearPlane, core::f32 farPlane,
            core::float3 direction, core::u32 resolution,
            core::f32 distance = 200.0f ) -> DirectionalCascadeData;

        auto UploadShadowMaps( CommandContext& ctx, core::Blackboard& b ) -> void;
        auto RenderShadowMaps( CommandContext& ctx, core::Blackboard& b ) -> void;

    private:

        eastl::vector<DrawIndirectCommand> mShadowCommands{};
        eastl::vector<View> mViews{};

        eastl::array<ShadowMapParameters, kMaxShadowRecords> mRecords{};
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        GeometryCullModule* mGeometryManagement{};

        core::u32 mDirectionalSize{ 1024 };
        core::u32 mDirectionalCascadeCount{ kDirectionalCascades };
        core::u32 mSpotSize{ 1024 };
        core::u32 mPointSize{ 512 };
        core::f32 mCascadesDistance{ 200 };

        bool mSoftShadowsEnabled{ true };
        bool mRegistered{};
        ShadowDiagnostics mDiagnostics{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_SHADOW_MAPPING_MODULE_HH
