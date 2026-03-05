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
#include "base/Material_Helpers.slang"

struct VSInput {
    float3 Position : ATTRIBUTE0;
    uint InstanceID : SV_InstanceID;
};

struct VSOutput {
    float4 PositionH : SV_Position;
};

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(Slot_0, UAV_SET_INDEX)]]
StructuredBuffer<MeshParameters, ScalarDataLayout> Meshes;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    MeshParameters mesh = Meshes[input.InstanceID];

    float4x4 model = mesh.Transform;

    o.PositionH =
        mul(u_Camera.Projection,
        mul(u_Camera.ViewMatrix,
        mul(model, float4(input.Position, 1.0))));

    return o;
}