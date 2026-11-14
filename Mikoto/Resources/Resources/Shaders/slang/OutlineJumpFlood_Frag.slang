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

#pragma once

#include "base/Memory_Helpers.slang"
#include "base/Outline_Helpers.slang"

struct DrawParams {
    uint Seeds;
    uint Step;
    uint Axis; // 0: horizontal, 1: vertical.
    uint Padding;
};
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D<float2> ) seeds;

[shader( "fragment" )]
float2 main( float2 unusedUV : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    uint width, height;
    seeds[parameters.Seeds].GetDimensions( width, height );
    int2 dimensions = int2( width, height );
    int2 pixel = int2( position.xy );
    int2 axisStep = parameters.Axis == 0 ? int2( parameters.Step, 0 ) : int2( 0, parameters.Step );
    float bestDistanceSquared = 3.402823e+38f;
    float2 bestSeed = kInvalidOutlineSeed;

    // Three samples per axis, including the center. Horizontal then vertical
    // replaces the source's optional 3x3 pass, following its default C# workflow.
    [unroll]
    for ( int offset = -1; offset <= 1; ++offset ) {
        int2 neighbor = clamp( pixel + offset * axisStep, int2( 0 ), dimensions - 1 );
        float2 candidate = seeds[parameters.Seeds].Load( int3( neighbor, 0 ) );
        if ( !IsOutlineSeedValid( candidate ) ) { continue; }

        // Always compare to THIS fragment, not to the neighbor that supplied the seed.
        float2 delta = position.xy - DecodeOutlineSeed( candidate, float2( dimensions ) );
        float distanceSquared = dot( delta, delta );
        if ( distanceSquared < bestDistanceSquared ) {
            bestDistanceSquared = distanceSquared;
            bestSeed = candidate;
        }
    }
    // Preserve the encoded seed directly; repeated decode/re-encode adds quantization error.
    return bestSeed;
}
