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

struct DrawParams { uint Silhouette; };
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) silhouettes;

[shader( "fragment" )]
float2 main( float2 unusedUV : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    uint width, height;
    silhouettes[parameters.Silhouette].GetDimensions( width, height );
    int2 dimensions = int2( width, height );
    int2 pixel = int2( position.xy );

    // MSAA resolve gives fractional coverage along silhouette edges.
    // Matrix indexing follows the source algorithm: first index is x, second is y.
    float3x3 coverage;
    [unroll]
    for ( int x = 0; x < 3; ++x ) {
        [unroll]
        for ( int y = 0; y < 3; ++y ) {
            int2 neighbor = clamp( pixel + int2( x - 1, y - 1 ), int2( 0 ), dimensions - 1 );
            coverage[x][y] = silhouettes[parameters.Silhouette].Load( int3( neighbor, 0 ) ).r;
        }
    }

    float center = coverage[1][1];
    if ( center > 0.99f ) {
        return EncodeOutlineSeed( position.xy, float2( dimensions ) );
    }
    if ( center < 0.01f ) {
        return kInvalidOutlineSeed;
    }

    // Sobel estimates the direction towards the covered interior.
    float2 direction = -float2(
        coverage[0][0] + 2.0f * coverage[0][1] + coverage[0][2] -
        coverage[2][0] - 2.0f * coverage[2][1] - coverage[2][2],
        coverage[0][0] + 2.0f * coverage[1][0] + coverage[2][0] -
        coverage[0][2] - 2.0f * coverage[1][2] - coverage[2][2] );

    // Isolated subpixel dots/lines have no reliable gradient. Keep their seeds.
    if ( all( abs( direction ) <= float2( 0.005f ) ) ) {
        return EncodeOutlineSeed( position.xy, float2( dimensions ) );
    }
    float2 edgePosition = position.xy + normalize( direction ) * ( 1.0f - center );
    return EncodeOutlineSeed( edgePosition, float2( dimensions ) );
}
