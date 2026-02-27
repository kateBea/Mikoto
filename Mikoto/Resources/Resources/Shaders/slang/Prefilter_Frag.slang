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

#include "base/Base.slang"
#include "base/Lighting_Helpers.slang"

struct PrefilterConstants {
    float4x4 MVP;
    float Roughness;
    uint NumSamples;
};

struct FSInput {
    float3 Pos : TEXCOORD0;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

[[vk::push_constant]]
PrefilterConstants u_Parameters;

[[vk::binding(0, DYNAMIC_RESOURCE_SET_INDEX)]]
SamplerCube u_SamplerEnv;

// Normal distribution
float D_GGX(float dotNH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

// Main filtering function
float3 PrefilterEnvMap(float3 R, float roughness) {
    uint NumSamples = u_Parameters.NumSamples;

    float3 N = R;
    float3 V = R;

    float3 color = float3(0, 0, 0);
    float totalWeight = 0.0;

    float textureWidth;
    float textureHeight;
    float levelCount;
    const uint lod = 0u;
    u_SamplerEnv.GetDimensions(lod, textureWidth, textureHeight, levelCount);
    float envMapDim = float(textureWidth);

    for (uint i = 0; i < NumSamples; i++) {
        float2 Xi = Hammersley2D(i, NumSamples);
        float3 H = ImportanceSample_GGX(Xi, roughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;

        float dotNL = saturate(dot(N, L));
        if (dotNL > 0.0) {
            // Filtering based on 
            // https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
            float dotNH = saturate(dot(N, H));
            float dotVH = saturate(dot(V, H));

            // Probability Distribution Function
            float pdf = D_GGX(dotNH, roughness) * dotNH / (4.0 * dotVH) + 0.0001;

            // Slid angle of current smple
            float omegaS = 1.0 / (float(NumSamples) * pdf);

            // Solid angle of 1 pixel across all cube faces
            float omegaP = 4.0 * PI / (6.0 * envMapDim * envMapDim);

            // Biased (+1.0) mip level for better result
            float mipLevel =
                (roughness == 0.0)
                    ? 0.0
                    : max(0.5 * log2(omegaS / omegaP) + 1.0, 0.0);

            float3 sampleColor = u_SamplerEnv.SampleLevel(L, mipLevel).rgb;

            color += sampleColor * dotNL;
            totalWeight += dotNL;
        }
    }

    return color / totalWeight;
}

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float3 N = normalize(input.Pos);
    float3 result = PrefilterEnvMap(N, u_Parameters.Roughness);

    o.Color = float4(result, 1.0);
    return o;
}