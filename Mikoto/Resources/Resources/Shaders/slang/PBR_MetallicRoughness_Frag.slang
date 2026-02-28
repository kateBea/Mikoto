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
#include "base/ClusteringLights_Helpers.slang"

struct LightingParameters {
    float4 GridSize;
    float Exposure;
    float Gamma;
    float MaxReflectionLOD;

    int IsSkyboxActive;
};

// Materials
[[vk::binding(0, TEXTURES_SET_INDEX)]] Sampler2D g_BindlessTextures[];
[[vk::binding(0, PER_MATERIAL_SET_INDEX)]] StructuredBuffer<MaterialParameters, ScalarDataLayout> Materials;

// Clustering and lighting resources
[[vk::binding(0, DYNAMIC_BUFFERS_SET_INDEX)]] ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;
[[vk::binding(3, DYNAMIC_BUFFERS_SET_INDEX)]] StructuredBuffer<LightInfo, ScalarDataLayout> Lights;
[[vk::binding(2, DYNAMIC_RESOURCE_SET_INDEX)]] StructuredBuffer<Cluster, ScalarDataLayout> Clusters;

// IBL resources
[[vk::binding(4, DYNAMIC_RESOURCE_SET_INDEX)]] Sampler2D u_SamplerBRDFLUT;
[[vk::binding(5, DYNAMIC_RESOURCE_SET_INDEX)]] SamplerCube u_PrefilteredMap;
[[vk::binding(6, DYNAMIC_RESOURCE_SET_INDEX)]] SamplerCube u_SamplerIrradiance;

[[vk::push_constant]]
LightingParameters u_Constants;

struct FSInput {
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 TexCoord0 : TEXCOORD3;
    float2 TexCoord1 : TEXCOORD4;
    float3 VertexColor : TEXCOORD5;

    uint MaterialIndex : TEXCOORD6;

    float4 ScreenPos : SV_Position;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput output;

    MaterialParameters material = Materials[input.MaterialIndex];

    float4 baseColor = float4(0.0, 0.0, 0.0, 0.0);
    float3 diffuseColor = float3(0.0, 0.0, 0.0);
    float perceptualRoughness = 0.0;
    float metallic = 0.0f;

    float3 f0 = float3(0.04);

    if (material.AlphaMask == PBR_ALPHA_MASK) {
        if (material.BaseColorTextureSet != INVALID_UV_SET) {
            Sampler2D colorMap = g_BindlessTextures[material.AlbedoIndex];
            baseColor = SRGBtoLinear(colorMap.Sample(material.BaseColorTextureSet == 0 ? 
                input.TexCoord0 : input.TexCoord1)) * material.BaseColorFactor;
        } else {
            baseColor = material.BaseColorFactor;
        }
        if (baseColor.a < material.AlphaMaskCutoff) {
            discard;
        }
    }

    if (material.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS) {
        // Metallic and Roughness material properties are packed together
        // In glTF, these factors can be specified by fixed scalar values
        // or from a metallic-roughness map
        perceptualRoughness = material.RoughnessFactor;
        metallic = material.MetallicFactor;

        if (material.PhysicalDescriptorTextureSet > -1) {
            // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
            // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
            Sampler2D physicalDescriptorMap = g_BindlessTextures[material.PhysicalDescriptorTextureSet];
            float4 mrSample = physicalDescriptorMap.Sample(material.PhysicalDescriptorTextureSet == 0 ? 
                input.TexCoord0 : input.TexCoord1);
            perceptualRoughness = mrSample.g * perceptualRoughness;
            metallic = mrSample.b * metallic;
        } else {
            perceptualRoughness = clamp(perceptualRoughness, MIN_ROUGHNESS, 1.0);
            metallic = clamp(metallic, 0.0, 1.0);
        }
        // Roughness is authored as perceptual roughness; as is convention,
        // convert to material roughness by squaring the perceptual roughness [2].

        // The albedo may be defined from a base texture or a flat color
        if (material.BaseColorTextureSet != INVALID_UV_SET) {
            Sampler2D colorMap = g_BindlessTextures[material.AlbedoIndex];
            baseColor = SRGBtoLinear(colorMap.Sample(material.BaseColorTextureSet == 0 ? 
                input.TexCoord0 : input.TexCoord1)) * material.BaseColorFactor;
        } else {
            baseColor = material.BaseColorFactor;
        }
    }

    if (material.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS) {
        // Values from specular glossiness workflow are converted to metallic roughness
        if (material.PhysicalDescriptorTextureSet > -1) {
            Sampler2D physicalDescriptorMap = g_BindlessTextures[material.PhysicalDescriptorTextureIndex];
            perceptualRoughness = 1.0 - physicalDescriptorMap.Sample(material.PhysicalDescriptorTextureSet == 0 ? 
                input.TexCoord0 : input.TexCoord1).a;
        } else {
            perceptualRoughness = 0.0;
        }

        const float epsilon = 1e-6;

        Sampler2D colorMap = g_BindlessTextures[material.AlbedoIndex];
        Sampler2D physicalDescriptorMap = g_BindlessTextures[material.PhysicalDescriptorTextureSet];

        float4 diffuse = SRGBtoLinear(colorMap.Sample(input.TexCoord0));
        float3 specular = SRGBtoLinear(physicalDescriptorMap.Sample(input.TexCoord0)).rgb;

        float maxSpecular = max(max(specular.r, specular.g), specular.b);

        // Convert metallic value from specular glossiness inputs
        metallic = ConvertMetallic(diffuse.rgb, specular, maxSpecular);

        float3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - MIN_ROUGHNESS) / max(1 - metallic, epsilon)) * material.DiffuseFactor.rgb;
        float3 baseColorSpecularPart = specular - (float3(MIN_ROUGHNESS) * (1 - metallic) * (1 / max(metallic, epsilon))) * material.SpecularFactor.rgb;
        baseColor = float4(lerp(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);
    }

    baseColor *= float4(input.VertexColor, 1.0);

    diffuseColor = baseColor.rgb * (float3(1.0) - f0);
    diffuseColor *= 1.0 - metallic;

    float alphaRoughness = perceptualRoughness * perceptualRoughness;

    float3 specularColor = lerp(f0, baseColor.rgb, metallic);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    float3 specularEnvironmentR0 = specularColor.rgb;
    float3 specularEnvironmentR90 = float3(1.0, 1.0, 1.0) * reflectance90;

    //float3 n = (material.NormalTextureSet != INVALID_UV_SET) ? GetNormalFromMap(input.TexCoord, input.FragmentWorldPos, input.Normal) : normalize(input.Normal);
    //n.y *= -1.0f;

    //float3 v = normalize(ubo.camPos - inWorldPos);  // Vector from surface point to camera
    //float3 l = normalize(uboParams.lightDir.xyz);   // Vector from surface point to light
    //float3 h = normalize(l + v);                    // Half vector between both l and v

    //float3 reflection = normalize(reflect(-v, n));

    //float NdotL = clamp(dot(n, l), 0.001, 1.0);
    //float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    //float NdotH = clamp(dot(n, h), 0.0, 1.0);
    //float LdotH = clamp(dot(l, h), 0.0, 1.0);
    //float VdotH = clamp(dot(v, h), 0.0, 1.0);

    return output;

}