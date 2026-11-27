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

#include <atomic>
#include <cmath>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Exception.hh>
#include <Core/Profiler.hh>

#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::renderer {

    auto Particle::GetColor() const -> core::float4 {
        const auto age{ glm::clamp( mAge / mLifetime, 0.0f, 1.0f ) };
        auto color{ glm::mix( mStartColor, mEndColor, age ) };
        const auto fadeIn{ mFadeIn > 0.0f ? glm::smoothstep( 0.0f, mFadeIn, age ) : 1.0f };
        const auto fadeOut{ mFadeOut > 0.0f ? 1.0f - glm::smoothstep( 1.0f - mFadeOut, 1.0f, age ) : 1.0f };
        color.a *= fadeIn * fadeOut;
        return color;
    }

    auto ParticleEmitterSettings::Sanitize() -> void {
        auto &s{ *this };
        s.mMaxParticles = eastl::clamp( s.mMaxParticles, 1u, kMaxEmitterParticles );
        s.mBurstCount = eastl::min( s.mBurstCount, s.mMaxParticles );
        s.mEmissionRate = eastl::clamp( s.mEmissionRate, 0.0f, 100000.0f );
        s.mLifetime = eastl::clamp( s.mLifetime, 0.02f, 300.0f );
        s.mLifetimeVariation = eastl::clamp( s.mLifetimeVariation, 0.0f, 0.95f );
        s.mSpeed = eastl::clamp( s.mSpeed, 0.0f, 1000.0f );
        s.mSpeedVariation = eastl::clamp( s.mSpeedVariation, 0.0f, 1.0f );
        s.mRadius = eastl::clamp( s.mRadius, 0.0f, 1000.0f );
        s.mConeAngle = eastl::clamp( s.mConeAngle, 0.0f, 180.0f );

        for ( core::usize i{}; i < 3; ++i ) {
            s.mDirection[i] = eastl::clamp( s.mDirection[i], -1.0f, 1.0f );
            s.mAcceleration[i] = eastl::clamp( s.mAcceleration[i], -10000.0f, 10000.0f );
        }

        const auto length{ glm::length( s.mDirection ) };
        s.mDirection = length > 0.0001f ? s.mDirection / length : core::float3{ 0, 1, 0 };
        s.mDrag = eastl::clamp( s.mDrag, 0.0f, 100.0f );
        s.mDuration = eastl::clamp( s.mDuration, 0.02f, 3600.0f );
        s.mStartSize = eastl::clamp( s.mStartSize, 0.0f, 1000.0f );
        s.mEndSize = eastl::clamp( s.mEndSize, 0.0f, 1000.0f );

        for ( core::usize i{}; i < 4; ++i ) {
            s.mStartColor[i] = eastl::clamp( s.mStartColor[i], 0.0f, i == 3 ? 1.0f : 100.0f );
            s.mEndColor[i] = eastl::clamp( s.mEndColor[i], 0.0f, i == 3 ? 1.0f : 100.0f );
        }

        s.mAngularSpeed = eastl::clamp( s.mAngularSpeed, -100.0f, 100.0f );
        s.mSoftness = eastl::clamp( s.mSoftness, 0.01f, 1.0f );
        s.mFadeIn = eastl::clamp( s.mFadeIn, 0.0f, 1.0f );
        s.mFadeOut = eastl::clamp( s.mFadeOut, 0.0f, 1.0f );
    }

    auto ParticleEmitterSettings::Serialize( YAML::Emitter &out ) const -> void {
        auto s{ *this };
        s.Sanitize();
        out << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << 1;
        out << YAML::Key << "MaxParticles" << YAML::Value << s.mMaxParticles;
        out << YAML::Key << "BurstCount" << YAML::Value << s.mBurstCount;
        out << YAML::Key << "Seed" << YAML::Value << s.mSeed;
        out << YAML::Key << "EmissionRate" << YAML::Value << s.mEmissionRate;
        out << YAML::Key << "Lifetime" << YAML::Value << s.mLifetime;
        out << YAML::Key << "LifetimeVariation" << YAML::Value << s.mLifetimeVariation;
        out << YAML::Key << "Speed" << YAML::Value << s.mSpeed;
        out << YAML::Key << "SpeedVariation" << YAML::Value << s.mSpeedVariation;
        out << YAML::Key << "Radius" << YAML::Value << s.mRadius;
        out << YAML::Key << "ConeAngle" << YAML::Value << s.mConeAngle;
        out << YAML::Key << "Drag" << YAML::Value << s.mDrag;
        out << YAML::Key << "Duration" << YAML::Value << s.mDuration;
        out << YAML::Key << "StartSize" << YAML::Value << s.mStartSize;
        out << YAML::Key << "EndSize" << YAML::Value << s.mEndSize;
        out << YAML::Key << "AngularSpeed" << YAML::Value << s.mAngularSpeed;
        out << YAML::Key << "Softness" << YAML::Value << s.mSoftness;
        out << YAML::Key << "Looping" << YAML::Value << s.mLooping;
        out << YAML::Key << "FadeEnabled" << YAML::Value << s.mFadeEnabled;
        out << YAML::Key << "FadeIn" << YAML::Value << s.mFadeIn;
        out << YAML::Key << "FadeOut" << YAML::Value << s.mFadeOut;
        out << YAML::Key << "Direction" << YAML::Value << YAML::Flow << YAML::BeginSeq;

        for ( core::usize i{}; i < 3; ++i ) {
            out << s.mDirection[i];
        }

        out << YAML::EndSeq;
        out << YAML::Key << "Acceleration" << YAML::Value << YAML::Flow << YAML::BeginSeq;

        for ( core::usize i{}; i < 3; ++i ) {
            out << s.mAcceleration[i];
        }

        out << YAML::EndSeq;
        out << YAML::Key << "StartColor" << YAML::Value << YAML::Flow << YAML::BeginSeq;

        for ( core::usize i{}; i < 4; ++i ) {
            out << s.mStartColor[i];
        }

        out << YAML::EndSeq;
        out << YAML::Key << "EndColor" << YAML::Value << YAML::Flow << YAML::BeginSeq;

        for ( core::usize i{}; i < 4; ++i ) {
            out << s.mEndColor[i];
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    auto ParticleEmitterSettings::Deserialize( const YAML::Node &node ) -> ParticleEmitterSettings {
        if ( !node.IsMap() || node["Version"].as<core::u32>( 1 ) != 1 ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported particle emitter format." );
        }

        ParticleEmitterSettings s{};
        s.mFadeEnabled = node["FadeEnabled"].as<bool>( s.mFadeEnabled );
        s.mFadeIn = node["FadeIn"].as<core::f32>( s.mFadeIn );
        s.mFadeOut = node["FadeOut"].as<core::f32>( s.mFadeOut );

        if ( node["MaxParticles"] ) {
            s.mMaxParticles = node["MaxParticles"].as<core::u32>();
        }

        if ( node["BurstCount"] ) {
            s.mBurstCount = node["BurstCount"].as<core::u32>();
        }

        if ( node["Seed"] ) {
            s.mSeed = node["Seed"].as<core::u32>();
        }

        if ( node["EmissionRate"] ) {
            s.mEmissionRate = node["EmissionRate"].as<core::f32>();
        }

        if ( node["Lifetime"] ) {
            s.mLifetime = node["Lifetime"].as<core::f32>();
        }

        if ( node["LifetimeVariation"] ) {
            s.mLifetimeVariation = node["LifetimeVariation"].as<core::f32>();
        }

        if ( node["Speed"] ) {
            s.mSpeed = node["Speed"].as<core::f32>();
        }

        if ( node["SpeedVariation"] ) {
            s.mSpeedVariation = node["SpeedVariation"].as<core::f32>();
        }

        if ( node["Radius"] ) {
            s.mRadius = node["Radius"].as<core::f32>();
        }

        if ( node["ConeAngle"] ) {
            s.mConeAngle = node["ConeAngle"].as<core::f32>();
        }

        if ( node["Drag"] ) {
            s.mDrag = node["Drag"].as<core::f32>();
        }

        if ( node["Duration"] ) {
            s.mDuration = node["Duration"].as<core::f32>();
        }

        if ( node["StartSize"] ) {
            s.mStartSize = node["StartSize"].as<core::f32>();
        }

        if ( node["EndSize"] ) {
            s.mEndSize = node["EndSize"].as<core::f32>();
        }

        if ( node["AngularSpeed"] ) {
            s.mAngularSpeed = node["AngularSpeed"].as<core::f32>();
        }

        if ( node["Softness"] ) {
            s.mSoftness = node["Softness"].as<core::f32>();
        }

        if ( node["Looping"] ) {
            s.mLooping = node["Looping"].as<bool>();
        }

        if ( const auto value{ node["Direction"] } ) {
            if ( !value.IsSequence() || value.size() != 3 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid particle Direction." );
            }

            for ( core::usize i{}; i < 3; ++i ) {
                s.mDirection[i] = value[i].as<core::f32>();
            }
        }

        if ( const auto value{ node["Acceleration"] } ) {
            if ( !value.IsSequence() || value.size() != 3 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid particle Acceleration." );
            }

            for ( core::usize i{}; i < 3; ++i ) {
                s.mAcceleration[i] = value[i].as<core::f32>();
            }
        }

        if ( const auto value{ node["StartColor"] } ) {
            if ( !value.IsSequence() || value.size() != 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid particle StartColor." );
            }

            for ( core::usize i{}; i < 4; ++i ) {
                s.mStartColor[i] = value[i].as<core::f32>();
            }
        }

        if ( const auto value{ node["EndColor"] } ) {
            if ( !value.IsSequence() || value.size() != 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid particle EndColor." );
            }

            for ( core::usize i{}; i < 4; ++i ) {
                s.mEndColor[i] = value[i].as<core::f32>();
            }
        }

        s.Sanitize();
        return s;
    }

    ParticleEmitter::ParticleEmitter( const ParticleEmitterCreateDescription &description )
        : mSettings{ description.mSettings },
          mMaterial{ description.mMaterial } {
        mSettings.Sanitize();
        Initialize();
    }

    ParticleEmitter::~ParticleEmitter() {
        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto ParticleEmitter::Initialize() -> void {
        mIsAllocated = true;
    }

    auto ParticleEmitter::Destroy() -> void {
        mIsAllocated = false;
    }

    auto ParticleEmitter::GetSettings() const -> const ParticleEmitterSettings & {
        return mSettings;
    }

    auto ParticleEmitter::GetMaterial() const -> const material::ParticleMaterialHandle & {
        return mMaterial;
    }

    auto ParticleEmitter::GetTint() const -> core::float4 {
        return mMaterial ? mMaterial->GetColor() : core::float4{ 1.0f };
    }

    auto ParticleEmitter::GetTexture() const -> rhi::TextureHandle {
        return mMaterial ? mMaterial->GetTexture() : rhi::TextureHandle{};
    }

    auto ParticleEmitterInstance::AllocateID() -> core::u64 {
        static std::atomic<core::u64> next{ 1 };
        return next.fetch_add( 1, std::memory_order_relaxed );
    }

    ParticleEmitterInstance::ParticleEmitterInstance( ParticleEmitterHandle emitter )
        : mEmitter{ eastl::move( emitter ) } {
    }

    ParticleEmitterInstance::ParticleEmitterInstance( const ParticleEmitterInstance &other )
        : mEmitter{ other.mEmitter } {
    }

    auto ParticleEmitterInstance::operator=( const ParticleEmitterInstance &other ) -> ParticleEmitterInstance & {
        if ( this != &other ) {
            mEmitter = other.mEmitter;
            Restart();
        }

        return *this;
    }

    auto ParticleEmitterInstance::GetID() const -> core::u64 {
        return mID;
    }

    auto ParticleEmitterInstance::GetEmitter() const -> const ParticleEmitterHandle & {
        return mEmitter;
    }

    auto ParticleEmitterInstance::SetEmitter( ParticleEmitterHandle emitter ) -> void {
        mEmitter = eastl::move( emitter );
    }

    auto ParticleEmitterInstance::GetParticles() const -> const eastl::vector<Particle> & {
        return mParticles;
    }

    auto ParticleEmitterInstance::GetParticleTexture( core::u32 index ) const -> rhi::TextureHandle {
        return index < mTextureSlots.size() ? mTextureSlots[index].mTexture : rhi::TextureHandle{};
    }

    auto ParticleEmitterInstance::GetPlayback() const -> ParticlePlayback {
        return mPlayback;
    }

    auto ParticleEmitterInstance::GetDroppedParticles() const -> core::u64 {
        return mDroppedParticles;
    }

    auto ParticleEmitterInstance::Play() -> void {
        if ( mPlayback == ParticlePlayback::eStopped ) {
            Restart();
        }
        else {
            // Resuming a paused draining emitter must not restart emission.
            mPlayback = ParticlePlayback::ePlaying;
        }
    }

    auto ParticleEmitterInstance::Pause() -> void {
        mPlayback = ParticlePlayback::ePaused;
    }

    auto ParticleEmitterInstance::Stop( bool clear ) -> void {
        mEmitting = false;
        // A draining emitter keeps integrating old particles but produces no new ones.
        mPlayback = clear ? ParticlePlayback::eStopped : ParticlePlayback::ePlaying;

        if ( clear ) {
            mParticles.clear();
            mTextureSlots.clear();
            mTimeAccumulator = 0;
            ++mRestartRevision;
            mSimulationTick = 0;
        }
    }

    auto ParticleEmitterInstance::Restart() -> void {
        ++mRestartRevision;
        mSimulationTick = 0;
        mParticles.clear();
        mTextureSlots.clear();
        mPlayback = ParticlePlayback::ePlaying;
        mTimeAccumulator = 0;
        mEmissionAccumulator = 0;
        mElapsed = 0;
        mDrainTime = 0;
        mLongestParticleLifetime = 0;
        mDroppedParticles = 0;
        mStarted = false;
        mEmitting = true;
    }

    auto ParticleEmitterInstance::SetSimulationMode( ParticleSimulationMode mode ) -> void {
        if ( mSimulationMode == mode ) {
            return;
        }

        const auto playback{ mPlayback };
        const bool emitting{ mEmitting };
        mSimulationMode = mode;
        Restart();

        if ( mode == ParticleSimulationMode::eGpu ) {
            // GPU mode has no CPU particle mirror; release the old pool as well.
            eastl::vector<Particle>{}.swap( mParticles );
        }

        mPlayback = playback;
        mEmitting = emitting;
    }

    auto ParticleEmitterInstance::GetSimulationMode() const -> ParticleSimulationMode {
        return mSimulationMode;
    }

    auto ParticleEmitterInstance::GetSimulationTick() const -> core::u64 {
        return mSimulationTick;
    }

    auto ParticleEmitterInstance::GetRestartRevision() const -> core::u64 {
        return mRestartRevision;
    }

    auto ParticleEmitterInstance::IsEmitting() const -> bool {
        return mEmitting;
    }

    auto ParticleEmitterInstance::Random() -> core::f32 {
        // Instance-local xorshift: repeatable seeds without a global RNG or locks.
        mRandomState ^= mRandomState << 13;
        mRandomState ^= mRandomState >> 17;
        mRandomState ^= mRandomState << 5;
        return static_cast<core::f32>( mRandomState >> 8 ) * ( 1.0f / 16777216.0f );
    }

    auto ParticleEmitterInstance::BindSpawnTexture() -> void {
        const auto texture{ mEmitter->GetTexture() };
        mSpawnTextureIndex = kInvalidParticleTextureIndex;
        core::u32 available{ kInvalidParticleTextureIndex };

        for ( core::u32 index{}; index < mTextureSlots.size(); ++index ) {
            auto &slot{ mTextureSlots[index] };

            if ( texture && slot.mTexture == texture ) {
                mSpawnTextureIndex = index;
            }
            else if ( !slot.mLiveCount ) {
                slot.mTexture.Reset();
                available = index;
            }
        }

        if ( texture && mSpawnTextureIndex == kInvalidParticleTextureIndex ) {
            if ( available == kInvalidParticleTextureIndex ) {
                available = core::as<core::u32>( mTextureSlots.size() );
                mTextureSlots.emplace_back();
            }

            mTextureSlots[available].mTexture = texture;
            mSpawnTextureIndex = available;
        }
    }

    auto ParticleEmitterInstance::Spawn( const core::float4x4 &world ) -> void {
        const auto &s{ mEmitter->GetSettings() };
        const core::f32 phi{ Random() * glm::two_pi<core::f32>() };
        const core::f32 z{ Random() * 2.0f - 1.0f };
        const core::f32 radial{ glm::sqrt( eastl::max( 0.0f, 1.0f - z * z ) ) };
        // cbrt gives uniform volume density rather than clustering toward the center.
        const core::float3 offset{ core::float3{ radial * glm::cos( phi ), z, radial * glm::sin( phi ) } * ( s.mRadius * std::cbrt( Random() ) ) };
        const auto tangent{ glm::normalize( glm::cross( s.mDirection, glm::abs( s.mDirection.y ) < 0.99f ? core::float3{ 0, 1, 0 } : core::float3{ 1, 0, 0 } ) ) };
        const auto bitangent{ glm::cross( s.mDirection, tangent ) };
        const core::f32 cosTheta{ glm::mix( glm::cos( glm::radians( s.mConeAngle ) ), 1.0f, Random() ) };
        const core::f32 sinTheta{ glm::sqrt( eastl::max( 0.0f, 1.0f - cosTheta * cosTheta ) ) };
        const core::f32 angle{ Random() * glm::two_pi<core::f32>() };
        const auto localDirection{ s.mDirection * cosTheta + ( tangent * glm::cos( angle ) + bitangent * glm::sin( angle ) ) * sinTheta };
        auto direction{ core::float3{ world * core::float4{ localDirection, 0 } } };
        const auto directionLength{ glm::length( direction ) };
        direction = directionLength > 0.0001f ? direction / directionLength : s.mDirection;
        auto &p{ mParticles.emplace_back() };
        p.mPosition = core::float3{ world * core::float4{ offset, 1 } };
        p.mVelocity = direction * s.mSpeed * ( 1.0f + ( Random() * 2.0f - 1.0f ) * s.mSpeedVariation );
        p.mLifetime = s.mLifetime * ( 1.0f + ( Random() * 2.0f - 1.0f ) * s.mLifetimeVariation );
        p.mRotation = Random() * glm::two_pi<core::f32>();

        const auto tint{ mEmitter->GetTint() };
        p.mAcceleration = s.mAcceleration;
        p.mDrag = s.mDrag;
        p.mAngularSpeed = s.mAngularSpeed;
        p.mSoftness = s.mSoftness;
        // Capture animation at birth so live authoring only changes new particles.
        p.mFadeIn = s.mFadeEnabled ? s.mFadeIn : 0.0f;
        p.mFadeOut = s.mFadeEnabled ? s.mFadeOut : 0.0f;
        p.mStartSize = s.mStartSize;
        p.mEndSize = s.mEndSize;
        p.mStartColor = s.mStartColor * tint;
        p.mEndColor = s.mEndColor * tint;
        p.mTextureIndex = mSpawnTextureIndex;
        const auto &material{ mEmitter->GetMaterial() };
        p.mBloomIntensity = material && material->IsBloomEnabled() ? material->GetBloomIntensity() : 0.0f;

        if ( mSpawnTextureIndex != kInvalidParticleTextureIndex ) {
            ++mTextureSlots[mSpawnTextureIndex].mLiveCount;
        }
    }

    auto ParticleEmitterInstance::Step( core::f32 dt, const core::float4x4 &world ) -> void {
        const auto &s{ mEmitter->GetSettings() };
        // Swap-remove expired particles. Rendering sorts a separate snapshot, so
        // simulation storage never needs stable ordering or per-particle allocations.
        for ( core::usize i{}; i < mParticles.size(); ) {
            auto &p{ mParticles[i] };
            p.mAge += dt;

            if ( p.mAge >= p.mLifetime ) {
                if ( p.mTextureIndex != kInvalidParticleTextureIndex ) {
                    --mTextureSlots[p.mTextureIndex].mLiveCount;
                }

                p = mParticles.back();
                mParticles.pop_back();
                continue;
            }

            const core::f32 damping{ glm::exp( -p.mDrag * dt ) };
            p.mVelocity = ( p.mVelocity + p.mAcceleration * dt ) * damping;
            p.mPosition += p.mVelocity * dt;
            p.mRotation += p.mAngularSpeed * dt;
            ++i;
        }

        const core::f32 emissionTime{ s.mLooping ? dt : eastl::clamp( s.mDuration - mElapsed, 0.0f, dt ) };
        mElapsed += dt;

        if ( !mEmitting ) {
            return;
        }

        mEmissionAccumulator += s.mEmissionRate * emissionTime;
        const auto requested{ static_cast<core::u32>( mEmissionAccumulator ) };
        mEmissionAccumulator -= requested;
        // Reducing capacity lets existing particles expire naturally.
        const auto available{ s.mMaxParticles - eastl::min( s.mMaxParticles, core::as<core::u32>( mParticles.size() ) ) };
        const auto count{ eastl::min( requested, available ) };
        mDroppedParticles += requested - count;

        for ( core::u32 i{}; i < count; ++i ) {
            Spawn( world );
        }
    }

    auto ParticleEmitterInstance::Update( core::f32 timeStep, const core::float4x4 &world, core::u64 entitySeed ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mEmitter || mPlayback != ParticlePlayback::ePlaying || timeStep <= 0 ) {
            return;
        }

        const auto &s{ mEmitter->GetSettings() };

        if ( mSimulationMode == ParticleSimulationMode::eGpu ) {
            // Only the scene clock runs on the CPU. The compute pass owns particle
            // allocation, randomization, integration and expiry; views consume this clock.
            constexpr core::f64 fixedStep{ 1.0 / 60.0 };
            mTimeAccumulator = eastl::min( mTimeAccumulator + timeStep, fixedStep * 8.0 );

            while ( mTimeAccumulator >= fixedStep ) {
                ++mSimulationTick;
                mTimeAccumulator -= fixedStep;
                if ( mEmitting && ( s.mLooping || mElapsed < s.mDuration ) ) {
                    mLongestParticleLifetime = eastl::max( mLongestParticleLifetime, s.mLifetime * ( 1.0f + s.mLifetimeVariation ) );
                    mDrainTime = 0.0f;
                }
                else {
                    // Older recipes can still have live particles.
                    mDrainTime += static_cast<core::f32>( fixedStep );
                }

                mElapsed += static_cast<core::f32>( fixedStep );
            }

            if ( mDrainTime >= mLongestParticleLifetime + fixedStep ) {
                mPlayback = ParticlePlayback::eStopped;
            }

            return;
        }

        BindSpawnTexture();

        const auto seed{ s.mSeed ^ core::as<core::u32>( entitySeed ) ^ core::as<core::u32>( entitySeed >> 32 ) };

        if ( !mStarted || seed != mAppliedSeed ) {
            mAppliedSeed = seed;
            mRandomState = eastl::max( seed, 1u );
        }

        if ( !mStarted ) {
            mParticles.reserve( s.mMaxParticles );
            mStarted = true;

            if ( mEmitting ) {
                for ( core::u32 i{}; i < s.mBurstCount; ++i ) {
                    Spawn( world );
                }
            }
        }
        // Bound catch-up work after pauses/debugger stalls. Fixed steps keep
        // emission and integration independent of ordinary display frame rates.
        constexpr core::f64 fixedStep{ 1.0 / 60.0 };
        mTimeAccumulator = eastl::min( mTimeAccumulator + timeStep, fixedStep * 8.0 );

        while ( mTimeAccumulator >= fixedStep ) {
            Step( static_cast<core::f32>( fixedStep ), world );
            mTimeAccumulator -= fixedStep;
        }

        if ( ( !mEmitting || ( !s.mLooping && mElapsed >= s.mDuration ) ) && mParticles.empty() ) {
            mPlayback = ParticlePlayback::eStopped;
        }
    }
}// namespace mikoto::renderer
