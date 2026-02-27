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
    float3 Position : ATTRIBUTE0;
    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput{
    float4 PositionH : SV_Position;
};

[[vk::binding(0, DYNAMIC_BUFFERS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(1, DYNAMIC_RESOURCE_SET_INDEX)]]
StructuredBuffer<MeshInfo, ScalarDataLayout> Meshes;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    MeshInfo meshInfo = Meshes[input.BaseInstanceIndex + input.InstanceIndex];
    float4x4 model = meshInfo.Transform;

    float4 position = float4(input.Position, 1.0);

    o.PositionH = mul(u_Camera.Projection,
                      mul(u_Camera.ViewMatrix,
                          mul(model, position)));

    return o;
}