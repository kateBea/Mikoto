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

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(Slot_0, UAV_SET_INDEX)]]
StructuredBuffer<MeshParameters, ScalarDataLayout> Meshes;

[[vk::binding(Slot_1, UAV_SET_INDEX)]]
StructuredBuffer<MaterialParameters, ScalarDataLayout> Materials;

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

    int AlbedoIndex : TEXCOORD5;
    int NormalIndex : TEXCOORD6;
    int MetallicIndex : TEXCOORD7;
    int RoughnessIndex : TEXCOORD8;
    int AoIndex : TEXCOORD9;
    float4 Albedo : TEXCOORD10;

    float MetallicFactor : TEXCOORD11;
    float RoughnessFactor : TEXCOORD12;

    float3 EmissiveFactors : TEXCOORD13;
    float EmissionIntensity : TEXCOORD14;
    int EmissionIndex : TEXCOORD15;
    float AlphaCutoff : TEXCOORD16;

    float2 TexCoord1 : TEXCOORD17;

    float4 PositionH : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    MeshParameters meshInfo = Meshes[input.BaseInstanceIndex + input.InstanceIndex];
    MaterialParameters meshMaterial = Materials[input.BaseInstanceIndex + input.InstanceIndex];

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

    o.AlbedoIndex = meshMaterial.AlbedoIndex;
    o.NormalIndex = meshMaterial.NormalIndex;
    o.MetallicIndex = meshMaterial.MetallicIndex;
    o.RoughnessIndex = meshMaterial.RoughnessIndex;
    o.AoIndex = meshMaterial.AoIndex;

    o.Albedo = meshMaterial.BaseColorFactor;
    o.MetallicFactor = meshMaterial.MetallicFactor;
    o.RoughnessFactor = meshMaterial.RoughnessFactor;

    o.EmissiveFactors = float3(0, 0, 0); //  // TODO
    o.EmissionIntensity = meshMaterial.EmissiveStrength;
    o.EmissionIndex = meshMaterial.EmissiveIndex;

    o.AlphaCutoff = meshMaterial.AlphaMaskCutoff;

    o.PositionH =
        mul(u_Camera.Projection,
        mul(u_Camera.ViewMatrix, worldPos));

    return o;
}