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

const static uint NUM_SAMPLES = 1024;

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
    float k = (roughness * roughness) * 0.5;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

float2 BRDF(float NoV, float roughness) {
    // Normal always points along z-axis for the 2D lookup
    const float3 N = float3(0, 0, 1);
    float3 V = float3(sqrt(1.0 - NoV * NoV), 0, NoV);

    float2 LUT = float2(0, 0);
    for (uint i = 0; i < NUM_SAMPLES; i++) {
        float2 Xi = Hammersley2D(i, NUM_SAMPLES);
        float3 H = ImportanceSample_GGX(Xi, roughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;

        float dotNL = max(dot(N, L), 0.0);
        float dotNV = max(dot(N, V), 0.0);
        float dotVH = max(dot(V, H), 0.0);
        float dotNH = max(dot(N, H), 0.0);

        if (dotNL > 0.0) {
            float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
            float G_Vis = (G * dotVH) / (dotNH * dotNV);
            float Fc = pow(1.0 - dotVH, 5.0);

            LUT += float2((1.0 - Fc) * G_Vis, Fc * G_Vis);
        }
    }

    return LUT / float(NUM_SAMPLES);
}

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;
    o.Color = float4(BRDF(input.UV.x, input.UV.y), 0.0, 1.0);
    return o;
}