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
#include "base/Memory_Helpers.slang"

static const int SSAO_KERNEL_SIZE = 64;
static const float SSAO_RADIUS = 0.5f;

struct DrawParams {
    uint mSamplerID;

    uint mKernelBufferID;
    uint mNoiseTextureID;
    uint mGBufferNormalsID;
    uint mGBufferPositionsID;

    float4x4 mProjection;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct FSOutput {
    float Occlusion : SV_Target0;
};

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    SamplerState sampler = samplers[parameters.mSamplerID];

    Texture2D normalsTexture = textures[parameters.mGBufferNormalsID];
    Texture2D positionsTexture = textures[parameters.mGBufferPositionsID];
    Texture2D noiseTexture = textures[parameters.mNoiseTextureID];

    // Sample G-Buffer values
    float3 fragPos = positionsTexture.Sample(sampler, input.UV).xyz;

    float3 normalEnc = normalsTexture.Sample(sampler, input.UV).xyz;
    float3 normal = normalize(normalEnc * 2.0f - 1.0f);

    // Get a random vector using a noise lookup
    float textureWidth;
    float textureHeight;
    float levelCount;
    positionsTexture.GetDimensions(0, textureWidth, textureHeight, levelCount);
    uint2 texDim = uint2(uint(textureWidth), uint(textureHeight));

    noiseTexture.GetDimensions(0, textureWidth, textureHeight, levelCount);
    uint2 noiseDim = uint2(uint(textureWidth), uint(textureHeight));

    float2 noiseUV = (float2(texDim) / float2(noiseDim)) * input.UV;
    float3 randomVec = noiseTexture.Sample(sampler, noiseUV).xyz * 2.0f - 1.0f;

    // Create TBN matrix
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = normalize(cross(tangent, normal));
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    const float bias = 0.025f;

    // SSAO loop
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        float4 kernelSample = buffers[parameters.mKernelBufferID].Read<float4>(i, 0);

        float3 sampleVec = mul(TBN, kernelSample.xyz);
        float3 samplePos = fragPos + sampleVec * SSAO_RADIUS;

        // project sample pos to NDC
        float4 offset = mul(parameters.mProjection, float4(samplePos, 1.0f));
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;

        // depth value stored in .w
        float sampleDepth = -positionsTexture.SampleLevel(sampler, offset.xy, 0).w;

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