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
#include "base/ImageBaseLighting_Helpers.slang"

struct FSInput {
    float3 Direction : TEXCOORD0;
    float  Exposure  : TEXCOORD1;
    float  Gamma     : TEXCOORD2;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

[[vk::binding(1, STATIC_SET_INDEX)]]
SamplerCube<float4> g_SkyboxTex;

[[vk::push_constant]]
SkyBoxParameters u_IBLParams;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    const float blur = 4.5;
    float3 color;

    if (u_IBLParams.MaxMipLevel > blur) {
        color = g_SkyboxTex.SampleLevel(input.Direction, blur).rgb;
    }
    else {
        color = g_SkyboxTex.Sample(input.Direction).rgb;
    }

    // Tone mapping
    color = Uncharted2Tonemap(color * input.Exposure);
    color *= (1.0 / Uncharted2Tonemap(float3(11.2)));

    // Gamma correction
    color = pow(color, float3(1.0 / input.Gamma));

    o.Color = float4(color, 1.0);
    return o;
}