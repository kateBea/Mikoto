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

#ifndef MIKOTO_SHADOW_MAPPING_PASS_HH
#define MIKOTO_SHADOW_MAPPING_PASS_HH

#include <EASTL/fixed_vector.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    inline constexpr u32 kMaxShadowMaps{ 5 };

    struct ShadowMapInfo {

        // Contains light space info, etc
        FGBufferHandle mShadowsBuffer{};

        FGSamplerHandle mDirShadowSampler{};

        FGPipelineHandle mDirShadowMapPipeline{};
        FGPipelineHandle mSpotShadowMapPipeline{};
        FGPipelineHandle mPointShadowMapPipeline{};

        eastl::fixed_vector<FGTextureHandle, kMaxShadowMaps> mDirShadowMaps{};
        eastl::fixed_vector<FGTextureHandle, kMaxShadowMaps> mSpotShadowMaps{};
        eastl::fixed_vector<FGTextureHandle, kMaxShadowMaps> mPointShadowMaps{};

        RenderResolution mShadowMapsReso{ RenderResolution::e3120P };
    };

    struct ShadowMapParameters {
        // View in light space
        float4x4 mView{};

        // Perspective in light space for point and spot
        // and ortho for directional, although it
        // can also be the latter for directional lights
        float4x4 mProjection{};

        i32 mLightType{ -1 }; // Dir, spot, point
    };

    class ShadowMappingModule final {
    public:
        explicit ShadowMappingModule(RenderResolution resolution);

        auto SetScene( const scene::Scene* scene) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetGeometryManager( GeometryCullModule& culling ) -> void;

        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        auto RegisterDirShadowMap( FrameGraph& graph ) -> void;
        auto RegisterPointShadowMap( FrameGraph& graph ) -> void;
        auto RegisterSpotShadowMap( FrameGraph& graph ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};
        GeometryCullModule* mGeometryManager{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };
}


#endif // MIKOTO_SHADOW_MAPPING_PASS_HH
