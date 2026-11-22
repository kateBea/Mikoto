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
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::renderer {

    struct ParticleSimulationInfo {
        FGBufferHandle mParticlesBuffer{};
        FGPipelineHandle mGraphicsPipeline{};
        FGSamplerHandle mSampler{};

        FGBufferHandle mParticleStates{};
        FGBufferHandle mEmitterStates{};
        FGBufferHandle mEmitterParameters{};
        FGBufferHandle mParticleIndices{};
        FGBufferHandle mTextureDescriptors{};

        FGPipelineHandle mSimulationPipeline{};
        FGPipelineHandle mSortPipeline{};
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
        core::u32 mMaxParticles{};
        core::u32 mPreviousCapacity{};
        core::u32 mTextureIndex{ kInvalidParticleTextureIndex };
        core::u32 mTextureOffset{};
        core::f32 mFadeIn{};
        core::f32 mFadeOut{};
    };

    struct EmitterSlot {
        struct TextureBinding {
            rhi::TextureHandle mTexture{};
            core::u64 mExpiresAtTick{};
        };

        core::u64 mInstanceID{};
        core::u64 mRevision{};
        core::u64 mTick{};
        core::u64 mSimulatedTicks{};
        eastl::vector<core::u32> mIndices{};
        eastl::vector<TextureBinding> mTextures{};
        bool mSeen{};
    };

    class ParticleSimulationModule final {
    public:
        explicit ParticleSimulationModule( rhi::RenderResolution resolution );

        auto RegisterSimulationPasses( FrameGraph& graph ) -> void;
        auto RegisterRenderPass( FrameGraph& graph ) -> void;

        auto SetScene( const scene::Scene* scene ) -> void;
        auto SetCamera( const scene::Camera* camera ) -> void;
        auto SetSimulationMode( ParticleSimulationMode mode ) -> void;

        MKT_NODISCARD auto GetVisibleCount() const -> core::u32;
        MKT_NODISCARD auto GetDroppedCount() const -> core::u32;
        MKT_NODISCARD auto GetSimulationMode() const -> ParticleSimulationMode;

    private:
        auto RegisterUploadPass( FrameGraph& graph ) -> void;
        auto RegisterParameterUploadPass( FrameGraph& graph ) -> void;
        auto RegisterSnapshotClearPass( FrameGraph& graph ) -> void;
        auto RegisterSimulationPass( FrameGraph& graph ) -> void;
        auto RegisterSortPass( FrameGraph& graph ) -> void;

        auto GatherCpuParticles( CommandContext& ctx ) -> void;
        auto GatherGpuEmitters( CommandContext& ctx ) -> void;

    private:
        // View configuration shared by either simulation path.
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        const scene::Scene* mScene{};
        const scene::Camera* mCamera{};

        // CPU billboard snapshot and its overflow count.
        eastl::vector<ParticleRenderData> mRenderData{};
        core::u32 mDroppedCount{};

        static constexpr core::u32 kMaxGpuEmitters{ 256 };
        static constexpr core::u32 kMaxGpuTextureBindings{ kMaxRenderedParticles + kMaxGpuEmitters };

        ParticleSimulationMode mSimulationMode{ ParticleSimulationMode::eCpu };
        core::u32 mGpuDrawCount{};
        eastl::array<EmitterSlot, kMaxGpuEmitters> mSlots{};
        eastl::array<EmitterParameters, kMaxGpuEmitters> mParameters{};
        eastl::vector<core::u32> mParticleIndices{};
        eastl::vector<core::u32> mTextureDescriptors{};

        struct SortStage {
            eastl::string mName{};
            core::u32 mRequiredCount{};
        };

        eastl::vector<SortStage> mSortStages{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_PARTICLE_SIMULATION_MODULE_HH

