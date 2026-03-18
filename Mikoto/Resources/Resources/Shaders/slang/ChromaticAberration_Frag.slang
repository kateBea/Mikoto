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

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

// Credits:
// Created by moonlightoctopus in 2025-05-25
// https://www.shadertoy.com/view/tcKGWw

float4 mainImage(float2 uv) {
    float distortionAmount = 3.0;
    float2 resolution = u_Camera.ScreenDimensions;

    float2 pixelSize = 1.0 / resolution;

    float3 col1 = u_FinalComposition.Sample(uv).xyz;
    float3 col2 = u_FinalComposition.Sample(uv - pixelSize * distortionAmount).xyz;
    float3 col3 = u_FinalComposition.Sample(uv - pixelSize * 2.0 * distortionAmount).xyz;

    float3 col = float3(col1.x, col2.y, col3.z);
    return float4(col, 1.0);
}

[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]]
Sampler2D u_FinalComposition;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    o.Color = mainImage(input.UV);
    return o;
}