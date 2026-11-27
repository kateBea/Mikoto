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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_PREPASS_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_PREPASS_MODULE_HH

#include <EASTL/optional.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryCullModule.hh>
#include <Renderer/Rhi/Types.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

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

    struct alignas( 16 ) LightParameters {
        core::float4 mPosition{};
        core::float4 mDirection{};// w marks the directional light used as the atmospheric sun.
        core::float4 mDiffuse{};

        core::f32 mCutOff{};
        core::f32 mOuterCutOff{};

        core::f32 mIntensity{};
        core::f32 mRadius{};

        core::i32 mActiveLightType{};

        core::i32 mShadowIndex{ -1 };
        core::u32 mShadowSlot{};
        core::f32 mPadding{};

        // Area basis vectors; w stores half width/length and half height, respectively.
        core::float4 mAreaAxisX{};
        core::float4 mAreaAxisY{};
        // Bindless texture/sampler, manual sRGB decode, source mip count.
        glm::uvec4 mAreaEmission{ 0xffffffffu, 0xffffffffu, 0, 0 };
        core::float4 mAreaEmissionUv{ 1, 1, 0, 0 };
    };

    /** Parameters for the normalized sum of successive Perlin noise octaves. */
    struct FractalBrownianMotionParameters {
        core::u32 mOctaves{ 7 };         // Number of noise layers, [1, 12].
        core::f32 mPersistence{ 0.5f };  // Amplitude multiplier per octave, [0, 1].
        core::f32 mLacunarity{ 2.7f };   // Frequency multiplier per octave, [1, 4].
        core::f32 mBaseFrequency{ 1.0f };// Initial lattice cells across the texture, (0, 64].
    };

    struct PrepassModuleInfo {
        FGTextureHandle mPerlinNoiseTarget{};
        FGTextureHandle mFractalBrownianMotionTarget{};

        const LightParameters *mCpuLights{};
        core::u32 mShadowRecordCount{};
        // Depth prepass
        FGTextureHandle mDepthPrepassColorTarget{};
        FGTextureHandle mPrepassDepthTarget{};
        FGPipelineHandle mDepthPrepassPipeline{};

        // GBuffer
        FGTextureHandle mGBufferPositionTarget{};
        FGTextureHandle mGBufferNormalTarget{};
        FGTextureHandle mGBufferContourTarget{};// RGB contour colour; alpha = width / 12.
        FGTextureHandle mGBufferColorTarget{};
        FGTextureHandle mGBufferEmissiveTarget{};
        FGPipelineHandle mGBufferPipeline{};

        // Light culling
        core::u32 mClusterCount{};
        FGBufferHandle mClusterBuffer{};

        FGSamplerHandle mAreaEmissionSampler{};
        core::u32 mLightsBufferCount{};
        FGBufferHandle mLightsBuffer{};

        FGPipelineHandle mAabbGenPipeline{};
        FGPipelineHandle mLightCullingPipeline{};

        core::u32 mActiveLightCount{};

        core::float4 mGridSize{};
    };

    class PrepassModule {
    public:
        explicit PrepassModule( renderer::rhi::RenderResolution resolution );

        auto SetScene( const scene::Scene *scene ) -> void;
        auto SetCamera( const scene::Camera *camera ) -> void;

        auto SetGeometryManager( GeometryCullModule &geom ) -> void;

        auto RegisterAabb( FrameGraph &graph ) -> void;
        auto RegisterLightCulling( FrameGraph &graph ) -> void;
        auto RegisterGBuffer( FrameGraph &graph ) -> void;
        // Editor selection/overlays also need depth when lighting is path traced.
        auto RegisterDepthPrepass( FrameGraph &graph ) -> void;

        // https://rtouti.github.io/graphics/perlin-noise-algorithm
        // Each registers a 1024x1024 fullscreen draw and readback, once per graph.
        // Linear grayscale PNGs are saved beneath Resources/Noise in the working directory.
        auto RegisterPerlinNoise( FrameGraph &graph ) -> void;
        auto RegisterFractalBrownianMotion( FrameGraph &graph ) -> void;

        /** Configure before RegisterFractalBrownianMotion. Registration snapshots these values.
         * Rebuild the graph to generate again; neither generation nor export runs every frame.
         * Throws for values outside the documented ranges.
         */
        auto SetFractalBrownianMotionParameters( const FractalBrownianMotionParameters &parameters ) -> void;
        MKT_NODISCARD auto GetFractalBrownianMotionParameters() const -> FractalBrownianMotionParameters;

    private:
        auto SetupLightList( CommandContext &ctx, FGBufferHandle lightBuffer, FGSamplerHandle emissionSampler,
            eastl::optional<core::u32> sunLight ) -> void;

    private:
        const scene::Scene *mScene{};
        const scene::Camera *mCamera{};

        core::u32 mActiveLights{};
        core::u32 mLocalSize{ 128 };
        core::u32 mGridSizeX{ 12 };
        core::u32 mGridSizeY{ 12 };
        core::u32 mGridSizeZ{ 24 };
        core::u32 mNumClusters{ mGridSizeX * mGridSizeY * mGridSizeZ };

        GeometryCullModule *mGeometryManagement{};

        eastl::vector<LightParameters> mLights{};

        FractalBrownianMotionParameters mFractalBrownianMotionParameters{};

        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_PREPASS_MODULE_HH
