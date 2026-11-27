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

// Intersect rays with terrain bounds and triangles for terrain-aware visibility. Height bounds
// reduce the number of detailed triangle tests along a sun ray.

#pragma once
#include "Memory_Helpers.slang"

struct TerrainSamplingRecord {
    float4x4 mWorld;
    float4x4 mWorldToLocal;
    uint4 mGrid;
};

bool TerrainRayBox( float3 origin, float3 ray, float3 lower, float3 upper, out float2 interval ) {
    float3 inverse = ( 2 * step( 0.0, ray ) - 1 ) / max( abs( ray ), 1e-12 );
    float3 a = ( lower - origin ) * inverse;
    float3 b = ( upper - origin ) * inverse;
    float3 lo = min( a, b );
    float3 hi = max( a, b );
    interval = float2( max( lo.x, max( lo.y, lo.z ) ), min( hi.x, min( hi.y, hi.z ) ) );
    return interval.y >= max( interval.x, .02 );
}

// Perform a bounded ray/triangle test on generated terrain. The hit distance must fall inside
// the requested sunlight segment.
bool TerrainRayTriangle( float3 origin, float3 ray, float3 a, float3 b, float3 c, float end ) {
    float3 e1 = b - a;
    float3 e2 = c - a;
    float3 h = cross( ray, e2 );
    float determinant = dot( e1, h );
    if ( abs( determinant ) < 1e-12 ) {
        return false;
    }
    float inverse = 1 / determinant;
    float3 s = origin - a;
    float u = dot( s, h ) * inverse;
    float3 q = cross( s, e1 );
    float v = dot( ray, q ) * inverse;
    float t = dot( e2, q ) * inverse;
    return u >= -1e-6 && v >= -1e-6 && u + v <= 1.000001 && t > .02 && t <= end + .001;
}

// Traverse the generated heightfield's actual triangles. Bounding boxes reject
// clouds above every peak immediately; traversal is independent of view/CSM range.
// Traverse the terrain height representation along the sun direction. Bounds reject empty
// regions before detailed surface intersection tests.
float TerrainSunVisibility( Pointer vertices, Pointer records, Pointer bounds, float3 position, float3 direction, float maximumDistance = 1e30 ) {
    if ( vertices == 0 || records == 0 || bounds == 0 ) {
        return 1;
    }
    uint count = LoadBufferValue<uint>( records, 0 );
    for ( uint terrainIndex = 0; terrainIndex < count; ++terrainIndex ) {
        TerrainSamplingRecord terrain = LoadBufferValue<TerrainSamplingRecord>( records, 16 + terrainIndex * sizeof( TerrainSamplingRecord ) );
        float3 origin = mul( terrain.mWorldToLocal, float4( position, 1 ) ).xyz;
        float3 ray = mul( terrain.mWorldToLocal, float4( direction, 0 ) ).xyz;
        float3 lower = LoadBufferValue<float4>( bounds, terrainIndex * 32 ).xyz - .001;
        float3 upper = LoadBufferValue<float4>( bounds, terrainIndex * 32 + 16 ).xyz + .001;
        float2 interval;
        if ( !TerrainRayBox( origin, ray, lower, upper, interval ) ) {
            continue;
        }
        interval.y = min( interval.y, maximumDistance );
        if ( interval.x >= interval.y ) {
            continue;
        }
        float3 first = LoadBufferValue<float4>( vertices, terrain.mGrid.z * 16 ).xyz;
        float3 next = LoadBufferValue<float4>( vertices, ( terrain.mGrid.z + 1 ) * 16 ).xyz;
        float spacing = max( next.x - first.x, 1e-5 );
        float time = max( interval.x, .02 );
        float2 p = ( ( origin + ray * ( time + .001 ) ).xz - first.xz ) / spacing;
        int2 cell = clamp( int2( floor( p ) ), 0, int2( terrain.mGrid.xy ) - 2 );
        int2 advance = int2( ray.x >= 0 ? 1 : -1, ray.z >= 0 ? 1 : -1 );
        float2 delta = spacing / max( abs( ray.xz ), 1e-12 );
        float2 edge = first.xz + ( float2( cell ) + float2( advance.x > 0 ? 1 : 0, advance.y > 0 ? 1 : 0 ) ) * spacing;
        float2 crossing = ( edge - origin.xz ) / ( ( 2 * step( 0.0, ray.xz ) - 1 ) * max( abs( ray.xz ), 1e-12 ) );
        if ( abs( ray.x ) < 1e-10 ) {
            crossing.x = 1e30;
        }
        if ( abs( ray.z ) < 1e-10 ) {
            crossing.y = 1e30;
        }
        uint budget = terrain.mGrid.x + terrain.mGrid.y + 2;
        for ( uint stepIndex = 0; stepIndex < budget && time <= interval.y; ++stepIndex ) {
            uint index = terrain.mGrid.z + uint( cell.y ) * terrain.mGrid.x + uint( cell.x );
            float3 a = LoadBufferValue<float4>( vertices, index * sizeof( float4 ) ).xyz;
            float3 b = LoadBufferValue<float4>( vertices, ( index + 1 ) * 16 ).xyz;
            float3 c = LoadBufferValue<float4>( vertices, ( index + terrain.mGrid.x ) * 16 ).xyz;
            float3 d = LoadBufferValue<float4>( vertices, ( index + terrain.mGrid.x + 1 ) * 16 ).xyz;
            if ( TerrainRayTriangle( origin, ray, a, b, c, interval.y ) || TerrainRayTriangle( origin, ray, b, d, c, interval.y ) ) {
                return 0;
            }
            bool x = crossing.x <= crossing.y;
            time = x ? crossing.x : crossing.y;
            if ( x ) {
                cell.x += advance.x;
                crossing.x += delta.x;
            }
            else {
                cell.y += advance.y;
                crossing.y += delta.y;
            }
            if ( any( cell < 0 ) || any( cell >= int2( terrain.mGrid.xy ) - 1 ) ) {
                break;
            }
        }
    }
    return 1;
}
