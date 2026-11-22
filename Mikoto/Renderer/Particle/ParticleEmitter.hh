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

#ifndef MIKOTO_RENDERER_PARTICLE_PARTICLE_EMITTER_HH
#define MIKOTO_RENDERER_PARTICLE_PARTICLE_EMITTER_HH

#include <EASTL/vector.h>

#include <yaml-cpp/yaml.h>

#include <Core/Types.hh>
#include <Core/Resource.hh>

#include <Material/ParticleMaterial.hh>

namespace mikoto::renderer {

    // https://alextardif.com/Particles.html
    
    // https://gdcvault.com/play/1025132/
    // https://www.gdcvault.com/play/1020002/Advanced-Visual-Effects-with-DirectX
    
    // https://wickedengine.net/category/devblog/2017/11/07/gpu-based-particle-simulation/index.html
    
    // https://github.com/GPUOpen-LibrariesAndSDKs/GPUParticles11/
    // https://gpuopen.com/learn/gpuparticles11-directx-11-sdk-sample/

    inline constexpr core::u32 kMaxEmitterParticles{ 16384 };
    inline constexpr core::u32 kMaxRenderedParticles{ 65536 };
    inline constexpr core::u32 kInvalidParticleTextureIndex{ 0xffffffffu };

    enum class ParticlePlayback {
        ePlaying,
        ePaused,
        eStopped
    };

    enum class ParticleSimulationMode : core::u8 {
        eCpu,
        eGpu
    };

    // Dense values owned by an emitter instance: no allocation or refcount per particle.
    struct Particle {
        core::float3 mPosition{};
        core::float3 mVelocity{};
        core::f32 mAge{};
        core::f32 mLifetime{ 1.0f };
        core::f32 mRotation{};

        // Motion and appearance are captured at spawn.
        core::float3 mAcceleration{};
        core::f32 mDrag{};
        core::f32 mAngularSpeed{};
        core::f32 mSoftness{};
        core::f32 mFadeIn{};
        core::f32 mFadeOut{};
        core::f32 mStartSize{};
        core::f32 mEndSize{};
        core::float4 mStartColor{};
        core::float4 mEndColor{};
        core::u32 mTextureIndex{ kInvalidParticleTextureIndex };

        MKT_NODISCARD auto GetColor() const -> core::float4;
    };

    // Four vectors form the shared CPU/Slang instance-buffer ABI.
    struct ParticleRenderData {
        core::float4 mPositionSize{};
        core::float4 mColor{};
        core::float4 mRotationSoftness{};
        glm::uvec4 mTexture{ kInvalidParticleTextureIndex, 0, 0, 0 };
    };

    struct ParticleEmitterSettings {
        core::u32 mMaxParticles{ 2048 };
        core::u32 mBurstCount{};
        core::u32 mSeed{ 1 };
        core::f32 mEmissionRate{ 100.0f };
        core::f32 mLifetime{ 2.0f };
        core::f32 mLifetimeVariation{ 0.25f };
        core::f32 mSpeed{ 2.0f };
        core::f32 mSpeedVariation{ 0.5f };
        core::f32 mRadius{ 0.15f };
        core::f32 mConeAngle{ 20.0f };
        core::float3 mDirection{ 0.0f, 1.0f, 0.0f };
        core::float3 mAcceleration{ 0.0f, -0.5f, 0.0f };
        core::f32 mDrag{ 0.1f };
        core::f32 mDuration{ 5.0f };
        core::f32 mStartSize{ 0.15f };
        core::f32 mEndSize{ 0.45f };
        core::float4 mStartColor{ 1.0f, 0.45f, 0.08f, 0.8f };
        core::float4 mEndColor{ 0.25f, 0.08f, 0.02f, 0.0f };
        core::f32 mAngularSpeed{ 0.5f };
        core::f32 mSoftness{ 0.8f };
        bool mLooping{ true };

        bool mFadeEnabled{ true };
        core::f32 mFadeIn{ 0.05f }; // Fraction of particle lifetime.
        core::f32 mFadeOut{ 0.25f };

        auto Sanitize() -> void;
        auto Serialize( YAML::Emitter& out ) const -> void;
        MKT_NODISCARD static auto Deserialize( const YAML::Node& node ) -> ParticleEmitterSettings;
    };

    struct ParticleEmitterCreateDescription {
        ParticleEmitterSettings mSettings{};
        material::ParticleMaterialHandle mMaterial{};
    };

    class ParticleEmitter final : public core::Resource {
    public:
        explicit ParticleEmitter( const ParticleEmitterCreateDescription& description = {} );

        MKT_NODISCARD auto GetSettings() const -> const ParticleEmitterSettings&;
        MKT_NODISCARD auto GetMaterial() const -> const material::ParticleMaterialHandle&;
        MKT_NODISCARD auto GetTint() const -> core::float4;
        MKT_NODISCARD auto GetTexture() const -> rhi::TextureHandle;

        ~ParticleEmitter() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        ParticleEmitterSettings mSettings{};
        material::ParticleMaterialHandle mMaterial{};
    };

    using ParticleEmitterHandle = core::Ref<ParticleEmitter>;

    class ParticleEmitterInstance final {
    public:
        explicit ParticleEmitterInstance( ParticleEmitterHandle emitter );

        ParticleEmitterInstance( const ParticleEmitterInstance& other );
        ParticleEmitterInstance( ParticleEmitterInstance&& ) noexcept = default;

        auto operator=( const ParticleEmitterInstance& other ) -> ParticleEmitterInstance&;
        auto operator=( ParticleEmitterInstance&& ) noexcept -> ParticleEmitterInstance& = default;

        auto Play() -> void;
        auto Pause() -> void;
        auto Restart() -> void;
        auto Stop( bool clear = true ) -> void;
        // Only future spawns use the replacement recipe.
        auto SetEmitter( ParticleEmitterHandle emitter ) -> void;
        auto SetSimulationMode( ParticleSimulationMode mode ) -> void;
        auto Update( core::f32 timeStep, const core::float4x4& world, core::u64 entitySeed ) -> void;

        MKT_NODISCARD auto IsEmitting() const -> bool;
        MKT_NODISCARD auto GetID() const -> core::u64;
        MKT_NODISCARD auto GetSimulationTick() const -> core::u64;
        MKT_NODISCARD auto GetRestartRevision() const -> core::u64;
        MKT_NODISCARD auto GetSimulationMode() const -> ParticleSimulationMode;

        MKT_NODISCARD auto GetPlayback() const -> ParticlePlayback;
        MKT_NODISCARD auto GetDroppedParticles() const -> core::u64;
        MKT_NODISCARD auto GetEmitter() const -> const ParticleEmitterHandle &;
        MKT_NODISCARD auto GetParticles() const -> const eastl::vector<Particle> &;
        MKT_NODISCARD auto GetParticleTexture( core::u32 index ) const -> rhi::TextureHandle;

    private:
        MKT_NODISCARD static auto AllocateID() -> core::u64;
        auto Random() -> core::f32;
        auto BindSpawnTexture() -> void;
        auto Spawn( const core::float4x4& world ) -> void;
        auto Step( core::f32 timeStep, const core::float4x4& world ) -> void;

    private:
        struct TextureSlot {
            rhi::TextureHandle mTexture{};
            core::u32 mLiveCount{};
        };

        eastl::vector<Particle> mParticles{};
        eastl::vector<TextureSlot> mTextureSlots{};
        core::u32 mSpawnTextureIndex{ kInvalidParticleTextureIndex };

        core::u64 mID{ AllocateID() };
        ParticleEmitterHandle mEmitter{};
        ParticlePlayback mPlayback{ ParticlePlayback::ePlaying };

        core::f32 mElapsed{ 0.0f };
        core::u32 mRandomState{ 1 };
        core::u32 mAppliedSeed{};
        core::u64 mDroppedParticles{ 0 };
        core::f64 mTimeAccumulator{ 0.0f };
        core::f64 mEmissionAccumulator{ 0.0f };

        core::f32 mDrainTime{};
        core::f32 mLongestParticleLifetime{};
        core::u64 mSimulationTick{};
        core::u64 mRestartRevision{ 1 };

        ParticleSimulationMode mSimulationMode{ ParticleSimulationMode::eCpu };

        bool mStarted{ false };
        bool mEmitting{ true };
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_PARTICLE_PARTICLE_EMITTER_HH
