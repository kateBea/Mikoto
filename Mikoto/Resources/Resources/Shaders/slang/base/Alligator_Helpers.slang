// SPDX-License-Identifier: CC-BY-SA-4.0
// Alligator Noise, Side Effects Software Inc. GPU adaptation by Mikoto, 2026.
// Reference: https://www.sidefx.com/docs/hdk/alligator_2alligator_8_c-example.html
// License: https://creativecommons.org/licenses/by-sa/4.0/
// Changes: periodic lattice hashing, float arithmetic and two running maxima
// replace HDK hashing, double arithmetic and the priority queue.
// See Alligator.LICENSE.txt for the retained HDK sample notice.

// Cloud detail erosion and curl helpers derived from the adapted noise model. Curl perturbs
// sampling positions while the erosion term breaks up smooth cloud silhouettes.

#pragma once
#include "CloudNoise_Helpers.slang"

// Alligator is the difference between the two strongest random, compact radial
// kernels. Nearest-point distance alone would produce a different noise family.
float CloudAlligator( float3 position, int period ) {
    int3 cell = int3( floor( position ) );
    float3 fraction = frac( position );
    float first = 0;
    float second = 0;
    for ( int z = -1; z <= 1; ++z ) {
        for ( int y = -1; y <= 1; ++y ) {
            for ( int x = -1; x <= 1; ++x ) {
                int3 offset = int3( x, y, z );
                uint seed = CloudLatticeHash( cell + offset, period );
                float3 center = float3( offset ) + float3( CloudRandom( seed ), CloudRandom( seed + 17 ), CloudRandom( seed + 73 ) );
                float radius = saturate( 1 - length( fraction - center ) );
                float kernel = radius * radius * ( 3 - 2 * radius );
                float value = CloudRandom( seed + 119 ) * kernel;
                second = max( second, min( first, value ) );
                first = max( first, value );
            }
        }
    }
    return first - second;
}

float3 CloudCurlPotential( float3 p, int period ) {
    return float3( CloudGradientNoise( p, period ), CloudGradientNoise( p + float3( 17.3, 9.2, 3.7 ), period ),
        CloudGradientNoise( p + float3( 7.1, 31.8, 11.6 ), period ) );
}

float3 CloudDetailCurl( float3 p, int period ) {
    const float epsilon = .08;
    float3 dx = ( CloudCurlPotential( p + float3( epsilon, 0, 0 ), period ) - CloudCurlPotential( p - float3( epsilon, 0, 0 ), period ) ) / ( 2 * epsilon );
    float3 dy = ( CloudCurlPotential( p + float3( 0, epsilon, 0 ), period ) - CloudCurlPotential( p - float3( 0, epsilon, 0 ), period ) ) / ( 2 * epsilon );
    float3 dz = ( CloudCurlPotential( p + float3( 0, 0, epsilon ), period ) - CloudCurlPotential( p - float3( 0, 0, epsilon ), period ) ) / ( 2 * epsilon );
    return float3( dy.z - dz.y, dz.x - dx.z, dx.y - dy.x );
}
