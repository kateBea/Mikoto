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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_WATER_SIMULATION_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_WATER_SIMULATION_MODULE_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Rhi/Types.hh>

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    // https://alextardif.com/Water.html
    // https://alextardif.com/Water.html
    // https://keobkeig.dev/tutorials/realistic-water-tutorial

    struct WaterSimulationModuleInfo {
        FGBufferHandle mVertices{};
        FGBufferHandle mIndices{};
        FGBufferHandle mWaves{};
        FGBufferHandle mSurfaces{};
        FGBufferHandle mCoast{};
        FGBufferHandle mCoastHistory{};
        FGBufferHandle mFftInitialSpectrum{};
        FGBufferHandle mFftSpectrum{};
        FGBufferHandle mFftRows{};
        FGBufferHandle mFftDisplacement{};
        FGBufferHandle mFftGradients{};
        FGPipelineHandle mFftTransformPipeline{};
        FGTextureHandle mSceneColor{};
        FGTextureHandle mSceneDepth{};
        FGSamplerHandle mSampler{};
        FGPipelineHandle mPipeline{};
    };

    class WaterSimulationModule final {
    public:
        explicit WaterSimulationModule( rhi::RenderResolution resolution );

        auto RegisterWaterSimulation( FrameGraph &graph, bool hdr = false ) -> void;

        auto SetCamera( const scene::Camera *camera ) -> void;
        auto SetScene( const scene::Scene *scene ) -> void;

    private:
        auto FilterWaterSimulators( CommandContext &ctx, core::Blackboard &blackboard ) -> void;
        auto RegisterUnderwaterPass( FrameGraph &graph ) -> void;
        auto RegisterCoastPass( FrameGraph &graph ) -> void;
        MKT_NODISCARD auto IsCameraUnderwater() const -> bool;

        auto RegisterFftSpectrumPass( FrameGraph &graph ) -> void;
        auto RegisterFftRowsPass( FrameGraph &graph ) -> void;
        auto RegisterFftColumnsPass( FrameGraph &graph ) -> void;
        auto RegisterFftTransformPass( FrameGraph &graph, core::cstr name, FGPipelineHandle pipeline,
            FGBufferHandle input, FGBufferHandle output, bool columns ) -> void;
        auto RegisterFftGradientsPass( FrameGraph &graph ) -> void;
        auto PrepareFftInputs( CommandContext &ctx, const WaterSimulationModuleInfo &info ) -> void;

    private:
        struct Plane {
            entt::entity mEntity{ entt::null };
            core::RevisionTracked::Revision mRevision{};
            core::u32 mFirstVertex{};
            core::u32 mFirstIndex{};
            core::u32 mIndexCount{};
            core::float2 mDimensions{};
            core::float2 mResolution{};
            core::float2 mCoastOrigin{};
            core::float4x4 mCoastWorld{ 1.0f };
            core::f64 mCoastTime{};
            bool mCoastInitialized{};
        };

    private:
        struct FftPatch {
            scene::WaterSimulationComponent::FftSettings mSettings{};
            core::f64 mTime{};
        };

        BufferUploadBatch mUploads{};
        eastl::vector<Plane> mPlanes{};

        eastl::vector<FftPatch> mFftPatches{};
        eastl::vector<scene::WaterSimulationComponent::FftSettings> mUploadedSpectra{};

        const scene::Scene *mScene{ nullptr };
        const scene::Camera *mCamera{ nullptr };
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_WATER_SIMULATION_MODULE_HH
