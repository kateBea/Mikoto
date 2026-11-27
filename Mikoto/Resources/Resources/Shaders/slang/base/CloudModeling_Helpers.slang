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

// Evaluate the seeded large-scale density profile inside a cloud formation. This profile
// supplies coherent lobes before high-frequency erosion is applied.

#pragma once

#include "CloudFormation_Helpers.slang"

float CloudSeededProfile( CloudsFrameData frame, CloudRecord cloud, float3 local ) {
    uint count = clamp( uint( cloud.mFormation.x ), 1u, 64u );
    uint side = uint( ceil( sqrt( float( count ) ) ) );
    int2 cell = int2( floor( ( local.xz + .5 ) * side ) );
    float3 extent = CloudMetricExtent( cloud );
    float3 p = local * extent;
    float spacing = min( extent.x, extent.z ) / side;
    float coverage = saturate( cloud.mShape.x );
    if ( coverage <= 0 ) {
        return 0;
    }
    float variation = saturate( cloud.mFormation.z );
    float scale = clamp( cloud.mFormation.y, .2, 3.0 );
    float distance = 1e20;
    // Neighbourhood remains bounded at high seed counts. The search radius grows
    // with formation scale so enlarged clusters are not cut at seed-cell edges.
    int search = min( 5, int( ceil( scale * 1.5 ) ) + 1 );
    for ( int z = -search; z <= search; ++z ) {
        for ( int x = -search; x <= search; ++x ) {
            int2 q = cell + int2( x, z );
            if ( any( q < 0 ) || any( q >= int2( side, side ) ) ) {
                continue;
            }
            uint id = uint( q.x + q.y * side );
            if ( id >= count ) {
                continue;
            }
            uint seed = cloud.mFlags.z ^ ( id * 911u + 17u );
            float3 random = float3( CloudFormationRandom( seed ), CloudFormationRandom( seed + 1 ), CloudFormationRandom( seed + 2 ) );
            float2 center = ( ( float2( q ) + .5 + ( random.xz - .5 ) * variation * .65 ) / side - .5 ) * extent.xz;
            float3 samplePosition = float3( center.x, 0, center.y );
            float3 material = samplePosition * max( cloud.mShape.z * 3, 1e-5 );
            float4 weather = CloudWeather( frame, cloud, material );
            float regional = saturate( coverage + ( weather.r - .5 ) * cloud.mWeather.y );
            if ( regional < .02 ) {
                continue;
            }
            float form = clamp( cloud.mProfile.x + ( weather.g - .5 ) * cloud.mWeather.z * 2, 0, 3 );
            float radius = spacing * ( .14 + regional * .38 ) * scale * lerp( .8, 1.2, random.x * variation );
            float height = -.08 + ( random.y - .5 ) * .20 * variation * cloud.mWeather.w;
            float body = CloudFormationDistance( p, center, radius, height, form, seed, random );
            distance = min( distance, body );
        }
    }
    float3 materialPosition = CloudMaterialPosition( cloud, local );
    float seedOffset = float( cloud.mFlags.z % 4096u ) * .0137;
    float3 noisePosition = materialPosition * max( cloud.mDensityDetail.y, .000001 ) * .5 + seedOffset;
    float4 noise = cloudVolumes[frame.mShapeResources.x].SampleLevel( cloudSamplers[frame.mResources.z], noisePosition, 0 );
    // Keep interior density variation at the lobe scale. Using only the thin
    // outer shell width here saturates every lobe into a featureless solid ball.
    float profileScale = distance < 0 ? .14 + coverage * .38 : cloud.mShape.w;
    float shapingScale = max( spacing * profileScale * scale, .015 );
    float normalizedDistance = distance / shapingScale;
    float billow = lerp( .5, noise.r, variation );
    float profile = billow < normalizedDistance ? 0 : saturate( billow * ( 1 - normalizedDistance ) );
    float border = smoothstep( 0, .04, .5 - max( abs( local.x ), max( abs( local.y ), abs( local.z ) ) ) );
    return profile * border;
}
