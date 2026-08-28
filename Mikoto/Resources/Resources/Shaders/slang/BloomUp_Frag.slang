//    Copyright 2025 ケイト
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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    float2 UV : TEXCOORD0;
};

// Ref: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

// This shader performs upsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
struct DrawParams {
    uint mSamplerID;
    uint mTargetImageID;
};

MKT_DECLARE_SAMPLERS samplers;

MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    SamplerState sampler = samplers[parameters.mSamplerID];
    Texture2D srcTexture = textures[parameters.mTargetImageID];

    float filterRadius = 0.005f;
    float2 texCoord = input.UV;

    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = filterRadius;
    float y = filterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = srcTexture.Sample(sampler, float2(texCoord.x - x, texCoord.y + y)).rgb;
    float3 b = srcTexture.Sample(sampler, float2(texCoord.x, texCoord.y + y)).rgb;
    float3 c = srcTexture.Sample(sampler, float2(texCoord.x + x, texCoord.y + y)).rgb;

    float3 d = srcTexture.Sample(sampler, float2(texCoord.x - x, texCoord.y)).rgb;
    float3 e = srcTexture.Sample(sampler, float2(texCoord.x, texCoord.y)).rgb;
    float3 f = srcTexture.Sample(sampler, float2(texCoord.x + x, texCoord.y)).rgb;

    float3 g = srcTexture.Sample(sampler, float2(texCoord.x - x, texCoord.y - y)).rgb;
    float3 h = srcTexture.Sample(sampler, float2(texCoord.x, texCoord.y - y)).rgb;
    float3 i = srcTexture.Sample(sampler, float2(texCoord.x + x, texCoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    o.Color.xyz = e * 4.0;
    o.Color.xyz += (b + d + f + h) * 2.0;
    o.Color.xyz += (a + c + g + i);
    o.Color.xyz *= 1.0 / 16.0;

    o.Color = float4(o.Color.xyz, 1.0f);

    return o;
}