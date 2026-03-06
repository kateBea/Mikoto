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
#include "base/Animation_Helpers.slang"

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(Slot_0, UAV_SET_INDEX)]]
StructuredBuffer<MeshParameters, ScalarDataLayout> Meshes;

[[vk::binding(Slot_2, UAV_SET_INDEX)]]
StructuredBuffer<BoneData, ScalarDataLayout> Bones;

struct VSInput {
    float3 Position : ATTRIBUTE0;
    float3 Normal : ATTRIBUTE1;
    float3 Color : ATTRIBUTE2;
    float2 TexCoord0 : ATTRIBUTE3;
    float2 TexCoord1 : ATTRIBUTE4;

    float4 Joint : ATTRIBUTE5;
    float4 Weight : ATTRIBUTE6;

    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 TexCoord0 : TEXCOORD3;
    float3 Color : TEXCOORD4;

    float2 TexCoord1 : TEXCOORD5;

    int MaterialIndex : TEXCOORD6;

    float4 PositionH : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    MeshParameters meshInfo = Meshes[input.BaseInstanceIndex + input.InstanceIndex];

    float4x4 model = meshInfo.Transform;

    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNormal = float3(0, 0, 0);

    if (meshInfo.AnimatorID != -1) {
        uint baseIndex = meshInfo.AnimatorID;

        for (uint i = 0; i < MAX_BONE_INFLUENCE; i++) {
            int joint = uint(input.Joint[i]);
            float weight = input.Weight[i];

            if (weight <= 0.0 || joint < 0) {
                continue;
            }

            if (joint >= MAX_BONES) {
                // Fallback to rigid
                skinnedPos = float4(input.Position, 1.0);
                skinnedNormal = input.Normal;
                break;
            }

            float4x4 bone = Bones[baseIndex].Bone[joint];

            skinnedPos += mul(bone, float4(input.Position, 1.0)) * weight;
            skinnedNormal += mul((float3x3)bone, input.Normal) * weight;
        }
    }
    else {
        skinnedPos = float4(input.Position, 1.0);
        skinnedNormal = input.Normal;
    }

    float4 worldPos = mul(model, skinnedPos);
    float3 worldNormal = normalize(mul((float3x3)model, skinnedNormal));

    o.TexCoord0 = input.TexCoord0;
    o.TexCoord1 = input.TexCoord1;
    o.Color = input.Color;

    o.FragmentWorldPos = worldPos.xyz;
    o.FragmentViewPos = mul(u_Camera.ViewMatrix, worldPos).xyz;
    o.Normal = worldNormal;
    o.MaterialIndex = input.BaseInstanceIndex + input.InstanceIndex;

    o.PositionH =
        mul(u_Camera.Projection,
        mul(u_Camera.ViewMatrix, worldPos));

    return o;
}