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

// Shared channel-offset sampling for chromatic aberration. The caller supplies the screen
// position, source texture and strength of the radial displacement.

#pragma once

// Channel separation adapted from moonlightoctopus, 2025-05-25.
// https://www.shadertoy.com/view/tcKGWw
// Clamp the shifted samples at the image edges and preserve the source alpha.
float4 SampleChromaticAberration( Texture2D source, SamplerState sampler, float2 uv,
    float2 pixelSize, float2 offsetPixels ) {
    float2 lower = pixelSize * 0.5f;
    float2 upper = 1.0f - lower;
    float2 offset = pixelSize * offsetPixels;

    float2 redUV = clamp( uv, lower, upper );
    float2 greenUV = clamp( uv - offset, lower, upper );
    float2 blueUV = clamp( uv - 2.0f * offset, lower, upper );

    float4 red = source.SampleLevel( sampler, redUV, 0 );
    float4 green = source.SampleLevel( sampler, greenUV, 0 );
    float4 blue = source.SampleLevel( sampler, blueUV, 0 );
    return float4( red.r, green.g, blue.b, red.a );
}
