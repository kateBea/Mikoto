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

struct IrradianceConstants {
    float4x4 MVP;
    float DeltaPhi;
    float DeltaTheta;
};

struct FSInput {
    float3 Pos : TEXCOORD0;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

[[vk::push_constant]]
IrradianceConstants u_Parameters;

[[vk::binding(0, DYNAMIC_RESOURCE_SET_INDEX)]]
SamplerCube u_SamplerEnv;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float3 N = normalize(input.Pos);
    float3 up = float3(0.0, 1.0, 0.0);

    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    const float twoPi = PI * 2.0;
    const float haldPi = PI * 0.5;

    float3 color = float3(0.0);
    uint sampleCount = 0;

    // Hemisphere sampling for irradiance
    for (float phi = 0.0; phi < twoPi; phi += u_Parameters.DeltaPhi) {
        for (float theta = 0.0; theta < haldPi; theta += u_Parameters.DeltaTheta) {
            float3 tempVec =
                cos(phi) * right +
                sin(phi) * up;

            float3 sampleVector =
                cos(theta) * N +
                sin(theta) * tempVec;

            float weight = cos(theta) * sin(theta);
            float3 sample = u_SamplerEnv.Sample(sampleVector).rgb;

            color += sample * weight;
            sampleCount++;
        }
    }

    o.Color = float4(PI * color / float(sampleCount), 1.0);
    return o;
}