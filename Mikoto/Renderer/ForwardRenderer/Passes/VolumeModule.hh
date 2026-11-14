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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_VOLUME_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_VOLUME_MODULE_HH

#include <EASTL/vector.h>

#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::scene {
    class Scene;
    class Camera;
}// namespace mikoto::scene

namespace mikoto::renderer {

    // Shared medium coefficients and analytic boundaries for both render paths.
    struct VolumeRecord {
        core::float4x4 mWorldToLocal{ 1.0f };
        core::float4 mAbsorptionDensity{};
        core::float4 mScatteringPhase{};
        core::float4 mDimensionsHeight{};
        core::float4 mNoise{};

        core::uint4 mFlags{};// sphere, density model, cast shadows, receive shadows
        core::float4 mWindOctaves{};
        core::uint4 mGrid{};               // dimensions, byte offset
        core::float4 mExtinctionMajorant{};// Base RGB/density bounds; height varies per ray segment.
    };

    struct VolumeModuleInfo {
        FGBufferHandle mData{};
    };

    // A tiled 2D atlas stores the camera-aligned volume, like the atmosphere LUTs.
    struct FogModuleInfo {
        FGBufferHandle mFrame{};
        FGTextureHandle mLighting{};
        FGTextureHandle mExtinction{};
        FGTextureHandle mHistory{};
        FGTextureHandle mExtinctionHistory{};
        FGTextureHandle mRadiance{};
        FGTextureHandle mTransmittance{};
        FGTextureHandle mSceneCopy{};
        FGSamplerHandle mSampler{};
    };

    struct FogFrameData {
        core::float4x4 mPreviousViewProjection{ 1.0f };
        core::float4x4 mPreviousInverseViewProjection{ 1.0f };
        core::float4 mPreviousCamera{};
        core::uint4 mGrid{};    // XY cells, depth slices, tiles per row
        core::float4 mRange{};  // near distance, far distance, previous far, history weight
        core::uint4 mFrame{};   // jitter frame, previous projection is orthographic
        core::uint4 mTextures{};// integrated radiance, transmission, sampler, volume count
    };

    class VolumeModule final {
    public:
        explicit VolumeModule( rhi::RenderResolution resolution );

        auto RegisterUpload( FrameGraph &graph ) -> void;
        auto RegisterFog( FrameGraph &graph ) -> void;
        auto RegisterFogMultisample( FrameGraph &graph, rhi::Multisampling samples ) -> void;

        // Returns true if density/shape/wind changed, invalidating path accumulation.
        auto Prepare( const scene::Scene *scene, const scene::Camera *camera, core::f32 ambientScale ) -> bool;

        MKT_NODISCARD auto GetData() const -> const eastl::vector<core::ubyte> &;

    private:
        auto RegisterFogLighting( FrameGraph &graph ) -> void;
        auto RegisterFogIntegration( FrameGraph &graph ) -> void;
        auto RegisterFogComposite( FrameGraph &graph ) -> void;

    private:
        rhi::RenderResolution mResolution{};
        const scene::Scene *mScene{};
        const scene::Camera *mCamera{};
        FogFrameData mFogFrame{};

        eastl::vector<core::ubyte> mData{};
        core::u64 mHash{};
        core::u32 mCount{};

        bool mUploadDirty{ true };
        bool mHistoryValid{};
        core::f32 mAmbientScale{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_VOLUME_MODULE_HH
