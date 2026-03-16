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
#include "base/Lighting_Helpers.slang"

struct FSInput {
    float3 ViewPos : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 Uv0 : TEXCOORD3;
    float2 Uv1 : TEXCOORD4;
    float4 Color : TEXCOORD5;

    uint MaterialIndex : TEXTCOORD6;

    float4 ScreenPos : SV_Position;
};

struct FSOutput {
    float4 Position : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Color : SV_Target2;
    float4 Emissive : SV_Target4;
};

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]] ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;
[[vk::binding(Slot_0, UNBOUNDED_IV_SAMPLERS_SET_INDEX)]] Sampler2D g_BindlessTextures[];

[[vk::binding(Slot_1, UAV_SET_INDEX)]] StructuredBuffer<MaterialParameters, ScalarDataLayout> Materials;

[shader("fragment")]
FSOutput main(FSInput input)
{
    FSOutput o;

    MaterialParameters materialInfo = Materials[input.MaterialIndex];

    // NORMAL
    float2 normalUvs = materialInfo.NormalTextureSet == 0 ? input.Uv0 : input.Uv1;
    float3 N = (materialInfo.NormalIndex != INVALID_TEXTURE_INDEX)
                   ? GetNormalFromMap(g_BindlessTextures[materialInfo.NormalIndex],
                           normalUvs,
                           input.Normal,
                           input.WorldPos)
                   : normalize(input.Normal);

    o.Normal = float4(normalize(N) * 0.5 + 0.5, 1.0);

    // POSITION (view-space + depth)
    o.Position = normalize(float4(input.ViewPos, LinearDepth(input.ScreenPos.z, u_Camera.Planes)));

    // ALBEDO
    if (materialInfo.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS) {
        float2 baseColorUvs = materialInfo.BaseColorTextureSet == 0 ? input.Uv0 : input.Uv1;

        float4 albedo =
            (materialInfo.AlbedoIndex != INVALID_TEXTURE_INDEX)
                ? pow(g_BindlessTextures[materialInfo.AlbedoIndex].Sample(baseColorUvs), float4(2.2))
                : materialInfo.BaseColorFactor;

        o.Color = float4(albedo.xyz, 1.0);
    }

    if (materialInfo.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS) {
        float2 diffuseUvs = materialInfo.BaseColorTextureSet == 0 ? input.Uv0 : input.Uv1;

        float4 diffuse =
            (materialInfo.DiffuseIndex != INVALID_TEXTURE_INDEX)
                ? pow(g_BindlessTextures[materialInfo.DiffuseIndex].Sample(diffuseUvs), float4(2.2))
                : materialInfo.DiffuseFactor;

        o.Color = float4(diffuse.xyz, 1.0);
    }

    // Emisive
    o.Emissive = float4(0, 0, 0, 0);
    if (materialInfo.IsBloomy == MKT_SHADER_TRUE) {
        o.Emissive = o.Color;                
    }

    return o;
}
