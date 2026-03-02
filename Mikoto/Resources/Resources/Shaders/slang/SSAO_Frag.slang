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

// ============================================================================
// SSAO Fragment Shader (Slang)
// ============================================================================

#include "base/Base.slang"

static const int SSAO_KERNEL_SIZE = 64;
static const float SSAO_RADIUS = 0.5f;

struct SSAOParams {
    float4x4 Projection;
    float4 Samples[SSAO_KERNEL_SIZE];
};

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<SSAOParams, ScalarDataLayout> u_Params;

// GBuffer inputs
[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_samplerPosition;
[[vk::binding(Slot_1, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_samplerNormal;
[[vk::binding(Slot_2, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SsaoNoise;

// FS input/output
struct FSInput {
    float2 UV : TEXCOORD0;
};

struct FSOutput {
    float Occlusion : SV_Target0;
};

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    // Sample G-Buffer values
    float3 fragPos = u_samplerPosition.Sample(input.UV).xyz;

    float3 normalEnc = u_samplerNormal.Sample(input.UV).xyz;
    float3 normal = normalize(normalEnc * 2.0f - 1.0f);

    // Get a random vector using a noise lookup
    float textureWidth;
    float textureHeight;
    float levelCount;
    u_samplerPosition.GetDimensions(0, textureWidth, textureHeight, levelCount);
    uint2 texDim = uint2(uint(textureWidth), uint(textureHeight));

    u_SsaoNoise.GetDimensions(0, textureWidth, textureHeight, levelCount);
    uint2 noiseDim = uint2(uint(textureWidth), uint(textureHeight));

    float2 noiseUV = (float2(texDim) / float2(noiseDim)) * input.UV;
    float3 randomVec = u_SsaoNoise.Sample(noiseUV).xyz * 2.0f - 1.0f;

    // Create TBN matrix
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = normalize(cross(tangent, normal));
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    const float bias = 0.025f;

    // SSAO loop
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        float3 sampleVec = mul(TBN, u_Params.Samples[i].xyz);
        float3 samplePos = fragPos + sampleVec * SSAO_RADIUS;

        // project sample pos to NDC
        float4 offset = mul(u_Params.Projection, float4(samplePos, 1.0f));
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;

        // depth value stored in .w
        float sampleDepth = -u_samplerPosition.SampleLevel(offset.xy, 0).w;

        float rangeCheck = smoothstep(
            0.0f,
            1.0f,
            SSAO_RADIUS / abs(fragPos.z - sampleDepth)
        );

        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0f - (occlusion / float(SSAO_KERNEL_SIZE));
    o.Occlusion = occlusion;
    return o;
}