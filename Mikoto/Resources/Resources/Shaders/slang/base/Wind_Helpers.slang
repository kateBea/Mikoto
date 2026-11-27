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

// Sample the uploaded world-space wind field and localized influences. Returned wind combines
// the background flow with spatially varying gusts and interaction controls.

#pragma once

#include "Memory_Helpers.slang"

// Sine-wave wind inspired by Guerrilla's shared vegetation force field and AMD's
// description. CPU parity: Scene/Sources/Wind.cc. No fluid state/readback required.
// https://www.gdcvault.com/play/1025530/contactUs
// https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
struct WindAreaData {
    float4x4 mWorldToLocal;
    float4 mVelocityFalloff;
    float4 mWaves;
    uint4 mControl;
};

float3 SampleWindField( uint64_t field, float3 position ) {
    if ( field == 0 ) {
        return 0;
    }
    float time = LoadBufferValue<float>( field, 0 );
    uint count = min( LoadBufferValue<uint>( field, 4 ), 64u );
    float3 result = 0;
    for ( uint index = 0; index < count; ++index ) {
        WindAreaData area = LoadBufferValue<WindAreaData>( field, 16 + index * sizeof( WindAreaData ) );
        float weight = 1;
        if ( area.mControl.x != 0 ) {
            float3 local = mul( area.mWorldToLocal, float4( position, 1 ) ).xyz;
            float distance = area.mControl.x == 2 ? length( local ) * 2 : max( abs( local.x ), max( abs( local.y ), abs( local.z ) ) ) * 2;
            if ( distance >= 1 ) {
                continue;
            }
            weight = smoothstep( 0, max( area.mVelocityFalloff.w, .001 ), 1 - distance );
        }
        float3 velocity = area.mVelocityFalloff.xyz;
        float speed = length( velocity );
        float3 direction = speed > .001 ? velocity / speed : float3( 1, 0, 0 );
        float3 side = normalize( float3( -direction.z, .15, direction.x ) );
        float phase = float( area.mControl.y % 4096u ) * .017;
        float spatial = dot( position, direction ) * 6.283185307 / max( area.mWaves.z, .1 );
        float clock = time * area.mWaves.y * 6.283185307;
        float broad = sin( spatial - clock + phase );
        float detail = sin( spatial * 2.17 - clock * 1.31 + phase * 1.7 );
        float gust = max( 0, 1 + area.mWaves.x * ( broad * .7 + detail * .3 ) );
        float lateral = sin( dot( position, side ) * .37 - clock * 1.73 + phase );
        result += ( velocity * gust + side * lateral * area.mWaves.w ) * weight;
    }
    return result;
}
