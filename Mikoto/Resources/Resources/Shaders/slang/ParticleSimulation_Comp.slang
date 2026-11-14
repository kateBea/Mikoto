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

#include "base/Particle.slang"

struct SimulationParameters {
    RWBufferReference<ParticleGpuState> mStates;
    RWBufferReference<ParticleGpuHeader> mHeaders;
    BufferReference<ParticleEmitterParameters> mEmitters;
    RWBufferReference<ParticleRenderData> mOutput;
    float4x4 mView;
};
MKT_DECLARE_PUSH_CONSTANT( SimulationParameters, parameters );

groupshared ParticleGpuHeader header;

float Random() {
    header.mRandom ^= header.mRandom << 13;
    header.mRandom ^= header.mRandom >> 17;
    header.mRandom ^= header.mRandom << 5;
    return float( header.mRandom >> 8 ) * ( 1.0 / 16777216.0 );
}

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
    p.mRotation.x = Random() * tau;
    return p;
}

void Emit( ParticleEmitterParameters emitter, uint count ) {
    // One lane allocates dead slots in a deterministic order. Integration below
    // is parallel; no atomics, cross-emitter contention or CPU alive-list readback.
    for ( uint i = 0; i < emitter.mCapacity && count != 0; ++i ) {
        uint slot = emitter.mOffset + i;
        if ( parameters.mStates[slot].mVelocityLifetime.w <= 0 ) {
            parameters.mStates[slot] = Spawn( emitter );
            --count;
        }
    }
}

[shader( "compute" )]
[numthreads( 128, 1, 1 )]
void main( uint3 group : SV_GroupID, uint lane : SV_GroupIndex ) {
    ParticleEmitterParameters emitter = parameters.mEmitters[group.x];
    if ( emitter.mCapacity == 0 ) {
        return;
    }
    if ( lane == 0 ) {
        if ( emitter.mReset != 0 ) {
            header = ( ParticleGpuHeader )0;
            header.mRandom = max( emitter.mSeed, 1u );
        } else {
            header = parameters.mHeaders[group.x];
        }
    }
    if ( emitter.mReset != 0 ) {
        for ( uint i = lane; i < emitter.mCapacity; i += 128 ) {
            parameters.mStates[emitter.mOffset + i] = ( ParticleGpuState )0;
        }
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
    const float damping = exp( -emitter.mAccelerationDrag.w * dt );
    for ( uint step = 0; step < emitter.mSteps; ++step ) {
        for ( uint i = lane; i < emitter.mCapacity; i += 128 ) {
            uint slot = emitter.mOffset + i;
            ParticleGpuState p = parameters.mStates[slot];
            if ( p.mVelocityLifetime.w > 0 ) {
                p.mPositionAge.w += dt;
                if ( p.mPositionAge.w >= p.mVelocityLifetime.w ) {
                    p.mVelocityLifetime.w = 0;
                } else {
                    p.mVelocityLifetime.xyz =
                            ( p.mVelocityLifetime.xyz + emitter.mAccelerationDrag.xyz * dt ) * damping;
                    p.mPositionAge.xyz += p.mVelocityLifetime.xyz * dt;
                    p.mRotation.x += emitter.mSizeRotationSoftness.z * dt;
                }
                parameters.mStates[slot] = p;
            }
        }
        // Expiry must be visible before allocation reuses slots, and newly spawned
        // particles must be visible before the next integration step begins.
        AllMemoryBarrierWithGroupSync();
        if ( lane == 0 ) {
            float emissionTime = emitter.mLooping != 0 ? dt : clamp( emitter.mEmission.z - header.mElapsed, 0.0, dt );
            header.mElapsed = min( header.mElapsed + dt, emitter.mEmission.z );
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
        parameters.mHeaders[group.x] = header;
    }

    // Snapshot generation is view-dependent; simulation state remains in world space.
    for ( uint i = lane; i < emitter.mCapacity; i += 128 ) {
        uint slot = emitter.mOffset + i;
        ParticleGpuState p = parameters.mStates[slot];
        if ( emitter.mVisible == 0 || p.mVelocityLifetime.w <= 0 ) {
            continue;
        }
        float age = saturate( p.mPositionAge.w / p.mVelocityLifetime.w );
        float size = lerp( emitter.mSizeRotationSoftness.x, emitter.mSizeRotationSoftness.y, age );
        float4 color = lerp( emitter.mStartColor, emitter.mEndColor, age );
        float viewDepth = -mul( parameters.mView, float4( p.mPositionAge.xyz, 1 ) ).z;
        if ( viewDepth <= -size || size <= 0 || color.a <= 0 ) {
            continue;
        }
        ParticleRenderData output;
        output.mPositionSize = float4( p.mPositionAge.xyz, size );
        output.mColor = color;
        output.mRotationSoftness = float4( p.mRotation.x, emitter.mSizeRotationSoftness.w, viewDepth, float( slot ) );
        parameters.mOutput[slot] = output;
    }
}
