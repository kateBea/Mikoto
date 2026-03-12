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
#include "base/Material_Helpers.slang"
#include "base/Animation_Helpers.slang"

struct DirectionalShadowMapUBO {
    float4x4 LightView;
    float4x4 LightProjection;
};

struct VSInput {
    float3 Position : ATTRIBUTE0;
    float3 Normal : ATTRIBUTE1;
    float4 Color : ATTRIBUTE2;
    float2 TexCoord0 : ATTRIBUTE3;
    float2 TexCoord1 : ATTRIBUTE4;

    float4 Joint : ATTRIBUTE5;
    float4 Weight : ATTRIBUTE6;

    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    
    float4 PositionH : SV_Position;
};

[[vk::push_constant]]
DirectionalShadowMapUBO u_CamerasInfo;

[[vk::binding(Slot_0, UAV_SET_INDEX)]] StructuredBuffer<MeshParameters, ScalarDataLayout> Meshes;
[[vk::binding(Slot_1, UAV_SET_INDEX)]] StructuredBuffer<BoneData, ScalarDataLayout> Bones;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    MeshParameters meshInfo = Meshes[input.BaseInstanceIndex + input.InstanceIndex];

    float4x4 model = meshInfo.Transform;

    float4 inWeight0 = input.Weight;
    float4 inJoint0 = input.Joint;

    float4 locPos = float4(0, 0, 0, 0);
    if (meshInfo.AnimatorID != -1) {
        uint animatorIndex = meshInfo.AnimatorID - 1;

        BoneData bonePalette = Bones[animatorIndex];

        float4x4 skinMat =
            inWeight0.x * bonePalette.BoneMatrices[(uint)inJoint0.x] +
            inWeight0.y * bonePalette.BoneMatrices[(uint)inJoint0.y] +
            inWeight0.z * bonePalette.BoneMatrices[(uint)inJoint0.z] +
            inWeight0.w * bonePalette.BoneMatrices[(uint)inJoint0.w];

        locPos = mul(skinMat, float4(input.Position, 1.0));
    }
    else {
        locPos = float4(input.Position, 1.0);
    }

    float4 worldPos = mul(model, locPos);

    o.PositionH =
        mul(u_CamerasInfo.LightProjection,
            mul(u_CamerasInfo.LightView,
                worldPos));

    return o;
}