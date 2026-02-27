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

static const uint MAX_BONES = 100;
static const uint MAX_BONE_INFLUENCE = 4;

struct BoneData {
    float4x4 Bone[MAX_BONES];
};

[[vk::binding(0, DYNAMIC_BUFFERS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(1, DYNAMIC_RESOURCE_SET_INDEX)]]
StructuredBuffer<MeshInfo, ScalarDataLayout> Meshes;

[[vk::binding(7, DYNAMIC_BUFFERS_SET_INDEX)]]
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
    float OcclusionStrength : TEXCOORD13;

    float3 EmissiveFactors : TEXCOORD14;
    float EmissionIntensity : TEXCOORD15;
    int EmissionIndex : TEXCOORD16;
    float AlphaCutoff : TEXCOORD17;

    float2 TexCoord1 : TEXCOORD18;

    float4 PositionH : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    MeshInfo p = Meshes[input.BaseInstanceIndex + input.InstanceIndex];

    float4x4 model = p.Transform;

    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNormal = float3(0, 0, 0);

    if (p.AnimatorID != -1) {
        uint baseIndex = p.AnimatorID;

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

    o.AlbedoIndex = p.AlbedoIndex;
    o.NormalIndex = p.NormalIndex;
    o.MetallicIndex = p.MetallicIndex;
    o.RoughnessIndex = p.RoughnessIndex;
    o.AoIndex = p.AoIndex;

    o.Albedo = p.Albedo;
    o.MetallicFactor = p.MetallicFactor;
    o.RoughnessFactor = p.RoughnessFactor;
    o.OcclusionStrength = p.OcclusionStrength;

    o.EmissiveFactors = p.EmissiveFactors;
    o.EmissionIntensity = p.EmissiveIntensity;
    o.EmissionIndex = p.EmissiveIndex;

    o.AlphaCutoff = p.AlphaCutoff;

    o.PositionH =
        mul(u_Camera.Projection,
        mul(u_Camera.ViewMatrix, worldPos));

    return o;
}