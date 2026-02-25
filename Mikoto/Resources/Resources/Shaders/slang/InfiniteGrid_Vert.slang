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
#include "base/Camera_Helpers.slang"

struct VSInput {
    uint VertexIndex : SV_VertexID;
};

struct VSOutput {
    float3 WorldPos : TEXCOORD0;
    float3 CameraWorldPos : TEXCOORD1;
    float4 PositionH : SV_Position;
};

[[vk::binding(0, DYNAMIC_BUFFERS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

static const float gridSize = 100.0;
static const float3 positions[4] = {
    float3(-1.0, 0.0, -1.0),
    float3(1.0, 0.0, -1.0),
    float3(1.0, 0.0, 1.0),
    float3(-1.0, 0.0, 1.0)
};

static const int Indices[6] = { 0, 2, 1, 2, 0, 3 };

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    int index = Indices[input.VertexIndex];
    float3 worldPos = positions[index] * gridSize;

    worldPos.x += u_Camera.ViewPosition.x;
    worldPos.z += u_Camera.ViewPosition.z;

    float4 vPos4 = float4(worldPos, 1.0);

    o.WorldPos = worldPos;
    o.CameraWorldPos = u_Camera.ViewPosition.xyz;
    o.PositionH = mul(u_Camera.Projection, mul(u_Camera.ViewMatrix, vPos4));

    return o;
}