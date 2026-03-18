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
#include "base/Camera_Helpers.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    float2 UV : TEXCOORD0;
};

// Ref: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

// This shader performs downsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.
// This particular method was customly designed to eliminate
// "pulsating artifacts and temporal stability issues".

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]]
Sampler2D srcTexture;

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]] 
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float2 texCoord = input.UV;

    float2 srcTexelSize = 1.0 / u_Camera.ScreenDimensions;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = srcTexture.Sample(float2(texCoord.x - 2 * x, texCoord.y + 2 * y)).rgb;
    float3 b = srcTexture.Sample(float2(texCoord.x, texCoord.y + 2 * y)).rgb;
    float3 c = srcTexture.Sample(float2(texCoord.x + 2 * x, texCoord.y + 2 * y)).rgb;

    float3 d = srcTexture.Sample(float2(texCoord.x - 2 * x, texCoord.y)).rgb;
    float3 e = srcTexture.Sample(float2(texCoord.x, texCoord.y)).rgb;
    float3 f = srcTexture.Sample(float2(texCoord.x + 2 * x, texCoord.y)).rgb;

    float3 g = srcTexture.Sample(float2(texCoord.x - 2 * x, texCoord.y - 2 * y)).rgb;
    float3 h = srcTexture.Sample(float2(texCoord.x, texCoord.y - 2 * y)).rgb;
    float3 i = srcTexture.Sample(float2(texCoord.x + 2 * x, texCoord.y - 2 * y)).rgb;

    float3 j = srcTexture.Sample(float2(texCoord.x - x, texCoord.y + y)).rgb;
    float3 k = srcTexture.Sample(float2(texCoord.x + x, texCoord.y + y)).rgb;
    float3 l = srcTexture.Sample(float2(texCoord.x - x, texCoord.y - y)).rgb;
    float3 m = srcTexture.Sample(float2(texCoord.x + x, texCoord.y - y)).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
    o.Color.xyz = e * 0.125;
    o.Color.xyz += (a + c + g + i) * 0.03125;
    o.Color.xyz += (b + d + f + h) * 0.0625;
    o.Color.xyz += (j + k + l + m) * 0.125;

    o.Color = float4(o.Color.xyz, 1.0f);

    return o;
}