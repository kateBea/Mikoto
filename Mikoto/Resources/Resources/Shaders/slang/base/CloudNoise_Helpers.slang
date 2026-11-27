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

// Deterministic gradient, Worley and fractal noise used by cloud texture generation. Lattice
// hashing keeps neighboring evaluations consistent across cell boundaries.

#pragma once

// Periodic gradient/cellular noise feeds separate shape and erosion textures.
// Cloud modeling reference: https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn
uint CloudLatticeHash( int3 cell, int period ) {
    cell = ( cell % period + period ) % period;
    uint value = uint( cell.x ) * 73856093u ^ uint( cell.y ) * 19349663u ^ uint( cell.z ) * 83492791u;
    return CloudHash( value );
}

float CloudGradientNoise( float3 p, int period ) {
    int3 cell = int3( floor( p ) );
    float3 fraction = frac( p );
    float3 fade = fraction * fraction * fraction * ( fraction * ( fraction * 6 - 15 ) + 10 );
    float value = 0;
    for ( int z = 0; z < kLinearFilterSide; ++z ) {
        for ( int y = 0; y < kLinearFilterSide; ++y ) {
            for ( int x = 0; x < kLinearFilterSide; ++x ) {
                int3 offset = int3( x, y, z );
                uint seed = CloudLatticeHash( cell + offset, period );
                float3 gradient = float3( CloudRandom( seed ), CloudRandom( seed + 19 ), CloudRandom( seed + 47 ) ) * 2 - 1;
                gradient /= max( length( gradient ), 1e-5 );
                float3 weight = lerp( 1 - fade, fade, float3( offset ) );
                value += dot( gradient, fraction - float3( offset ) ) * weight.x * weight.y * weight.z;
            }
        }
    }
    return saturate( .5 + value * .9 );
}

float CloudWorley( float3 p, int period ) {
    int3 cell = int3( floor( p ) );
    float nearest = 3;
    for ( int z = -1; z <= 1; ++z ) {
        for ( int y = -1; y <= 1; ++y ) {
            for ( int x = -1; x <= 1; ++x ) {
                int3 adjacent = cell + int3( x, y, z );
                uint seed = CloudLatticeHash( adjacent, period );
                float3 point = float3( CloudRandom( seed ), CloudRandom( seed + 17 ), CloudRandom( seed + 73 ) );
                nearest = min( nearest, length( float3( adjacent ) + point - p ) );
            }
        }
    }
    return saturate( 1 - nearest );
}

float CloudGradientFbm( float3 uv, int period ) {
    return CloudGradientNoise( uv * period, period ) * .625 +
           CloudGradientNoise( uv * ( period * 2 ), period * 2 ) * .25 +
           CloudGradientNoise( uv * ( period * 4 ), period * 4 ) * .125;
}
