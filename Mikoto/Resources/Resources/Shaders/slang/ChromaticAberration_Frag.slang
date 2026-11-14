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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct DrawParams {
    uint mSamplerID;
    uint mTonemapImageID;
    float2 mTexelSize;
    float mIntensity;
    float mDirection; // Radians.
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

// Credits:
// Created by moonlightoctopus in 2025-05-25
// https://www.shadertoy.com/view/tcKGWw
// Mikoto additions: adjustable strength/direction, clamped edges and preserved alpha.

float4 mainImage( float2 uv ) {
    SamplerState sampler = samplers[parameters.mSamplerID];
    Texture2D tonemappedImage = textures[parameters.mTonemapImageID];

    float distortionAmount = parameters.mIntensity;
    float2 pixelSize = parameters.mTexelSize;
    float2 lower = pixelSize * 0.5;
    float2 upper = 1.0 - lower;

    // At 45 degrees the direction is (1, 1), restoring the original per-axis
    // three-pixel step. Red stays in place; green shifts once and blue twice.
    float2 direction = float2( cos( parameters.mDirection ), sin( parameters.mDirection ) );
    direction /= max( abs( direction.x ), abs( direction.y ) );
    float2 offset = pixelSize * distortionAmount * direction;

    float2 redUV = clamp( uv, lower, upper );
    float2 greenUV = clamp( uv - offset, lower, upper );
    float2 blueUV = clamp( uv - 2.0 * offset, lower, upper );

    float4 col1 = tonemappedImage.SampleLevel( sampler, redUV, 0 );
    float4 col2 = tonemappedImage.SampleLevel( sampler, greenUV, 0 );
    float4 col3 = tonemappedImage.SampleLevel( sampler, blueUV, 0 );

    float3 color = float3( col1.r, col2.g, col3.b );
    return float4( color, col1.a );
}

[shader( "fragment" )]
float4 main( FSInput input ) : SV_Target0 {
    return mainImage( input.UV );
}
