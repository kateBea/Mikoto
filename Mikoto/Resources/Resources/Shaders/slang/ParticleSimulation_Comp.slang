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

// Advance one emitter per workgroup, including deterministic spawning and fixed simulation
// steps. Live states are converted into render records after all lanes finish the simulation
// phase.

#include "base/Particle.slang"

struct SimulationParameters {
    RWBufferReference<ParticleGpuState> mStates;
    RWBufferReference<ParticleGpuHeader> mHeaders;
    BufferReference<ParticleEmitterParameters> mEmitters;
    RWBufferReference<ParticleRenderData> mOutput;
    BufferReference<uint> mIndices;
    BufferReference<uint> mTextureDescriptors;
    float4x4 mView;
};
MKT_DECLARE_PUSH_CONSTANT( SimulationParameters, parameters );

// The workgroup shares emitter bookkeeping, but each lane owns a strided set of particle slots.
// Lane zero alone advances the spawn/random counters between synchronized phases.
groupshared ParticleGpuHeader header;

float Random() {
    header.mRandom ^= header.mRandom << 13;
    header.mRandom ^= header.mRandom >> 17;
    header.mRandom ^= header.mRandom << 5;
    return float( header.mRandom >> 8 ) * ( 1.0 / 16777216.0 );
}

// Sample the emitter volume and cone direction, then capture appearance/lifetime values into a
// new particle. Existing live particles retain the appearance recorded at birth.
ParticleGpuState Spawn( ParticleEmitterParameters emitter ) {
    const float tau = 6.28318530718;
    float phi = Random() * tau;
    float z = Random() * 2 - 1;
    float radial = sqrt( max( 0.0, 1 - z * z ) );
    float3 offset =
        float3( radial * cos( phi ), z, radial * sin( phi ) ) * emitter.mEmission.y * pow( Random(), 1.0 / 3.0 );
    float3 axis = emitter.mDirectionCone.xyz;
    float3 tangent = normalize( cross( axis, abs( axis.y ) < 0.99 ? float3( 0, 1, 0 ) : float3( 1, 0, 0 ) ) );
    float3 bitangent = cross( axis, tangent );
    float cosTheta = lerp( cos( emitter.mDirectionCone.w ), 1.0, Random() );
    float sinTheta = sqrt( max( 0.0, 1 - cosTheta * cosTheta ) );
    float angle = Random() * tau;
    float3 direction =
        mul( emitter.mWorld,
            float4( axis * cosTheta + ( tangent * cos( angle ) + bitangent * sin( angle ) ) * sinTheta, 0 ) )
            .xyz;
    direction = length( direction ) > 0.0001 ? normalize( direction ) : axis;
    ParticleGpuState p = ( ParticleGpuState )0;
    p.mPositionAge = float4( mul( emitter.mWorld, float4( offset, 1 ) ).xyz, 0 );
    p.mVelocityLifetime.xyz =
        direction * emitter.mLifetimeSpeed.z * ( 1 + ( Random() * 2 - 1 ) * emitter.mLifetimeSpeed.w );
    p.mVelocityLifetime.w = emitter.mLifetimeSpeed.x * ( 1 + ( Random() * 2 - 1 ) * emitter.mLifetimeSpeed.y );
    p.mRotationFade = float4( Random() * tau, emitter.mFadeIn, emitter.mFadeOut, emitter.mEmission.w );
    p.mAccelerationDrag = emitter.mAccelerationDrag;
    p.mSizeRotationSoftness = emitter.mSizeRotationSoftness;
    p.mStartColor = emitter.mStartColor;
    p.mEndColor = emitter.mEndColor;
    p.mTexture.x = emitter.mTextureIndex;
    return p;
}

// Fill available particle slots from the emitter spawn budget. A reduced authored particle
// limit is applied before new particles are admitted.
void Emit( ParticleEmitterParameters emitter, uint count ) {
    // One lane allocates dead slots in a deterministic order. Integration below
    // is parallel; no atomics, cross-emitter contention or CPU alive-list readback.
    if ( emitter.mMaxParticles < emitter.mCapacity ) {
        uint alive = 0;
        for ( uint i = 0; i < emitter.mCapacity; ++i ) {
            uint slot = parameters.mIndices[emitter.mOffset + i];
            alive += parameters.mStates[slot].mVelocityLifetime.w > 0 ? 1u : 0u;
        }

        count = min( count, emitter.mMaxParticles - min( alive, emitter.mMaxParticles ) );
    }

    for ( uint i = 0; i < emitter.mCapacity && count != 0; ++i ) {
        uint slot = parameters.mIndices[emitter.mOffset + i];
        if ( parameters.mStates[slot].mVelocityLifetime.w <= 0 ) {
            parameters.mStates[slot] = Spawn( emitter );
            --count;
        }
    }
}

[shader( "compute" )]
    [numthreads( kParticleSimulationGroupSize, 1, 1 )] void
    main( uint3 group : SV_GroupID, uint lane : SV_GroupIndex ) {
        ParticleEmitterParameters emitter = parameters.mEmitters[group.x];
        if ( emitter.mCapacity == 0 ) {
            return;
        }
        if ( lane == 0 ) {
            header = emitter.mReset != 0 ? ( ParticleGpuHeader )0 : parameters.mHeaders[group.x];

            if ( emitter.mReset != 0 || header.mSeed != emitter.mSeed ) {
                header.mSeed = emitter.mSeed;
                header.mRandom = max( emitter.mSeed, 1u );
            }
        }

        // New indices can contain bytes from a retired emitter. Clear those
        // indices while keeping every existing live particle in its original slot.
        uint first = emitter.mReset != 0 ? 0 : emitter.mPreviousCapacity;
        for ( uint i = first + lane; i < emitter.mCapacity; i += kParticleSimulationGroupSize ) {
            uint slot = parameters.mIndices[emitter.mOffset + i];
            parameters.mStates[slot] = ( ParticleGpuState )0;
        }

        AllMemoryBarrierWithGroupSync();
        if ( lane == 0 && header.mInitialized == 0 && emitter.mInitialize != 0 ) {
            header.mInitialized = 1;
            if ( emitter.mEmitting != 0 ) {
                Emit( emitter, emitter.mBurst );
            }
        }
        AllMemoryBarrierWithGroupSync();

        const float dt = 1.0 / 60.0;
        // Run the requested fixed simulation steps. Slot updates complete across the workgroup
        // before lane zero spawns the particles due at that step.
        for ( uint step = 0; step < emitter.mSteps; ++step ) {
            for ( uint i = lane; i < emitter.mCapacity; i += kParticleSimulationGroupSize ) {
                uint slot = parameters.mIndices[emitter.mOffset + i];
                ParticleGpuState p = parameters.mStates[slot];
                if ( p.mVelocityLifetime.w > 0 ) {
                    p.mPositionAge.w += dt;
                    if ( p.mPositionAge.w >= p.mVelocityLifetime.w ) {
                        p.mVelocityLifetime.w = 0;
                    }
                    else {
                        float damping = exp( -p.mAccelerationDrag.w * dt );
                        p.mVelocityLifetime.xyz =
                            ( p.mVelocityLifetime.xyz + p.mAccelerationDrag.xyz * dt ) * damping;
                        p.mPositionAge.xyz += p.mVelocityLifetime.xyz * dt;
                        p.mRotationFade.x += p.mSizeRotationSoftness.z * dt;
                    }
                    parameters.mStates[slot] = p;
                }
            }
            // Expiry must be visible before allocation reuses slots, and newly spawned
            // particles must be visible before the next integration step begins.
            AllMemoryBarrierWithGroupSync();
            if ( lane == 0 ) {
                float emissionTime = emitter.mLooping != 0 ? dt : clamp( emitter.mEmission.z - header.mElapsed, 0.0, dt );
                header.mElapsed += dt;
                if ( emitter.mEmitting != 0 ) {
                    header.mEmissionRemainder += emitter.mEmission.x * emissionTime;
                    uint count = uint( header.mEmissionRemainder );
                    header.mEmissionRemainder -= float( count );
                    Emit( emitter, count );
                }
            }
            AllMemoryBarrierWithGroupSync();
        }
        if ( lane == 0 ) {
            // Publish persistent emitter counters after simulation. The following render-record
            // phase derives current size, color, fade and depth from each surviving particle.
            parameters.mHeaders[group.x] = header;
        }

        // Snapshot generation is view-dependent; simulation state remains in world space.
        for ( uint i = lane; i < emitter.mCapacity; i += kParticleSimulationGroupSize ) {
            uint slot = parameters.mIndices[emitter.mOffset + i];
            ParticleGpuState p = parameters.mStates[slot];
            if ( emitter.mVisible == 0 || p.mVelocityLifetime.w <= 0 ) {
                continue;
            }
            float age = saturate( p.mPositionAge.w / p.mVelocityLifetime.w );
            float size = lerp( p.mSizeRotationSoftness.x, p.mSizeRotationSoftness.y, age );
            float4 color = lerp( p.mStartColor, p.mEndColor, age );
            // Match Particle::GetColor. Durations belong to each particle's birth settings.
            float fadeIn = p.mRotationFade.y > 0 ? smoothstep( 0.0, p.mRotationFade.y, age ) : 1.0;
            float fadeOut = p.mRotationFade.z > 0 ? 1.0 - smoothstep( 1.0 - p.mRotationFade.z, 1.0, age ) : 1.0;
            color.a *= fadeIn * fadeOut;
            float viewDepth = -mul( parameters.mView, float4( p.mPositionAge.xyz, 1 ) ).z;
            if ( viewDepth <= -size || size <= 0 || color.a <= 0 ) {
                continue;
            }
            ParticleRenderData output;
            output.mPositionSize = float4( p.mPositionAge.xyz, size );
            output.mColor = color;
            output.mRotationSoftness = float4( p.mRotationFade.x, p.mSizeRotationSoftness.w, viewDepth, float( slot ) );
            uint descriptor = kInvalidParticleTextureIndex;
            if ( p.mTexture.x != kInvalidParticleTextureIndex ) {
                descriptor = parameters.mTextureDescriptors[emitter.mTextureOffset + p.mTexture.x];
            }
            output.mTexture = uint4( descriptor, asuint( p.mRotationFade.w ), 0, 0 );
            parameters.mOutput[slot] = output;
        }
    }
