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
#include "base/Math_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct DrawParams {
    float4x4 MVP;
    float DeltaPhi;
    float DeltaTheta;

    uint mCubeID;
    uint mSamplerID;
};

struct FSInput {
    float3 Pos : TEXCOORD0;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

MKT_DECLARE_TEXTURE_SRV(TextureCube) textures;
MKT_DECLARE_SAMPLERS samplers;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float3 N = normalize(input.Pos);
    float3 up = float3(0.0, 1.0, 0.0);

    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    const float twoPi = PI_Value * 2.0;
    const float haldPi = PI_Value * 0.5;

    float3 color = float3(0.0);
    uint sampleCount = 0;

    // Hemisphere sampling for irradiance
    for (float phi = 0.0; phi < twoPi; phi += parameters.DeltaPhi) {
        for (float theta = 0.0; theta < haldPi; theta += parameters.DeltaTheta) {
            float3 tempVec =
                cos(phi) * right +
                sin(phi) * up;

            float3 sampleVector =
                cos(theta) * N +
                sin(theta) * tempVec;

            float weight = cos(theta) * sin(theta);
            float3 sample = textures[parameters.mCubeID].Sample(samplers[parameters.mSamplerID], sampleVector).rgb;

            color += sample * weight;
            sampleCount++;
        }
    }

    o.Color = float4(PI_Value * color / float(sampleCount), 1.0);
    return o;
}