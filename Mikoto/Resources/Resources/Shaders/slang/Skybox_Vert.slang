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
#include "base/Lighting_Helpers.slang"

struct VSInput {
    float3 Position : ATTRIBUTE0;
};

struct VSOutput {
    float3 Direction : TEXCOORD0;
    float  Exposure  : TEXCOORD1;
    float  Gamma     : TEXCOORD2;

    float4 PositionH : SV_Position;
};

[[vk::binding(0, DYNAMIC_BUFFERS_SET_INDEX)]]
ConstantBuffer<CameraInfo> u_Camera;

[[vk::push_constant]]
SkyBoxParameters u_IBLParams;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    o.Exposure = u_IBLParams.Exposure;
    o.Gamma    = u_IBLParams.Gamma;

    float3 pos = input.Position;

    // Remove translation from view matrix
    float4x4 viewNoTrans = RemoveTranslation(u_Camera.ViewMatrix);

    // Skybox direction
    o.Direction = pos;

    // Project to clip space
    float4 clip = mul(u_Camera.Projection, mul(viewNoTrans, float4(pos, 1.0)));
    o.PositionH = clip;

    return o;
}
