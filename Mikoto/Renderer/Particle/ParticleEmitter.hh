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

#include <Filesystem/Path.hh>

namespace mikoto::renderer {
    /** @brief Selects where particle state is advanced; both modes draw billboard quads. */
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
    };

    // Three float4s form the shared CPU/Slang instance-buffer ABI.
    struct ParticleRenderData {
        core::float4 mPositionSize{};
        core::float4 mColor{};
        core::float4 mRotationSoftness{};
    };

    inline constexpr core::u32 kMaxEmitterParticles{ 16384 };
    inline constexpr core::u32 kMaxRenderedParticles{ 65536 };

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
    };

    // Immutable, reusable recipe. Editing creates a new recipe; live simulation
    // is kept separately so two entities sharing this resource remain independent.
    /**
     * @brief Clamps particle settings and replaces invalid values.
     * @param settings Settings describing the requested operation.
     * @returns Settings safe for the supported simulation paths.
     */
    auto SanitizeParticleSettings( ParticleEmitterSettings settings ) -> ParticleEmitterSettings;

    class ParticleEmitter final : public core::Resource {
    public:
        explicit ParticleEmitter( ParticleEmitterSettings settings = {} );
        ~ParticleEmitter() override;
        auto GetSettings() const -> const ParticleEmitterSettings&;
        auto Save( const filesystem::Path& path ) const -> void;
        static auto Load( const filesystem::Path& path ) -> core::Ref<ParticleEmitter>;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        ParticleEmitterSettings mSettings{};
    };

    using ParticleEmitterHandle = core::Ref<ParticleEmitter>;

    enum class ParticlePlayback {
        ePlaying,
        ePaused,
        eStopped
    };

    class ParticleEmitterInstance final {
    public:
        ParticleEmitterInstance();

        ParticleEmitterInstance( const ParticleEmitterInstance& other );
        auto operator=( const ParticleEmitterInstance& other ) -> ParticleEmitterInstance&;

        ParticleEmitterInstance( ParticleEmitterInstance&& ) = default;
        auto operator=( ParticleEmitterInstance&& ) -> ParticleEmitterInstance& = default;

        auto GetEmitter() const -> const ParticleEmitterHandle&;
        auto SetEmitter( ParticleEmitterHandle emitter ) -> void;
        auto GetParticles() const -> const eastl::vector<Particle>&;
        auto GetPlayback() const -> ParticlePlayback;
        auto GetDroppedParticles() const -> core::u64;
        auto Play() -> void;
        auto Pause() -> void;
        auto Stop( bool clear = true ) -> void;
        auto Restart() -> void;

        /** @brief Restarts state when changing backend; no synchronous GPU readback is performed. */
        auto SetSimulationMode( ParticleSimulationMode mode ) -> void;

        MKT_NODISCARD auto GetSimulationMode() const -> ParticleSimulationMode;
        /** @brief Scene-owned fixed-step clock. Rendering the scene again does not advance it. */
        MKT_NODISCARD auto GetSimulationTick() const -> core::u64;
        MKT_NODISCARD auto GetRestartRevision() const -> core::u64;
        MKT_NODISCARD auto IsEmitting() const -> bool;
        auto Update( core::f32 timeStep, const core::float4x4& world, core::u64 entitySeed ) -> void;

    private:
        auto Random() -> core::f32;
        auto Step( core::f32 timeStep, const core::float4x4& world ) -> void;
        auto Spawn( const core::float4x4& world ) -> void;
        ParticleEmitterHandle mEmitter{};
        eastl::vector<Particle> mParticles{};
        ParticlePlayback mPlayback{ ParticlePlayback::ePlaying };
        core::f64 mTimeAccumulator{};
        core::f64 mEmissionAccumulator{};
        core::f32 mElapsed{};
        core::u32 mRandomState{ 1 };
        core::u64 mDroppedParticles{};
        bool mStarted{};
        bool mEmitting{ true };
        ParticleSimulationMode mSimulationMode{ ParticleSimulationMode::eCpu };
        core::u64 mSimulationTick{};
        core::u64 mRestartRevision{ 1 };
        core::f32 mDrainTime{};
    };

    auto SerializeParticleSettings( const ParticleEmitterSettings& settings, YAML::Emitter& out ) -> void;
    auto DeserializeParticleSettings( const YAML::Node& node ) -> ParticleEmitterSettings;
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_PARTICLE_PARTICLE_EMITTER_HH
