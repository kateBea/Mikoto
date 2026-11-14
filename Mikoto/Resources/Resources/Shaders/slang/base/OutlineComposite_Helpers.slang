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

#include "Memory_Helpers.slang"
#include "Outline_Helpers.slang"

struct DrawParams {
    float4 Color;
    uint Seeds;
    float Width;
    uint Interior;
    uint Padding;
};
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

float4 EvaluateOutline( int2 pixel, float2 sampleUV ) {
    float2 encoded = textures[parameters.Seeds].Load( int3( pixel, 0 ) ).rg;
    if ( !IsOutlineSeedValid( encoded ) ) { discard; }

    uint width, height;
    textures[parameters.Seeds].GetDimensions( width, height );
    float2 dimensions = float2( width, height );
    float2 nearest = DecodeOutlineSeed( encoded, dimensions );
    float distance = length( sampleUV * dimensions - nearest );
    // Golus's one-pixel fringe makes the distance threshold antialiased.
    float alpha = parameters.Color.a * saturate( parameters.Width - distance + 1.0f );
    if ( alpha <= 0.0f ) { discard; }

    // Premultiply before MSAA resolve to avoid dark fringes from uncovered samples.
    return float4( parameters.Color.rgb * alpha, alpha );
}
