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

struct ColorParams {
    float Exposure;
    float Contrast;
    float Saturation;
    float3 Tint;
};

float3 ApplySaturation(float3 color, float saturation) {
    float luminance = dot(color, float3(0.299, 0.587, 0.114));
    return lerp(float3(luminance, luminance, luminance), color, saturation);
}

[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]]
Sampler2D u_FinalComposition;

[[vk::push_constant]]
ColorParams u_ColorParams;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float3 color = u_FinalComposition.Sample(input.UV).rgb;

    // Exposure
    color *= u_ColorParams.Exposure;

    // Contrast (centered around 0.5)
    color = (color - 0.5) * u_ColorParams.Contrast + 0.5;

    // Saturation
    color = ApplySaturation(color, u_ColorParams.Saturation);

    // Tint
    color *= u_ColorParams.Tint;

    o.Color = float4(color, 1.0);
    return o;
}