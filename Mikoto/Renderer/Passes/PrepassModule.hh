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
#include <Core/String.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/GeometryCullModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

namespace mikoto::renderer {

    inline constexpr core::u32 kMaxLightPerCluster{ 256 };

    struct ClusteredShadingParams {
        core::float4 mGridSize{};
        core::u32 mActiveLightCount{};
    };

    struct ClusterParameters {
        core::float4 mCenter{};
        core::float4 mClosestPoint{};
        core::float4 mDistanceSquared{};

        core::float4 mMinPoint{};
        core::float4 mMaxPoint{};

        core::u32 mCount{};
        core::u32 mPadding[3]{};
        core::u32 mLightIndices[kMaxLightPerCluster]{};
    };

    struct alignas(16) LightParameters {
        core::float4 mPosition{};
        core::float4 mDirection{};
        core::float4 mDiffuse{};

        core::f32 mCutOff{};
        core::f32 mOuterCutOff{};

        core::f32 mIntensity{};
        core::f32 mRadius{};

        core::i32 mActiveLightType{};

        core::f32 mPadding[3]{};
    };

    struct PrepassModuleInfo {
        // Depth prepass
        FGTextureHandle mDepthPrepassColorTarget{};
        FGTextureHandle mPrepassDepthTarget{};
        FGPipelineHandle mDepthPrepassPipeline{};

        // GBuffer
        FGTextureHandle mGBufferPositionTarget{};
        FGTextureHandle mGBufferNormalTarget{};
        FGTextureHandle mGBufferColorTarget{};
        FGTextureHandle mGBufferEmissiveTarget{};
        FGPipelineHandle mGBufferPipeline{};

        // Light culling
        core::u32 mClusterCount{};
        FGBufferHandle mClusterBuffer{};
        FGBufferHandle mLightsBuffer{};
        FGPipelineHandle mAabbGenPipeline{};
        FGPipelineHandle mLightCullingPipeline{};

        core::u32 mActiveLightCount{};

        core::float4 mGridSize{};
    };

    class PrepassModule {
    public:
        explicit PrepassModule( RenderResolution resolution );

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        auto SetGeometryManager( GeometryCullModule& geom ) -> void;

        auto RegisterPasses( FrameGraph& graph ) -> void;

    private:
        auto RegisterAabb( FrameGraph& graph ) -> void;
        auto RegisterGBuffer( FrameGraph& graph ) -> void;
        auto RegisterDepthPrepass( FrameGraph& graph ) -> void;
        auto RegisterLightCulling( FrameGraph& graph ) -> void;

        auto SetupLightList( CommandContext& ctx, FGBufferHandle lightBuffer ) -> void;

    private:
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        core::u32 mActiveLights{};
        core::u32 mLocalSize{ 128 };
        core::u32 mGridSizeX{ 12 };
        core::u32 mGridSizeY{ 12 };
        core::u32 mGridSizeZ{ 24 };
        core::u32 mNumClusters{ mGridSizeX * mGridSizeY * mGridSizeZ };

        GeometryCullModule* mGeometryManagement{};

        eastl::vector<LightParameters> mLights{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_CLUSTERED_SHADING_HH
