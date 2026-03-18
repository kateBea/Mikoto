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
#include "base/Lighting_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

struct FinalShadingParameters {
    float4 GridSize;
    float Exposure;
    float Gamma;
    float MaxReflectionLOD;

    int EnableSSAO;
    int UseBlurred;
    float SSAOIntensity;

    int PrefilteredCubeMipLevels;
    float ScaleIBLAmbient;
    int Step; // for debug

    int IsSkyboxActive;
};

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(Slot_0, UNBOUNDED_IV_SAMPLERS_SET_INDEX)]]
Sampler2D g_BindlessTextures[];

[[vk::binding(Slot_3, UAV_SET_INDEX)]] StructuredBuffer<Cluster, ScalarDataLayout> Clusters;
[[vk::binding(Slot_4, UAV_SET_INDEX)]] StructuredBuffer<LightInfo, ScalarDataLayout> Lights;

[[vk::binding(Slot_1, UAV_SET_INDEX)]] StructuredBuffer<MaterialParameters, ScalarDataLayout> Materials;

[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SamplerBRDFLUT;
[[vk::binding(Slot_1, STATIC_SAMPLERS_SET_INDEX)]] SamplerCube u_PrefilteredMap;
[[vk::binding(Slot_2, STATIC_SAMPLERS_SET_INDEX)]] SamplerCube u_SamplerIrradiance;

[[vk::binding(Slot_3, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SSAO;
[[vk::binding(Slot_4, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SSAOBlur;

[[vk::push_constant]]
FinalShadingParameters u_Constants;

struct FSInput {
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 TexCoord : TEXCOORD3;
    float3 VertexColor : TEXCOORD4;
    float2 TexCoord1 : TEXCOORD5;
    int MaterialIndex : TEXCOORD5;

    float4 ScreenPos : SV_Position;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    MaterialParameters material = Materials[input.MaterialIndex];

    float4 albedo = (material.AlbedoIndex != INVALID_TEXTURE_INDEX)
                        ? pow(g_BindlessTextures[material.AlbedoIndex].Sample(input.TexCoord), float4(2.2))
                        : material.BaseColorFactor;

    float metallic = (material.MetallicIndex != INVALID_TEXTURE_INDEX)
                         ? g_BindlessTextures[material.MetallicIndex].Sample(input.TexCoord).r
                         : material.MetallicFactor;

    float roughness = (material.RoughnessIndex != INVALID_TEXTURE_INDEX)
                          ? g_BindlessTextures[material.RoughnessIndex].Sample(input.TexCoord).r
                          : material.RoughnessFactor;

    float3 N = (material.NormalIndex != INVALID_TEXTURE_INDEX)
                   ? GetNormalFromMap(g_BindlessTextures[material.NormalIndex], input.TexCoord, input.FragmentWorldPos, input.Normal)
                                         : normalize(input.Normal);

    float3 V = normalize(u_Camera.ViewPosition.xyz - input.FragmentWorldPos);
    float3 F0 = lerp(float3(0.04), albedo.xyz, metallic);

    uint zTile =
        uint((log(abs(input.FragmentViewPos.z) / u_Camera.Planes.y) *
        u_Constants.GridSize.z) / log(u_Camera.Planes.x / u_Camera.Planes.y));

    float2 tileSize = u_Camera.ScreenDimensions / u_Constants.GridSize.xy;

    uint xTile = uint(input.ScreenPos.x / tileSize.x);
    uint yTile = uint(input.ScreenPos.y / tileSize.y);

    uint tileIndex = xTile +
                     yTile * uint(u_Constants.GridSize.x) +
                     zTile * uint(u_Constants.GridSize.x * u_Constants.GridSize.y);

    float3 Lo = float3(0, 0, 0);

    if (tileIndex < Clusters.getCount()) {
        uint lightCount = Clusters[tileIndex].Count;

        for (uint i = 0; i < lightCount; i++) {
            uint idx = Clusters[tileIndex].LightIndices[i];
            LightInfo L = Lights[idx];

            switch (L.ActiveLightType) {
            case LIGHT_TYPE_POINT:
                Lo += ComputePointLight(N, V, F0, roughness, metallic, albedo.xyz, L, input.FragmentWorldPos);
                break;

            case LIGHT_TYPE_SPOT:
                Lo += ComputeSpotLight(N, V, F0, roughness, metallic, albedo.xyz, L, input.FragmentWorldPos);
                break;

            case LIGHT_TYPE_DIRECTIONAL:
                Lo += ComputeDirectionalLight(N, V, F0, roughness, metallic, albedo.xyz, L);
                break;
            }
        }
    }

    float3 R = reflect(-V, N);

    float2 brdf = u_SamplerBRDFLUT.Sample(float2(max(dot(N, V), 0.0), roughness)).rg;

    float3 reflection = float3(0, 0, 0);
    float3 irradiance = float3(0, 0, 0);

    float3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

    if (u_Constants.IsSkyboxActive == 1) {
        reflection = PrefilteredReflection(u_PrefilteredMap, R, roughness, u_Constants.MaxReflectionLOD);
        irradiance = u_SamplerIrradiance.Sample(N).rgb;
    }

    // SSAO
    float ssao = 1.0f;
    
    if (u_Constants.EnableSSAO != MKT_SHADER_FALSE) {
        float2 uv = input.ScreenPos.xy / u_Camera.ScreenDimensions;

        ssao = (material.AoIndex != INVALID_TEXTURE_INDEX)
                   ? g_BindlessTextures[material.AoIndex].Sample(input.TexCoord).r
            : (u_Constants.UseBlurred == MKT_SHADER_TRUE ? u_SSAOBlur.Sample(uv).r : u_SSAO.Sample(uv).r);

        float ssaoStrength = u_Constants.SSAOIntensity;
        ssao = pow(ssao, ssaoStrength);
    }
    
    float3 diffuse = irradiance * albedo.xyz;
    float3 specular = reflection * (F * brdf.x + brdf.y);

    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 ambient = (kD * diffuse + specular) * float3(ssao, ssao, ssao);

    float3 color = ambient + Lo;

    // Tone Mapping
    color = Uncharted2Tonemap(color * u_Constants.Exposure);
    color *= (1.0 / Uncharted2Tonemap(float3(11.2)));

    // Gamma Correction
    color = pow(color, float3(1.0 / u_Constants.Gamma));

    // Emissive
    float3 emissive = float3(0, 0, 0);
    if (material.EmissiveIndex != INVALID_TEXTURE_INDEX) {
        Sampler2D emissiveMap = g_BindlessTextures[material.EmissiveIndex];
        emissive = CalculateEmissive(material.EmissiveFactor.xyz, material.EmissiveStrength, material.EmissiveIndex, emissiveMap, input.TexCoord);
    } else {
        emissive = CalculateEmissive(material.EmissiveFactor.xyz, material.EmissiveStrength, material.EmissiveIndex, input.TexCoord);
    }

    float alpha = material.AlphaMaskCutoff * albedo.a;
    if (material.AlbedoIndex == INVALID_TEXTURE_INDEX) {
        alpha = material.AlphaMaskCutoff;
    }

    o.Color = float4(color + emissive, alpha);
    return o;
}