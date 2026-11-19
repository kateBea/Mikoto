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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_PARTICLE_SIMULATION_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_PARTICLE_SIMULATION_MODULE_HH

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::renderer {

    struct GpuParticleSimulationModuleInfo {
        FGBufferHandle mParticleStates{};
        FGBufferHandle mEmitterStates{};
        FGBufferHandle mEmitterParameters{};
        FGBufferHandle mOutput{};

        FGPipelineHandle mSimulationPipeline{};
        FGPipelineHandle mSortPipeline{};
    };

    struct ParticleSimulationInfo {
        FGBufferHandle mParticlesBuffer{};
        FGPipelineHandle mGraphicsPipeline{};
    };

    
    struct EmitterParameters {
        core::float4x4 mWorld{};
        core::float4 mDirectionCone{};
        core::float4 mAccelerationDrag{};
        core::float4 mLifetimeSpeed{};
        core::float4 mEmission{};
        core::float4 mSizeRotationSoftness{};
        core::float4 mStartColor{};
        core::float4 mEndColor{};

        core::u32 mOffset{};
        core::u32 mCapacity{};
        core::u32 mSteps{};
        core::u32 mReset{};
        core::u32 mSeed{};
        core::u32 mBurst{};
        core::u32 mEmitting{};
        core::u32 mVisible{};
        core::u32 mLooping{};
        core::u32 mInitialize{};
        core::u32 mPadding0{};
        core::u32 mPadding1{};
    };

    struct EmitterSlot {
        core::u64 mGuid{};
        core::u64 mRevision{};
        core::u64 mTick{};
        ParticleEmitterHandle mRecipe{};
        core::u32 mOffset{};
        core::u32 mCapacity{};
        bool mSeen{};
    };

    /**
     * @brief Registers GPU particle simulation and sorted billboard snapshot passes.
     * @details Consumes scene-owned fixed-step clocks, never wall time. Separate views
     * maintain independent GPU mirrors so drawing an extra camera cannot advance the scene.
     * A new graph restarts its mirror. All buffers and pass callbacks share the graph lifetime.
     */
    class GpuParticleSimulationModule final {
    public:
        GpuParticleSimulationModule() = default;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;

        auto RegisterSortPass( FrameGraph& graph ) -> void;
        auto RegisterSimulationPass( FrameGraph& graph ) -> void;
        auto RegisterSnapshotClearPass( FrameGraph& graph ) -> void;
        auto RegisterParameterUploadPass( FrameGraph& graph ) -> void;

        MKT_NODISCARD auto GetDrawCount() const -> core::u32;

    private:
        auto Gather() -> void;

    private:
        static constexpr core::u32 kMaxGpuEmitters{ 256 };

        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        // Persistent emitter slots and
        // their per-frame upload parameters.
        core::u32 mDrawCount{};
        eastl::array<EmitterSlot, kMaxGpuEmitters> mSlots{};
        eastl::array<EmitterParameters, kMaxGpuEmitters> mParameters{};
    };

    class ParticleSimulationModule final {
    public:
        explicit ParticleSimulationModule( rhi::RenderResolution resolution );

        auto RegisterUploadPass( FrameGraph& graph ) -> void;
        auto RegisterRenderPass( FrameGraph& graph ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetSimulationMode( ParticleSimulationMode mode ) -> void;

        MKT_NODISCARD auto GetVisibleCount() const -> core::u32;
        MKT_NODISCARD auto GetDroppedCount() const -> core::u32;
        MKT_NODISCARD auto GetGpuSimulation() -> GpuParticleSimulationModule*;
        MKT_NODISCARD auto GetSimulationMode() const -> ParticleSimulationMode;

    private:
        auto Gather() -> void;

    private:
        // View configuration shared by either simulation path.
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        // CPU billboard snapshot and its overflow count.
        eastl::vector<ParticleRenderData> mRenderData{};
        core::u32 mDroppedCount{};

        // GPU state is allocated only when the GPU path is selected.
        ParticleSimulationMode mSimulationMode{ ParticleSimulationMode::eCpu };
        eastl::unique_ptr<GpuParticleSimulationModule> mGpuSimulation{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_PARTICLE_SIMULATION_MODULE_HH

