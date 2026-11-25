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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_INDIRECT_LIGHTING_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_INDIRECT_LIGHTING_MODULE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    struct ScreenSpaceReflectionSettings {
        bool mEnabled{};
        bool mHalfResolution{ true };

        core::u32 mMaxSteps{ 96 };
        core::u32 mSpatialSamples{ 8 };

        core::f32 mMaxDistance{ 80.0f }; // Metres along the reflected ray.
        core::f32 mThickness{ 0.15f }; // View-space acceptance band, in metres.
        core::f32 mMaxRoughness{ 0.85f };
        core::f32 mHistoryWeight{ 0.9f };
        core::f32 mSamplingBias{ 0.3f };
        core::f32 mIntensity{ 1.0f };

        auto operator==( const ScreenSpaceReflectionSettings& other ) const -> bool = default;
    };

    struct IndirectLightingModuleInfo {
        // PBR writes its actual integrated specular response and environment lobe.
        // Composition replaces that lobe only where the screen-space hit is valid.
        FGTextureHandle mSurfaceResponse{};
        FGTextureHandle mEnvironmentSpecular{};
        FGTextureHandle mRasterSurfaceResponse{};
        FGTextureHandle mRasterEnvironmentSpecular{};
    };

    /** Owns stochastic reflection tracing, spatial reconstruction and per-view temporal history. */
    class IndirectLightingModule final {
    public:
        explicit IndirectLightingModule( rhi::RenderResolution resolution );

        auto SetSettings( const ScreenSpaceReflectionSettings& settings ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        /** @brief Allocates the auxiliary PBR targets before opaque shading is registered. */
        auto RegisterSurfaceTargets( FrameGraph& graph, rhi::Multisampling samples ) -> void;

        /** @brief Registers the HDR reflection chain after the final opaque/MSAA scene resolve. */
        auto RegisterPasses( FrameGraph& graph, rhi::Multisampling samples ) -> void;

        /** @brief Selects this view's scene and invalidates history on cuts, lens changes or RT replacement. */
        auto Prepare( const scene::Scene* scene, const scene::Camera* camera, bool rayTracedReflections ) -> void;

        auto ResetHistory() -> void;

    private:
        const scene::Camera* mCamera{};
        const scene::Scene* mHistoryScene{};
        const scene::Camera* mHistoryCamera{};

        ScreenSpaceReflectionSettings mSettings{};
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };

        core::float4x4 mPreviousViewProjection{ 1.0f };
        core::float4x4 mPreviousView{ 1.0f };
        core::float4x4 mPreviousProjection{ 1.0f };
        core::float3 mPreviousCameraPosition{};

        core::u32 mFrameIndex{};
        bool mCanRenderReflections{};
        bool mReflectionHistoryValid{};
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_INDIRECT_LIGHTING_MODULE_HH
