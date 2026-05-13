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

#ifndef MIKOTO_CLUSTERED_SHADING_HH
#define MIKOTO_CLUSTERED_SHADING_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Passes/GeometryCullModule.hh>
#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    inline constexpr u32 kMaxLightPerCluster{ 256 };

    struct ClusteredShadingParams {
        float4 mGridSize{};
        u32 mActiveLightCount{};
    };

    struct ClusterParameters  {
        float4 Center{};
        float4 ClosestPoint{};
        float4 DistanceSquared{};

        float4 MinPoint{};
        float4 MaxPoint{};

        u32 Count{};
        u32 LightIndices[kMaxLightPerCluster]{};
    };

    struct LightParameters {
        float4 mPosition{};
        float4 mDirection{};
        float4 mDiffuse{};

        f32 mCutOff{};
        f32 mOuterCutOff{};

        f32 mIntensity{};
        f32 mRadius{};

        i32 mActiveLightType{};
    };

    struct PrepassModuleInfo {
        // Depth prepass
        FGTextureHandle mDepthPrepassColorTarget{};
        FGTextureHandle mDepthPrepassDepthTarget{};
        FGPipelineHandle mDepthPrepassPipeline{};

        // GBuffer
        FGTextureHandle mGBufferPositionTarget{};
        FGTextureHandle mGBufferNormalTarget{};
        FGTextureHandle mGBufferColorTarget{};
        FGTextureHandle mGBufferEmissiveTarget{};
        FGPipelineHandle mGBufferPipeline{};

        // Light culling
        FGBufferHandle mClusterBuffer{};
        FGBufferHandle mLightCullingBuffer{};

        FGPipelineHandle mAabbGenPipeline{};
        FGPipelineHandle mLightCullingPipeline{};

        u32 mActiveLightCount{};

        float4 mGridSize{};
    };

    class PrepassModule {
    public:
        explicit PrepassModule(RenderResolution resolution);

        auto SetScene(const scene::Scene* scene) -> void;
        auto SetCamera(const scene::Camera* camera) -> void;

        auto SetGeometryManager(GeometryCullModule& geom) -> void;

        auto RegisterPasses(FrameGraph &graph) -> void;

    private:
        auto RegisterAABB( FrameGraph& graph ) -> void;
        auto RegisterGBuffer( FrameGraph& graph ) -> void;
        auto RegisterDepthPrepass( FrameGraph& graph ) -> void;
        auto RegisterLightCulling( FrameGraph& graph ) -> void;

        auto SetupLightList(CommandContext &ctx, FGBufferHandle lightBuffer) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        u32 mGridSizeX{ 12 };
        u32 mGridSizeY{ 12 };
        u32 mGridSizeZ{ 24 };
        u32 mNumClusters{ mGridSizeX * mGridSizeY * mGridSizeZ };
        u32 mLocalSize{ 128 }; // for light culling
        u32 mActiveLights{};

        GeometryCullModule* mGeometryManagement{};

        eastl::vector<LightParameters> mLights{};

        RenderResolution mResolution{ RenderResolution::e1080P };
    };
}


#endif//MIKOTO_CLUSTERED_SHADING_HH
