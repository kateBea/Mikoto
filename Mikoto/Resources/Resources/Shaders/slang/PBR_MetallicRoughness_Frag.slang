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
#include "base/Math_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"

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

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo
{
    float NdotL;               // cos angle between normal and light direction
    float NdotV;               // cos angle between normal and view direction
    float NdotH;               // cos angle between normal and half vector
    float LdotH;               // cos angle between light direction and half vector
    float VdotH;               // cos angle between view direction and half vector

    float perceptualRoughness; // roughness value, as authored by the model creator (input to shader)
    float metalness;           // metallic value at the surface
    float3 reflectance0;       // full reflectance color (normal incidence angle)
    float3 reflectance90;        // reflectance color at grazing angle
    float alphaRoughness;        // roughness mapped to a more linear change in the roughness (proposed by [2])
    float3 diffuseColor;         // color contribution from diffuse lighting
    float3 specularColor;        // color contribution from specular lighting
};

struct BaseInputs {
    float perceptualRoughness;
    float metallic;
    float3 specularEnvironmentR0;
    float3 specularEnvironmentR90;
    float alphaRoughness;
    float3 diffuseColor;
    float3 specularColor;
};

// Camera
[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

// Materials
[[vk::binding(Slot_0, UNBOUNDED_IV_SAMPLERS_SET_INDEX)]] Sampler2D g_BindlessTextures[];

[[vk::binding(Slot_2, UAV_SET_INDEX)]] StructuredBuffer<MaterialParameters, ScalarDataLayout> Materials;

// Lighting
[[vk::binding(Slot_3, UAV_SET_INDEX)]] StructuredBuffer<Cluster, ScalarDataLayout> Clusters;
[[vk::binding(Slot_4, UAV_SET_INDEX)]] StructuredBuffer<LightInfo, ScalarDataLayout> Lights;

// IBL
[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SamplerBRDFLUT;
[[vk::binding(Slot_1, STATIC_SAMPLERS_SET_INDEX)]] SamplerCube u_PrefilteredMap;
[[vk::binding(Slot_2, STATIC_SAMPLERS_SET_INDEX)]] SamplerCube u_SamplerIrradiance;

// SSAO
//[[vk::binding(Slot_3, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SSAO;
//[[vk::binding(Slot_4, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SSAOBlur;

[[vk::push_constant]]
FinalShadingParameters u_Constants;

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

float4 Tonemap(float4 color) {
    float3 outcol = Uncharted2Tonemap(color.rgb * u_Constants.Exposure);
    outcol = outcol * (1.0f / Uncharted2Tonemap(float3(11.2f)));
    return float4(pow(outcol, float3(1.0f / u_Constants.Gamma)), color.a);
}

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
float3 GetIBLContribution(float NdotV, float3 diffuseColor, float perceptualRoughness, float3 n, float3 reflection) {
    float lod = (perceptualRoughness * u_Constants.PrefilteredCubeMipLevels);
    // retrieve a scale and bias to F0. See [1], Figure 3
    float3 brdf = u_SamplerBRDFLUT.Sample(float2(NdotV, 1.0 - perceptualRoughness)).rgb;
    float3 diffuseLight = SRGBtoLinear(Tonemap(u_SamplerIrradiance.Sample(n))).rgb;

    float3 specularLight = SRGBtoLinear(Tonemap(u_PrefilteredMap.SampleLevel(reflection, lod))).rgb;

    float3 diffuse = diffuseLight * diffuseColor;
    float3 specular = specularLight * (diffuseColor * brdf.x + brdf.y);

    // For presentation, this allows us to disable IBL terms
    // For presentation, this allows us to disable IBL terms
    diffuse *= u_Constants.ScaleIBLAmbient;
    specular *= u_Constants.ScaleIBLAmbient;

    return diffuse + specular;
}

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
float3 Diffuse(PBRInfo pbrInputs) {
    return pbrInputs.diffuseColor / PI_Value;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
float3 SpecularReflection(PBRInfo pbrInputs) {
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float GeometricOcclusion(PBRInfo pbrInputs) {
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float MicrofacetDistribution(PBRInfo pbrInputs) {
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI_Value * f * f);
}

float3 ComputeDirectionalLightContribution(float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs) {
    float3 l = normalize(lightInfo.Direction.xyz); // Vector from surface point to light
    float3 h = normalize(l + v);                     // Half vector between both l and v

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(
		NdotL,
		NdotV,
		NdotH,
		LdotH,
		VdotH,
		inputs.perceptualRoughness,
		inputs.metallic,
		inputs.specularEnvironmentR0,
        inputs.specularEnvironmentR90,
		inputs.alphaRoughness,
		inputs.diffuseColor,
		inputs.specularColor
	);

    // Calculate the shading terms for the microfacet specular shading model
    float3 F = SpecularReflection(pbrInputs);
    float G = GeometricOcclusion(pbrInputs);
    float D = MicrofacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution
    float3 diffuseContrib = (1.0 - F) * Diffuse(pbrInputs);
    float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float3 color = NdotL * lightInfo.Diffuse * (diffuseContrib + specContrib);

    return color;
}

struct FSOutput {
    float4 Color : SV_Target0;
};

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput output;

    MaterialParameters material = Materials[input.MaterialIndex];

    float4 baseColor = float4(0.4, 0.2, 0.4, 0.0);

    if (material.AlphaMask == PBR_ALPHA_MASK) {
        if (material.AlbedoIndex != INVALID_UV_SET) {
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

    float metallic = 0.0f;
    float perceptualRoughness = 0.0;

    float3 f0 = float3(0.04);
    float3 diffuseColor = float3(0.0, 0.0, 0.0);


    if (material.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS) {
        // Metallic and Roughness material properties are packed together
        // In glTF, these factors can be specified by fixed scalar values
        // or from a metallic-roughness map
        perceptualRoughness = material.RoughnessFactor;
        metallic = material.MetallicFactor;

        if (material.MetallicRoughnessIndex != INVALID_TEXTURE_INDEX) {
            // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
            // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
            Sampler2D metallicRoughnessMap = g_BindlessTextures[material.MetallicRoughnessIndex];
            float4 mrSample = metallicRoughnessMap.Sample(material.MetallicRoughnessTextureSet == 0 ? input.TexCoord0 : input.TexCoord1);

            perceptualRoughness = mrSample.g * perceptualRoughness;
            metallic = mrSample.b * metallic;
        } else {
            perceptualRoughness = clamp(perceptualRoughness, MIN_ROUGHNESS, 1.0);
            metallic = clamp(metallic, 0.0, 1.0);
        }
        // Roughness is authored as perceptual roughness; as is convention,
        // convert to material roughness by squaring the perceptual roughness [2].

        // The albedo may be defined from a base texture or a flat color
        if (material.AlbedoIndex != INVALID_UV_SET) {
            Sampler2D colorMap = g_BindlessTextures[material.AlbedoIndex];
            baseColor = SRGBtoLinear(colorMap.Sample(material.BaseColorTextureSet == 0 ? input.TexCoord0 : input.TexCoord1)) * material.BaseColorFactor;
        } else {
            baseColor = material.BaseColorFactor;
        }
    }

    if (material.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS) {
        // Values from specular glossiness workflow are converted to metallic roughness
        if (material.SpecularGlossinessIndex != INVALID_TEXTURE_INDEX) {
            Sampler2D specularGlossinessMap = g_BindlessTextures[material.SpecularGlossinessIndex];
            perceptualRoughness = 1.0 - specularGlossinessMap.Sample(material.SpecularGlossinessSet == 0 ? input.TexCoord0 : input.TexCoord1).a;
        } else {
            perceptualRoughness = 0.0;
        }

        const float epsilon = 1e-6;

        // Diffuse index because specular glossiness
        Sampler2D colorMap = g_BindlessTextures[material.DiffuseIndex];
        Sampler2D specularGlossinessMap = g_BindlessTextures[material.SpecularGlossinessIndex];

        float4 diffuse = SRGBtoLinear(colorMap.Sample(input.TexCoord0));
        float3 specular = SRGBtoLinear(specularGlossinessMap.Sample(input.TexCoord0)).rgb;

        float maxSpecular = max(max(specular.r, specular.g), specular.b);

        // Convert metallic value from specular glossiness inputs
        metallic = ConvertMetallic(diffuse.rgb, specular, maxSpecular);

        float3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - MIN_ROUGHNESS) / max(1 - metallic, epsilon)) * material.DiffuseFactor.rgb;
        float3 baseColorSpecularPart = specular - (float3(MIN_ROUGHNESS) * (1 - metallic) * (1 / max(metallic, epsilon))) * material.SpecularFactor.rgb;
        baseColor = float4(lerp(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);
    }

    float inputColorAplha = 1.0f; // TODO: extend input color to take 4th component
    baseColor *= float4(input.VertexColor, inputColorAplha);

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

    float2 normalUvSet = material.NormalTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;
    float3 n = (material.NormalTextureSet != INVALID_UV_SET) ? 
        GetNormalFromMap(g_BindlessTextures[material.NormalIndex], normalUvSet, input.Normal, input.FragmentWorldPos) : 
        normalize(input.Normal);

    n.y *= -1.0f; // Match Vulkan coordinates

    float3 v = normalize(u_Camera.ViewPosition.xyz - input.FragmentWorldPos); // Vector from surface point to camera
    float3 reflection = normalize(reflect(-v, n));

    // Compute tile index light clustering
    uint tileIndex = ComputeTileIndex(input.FragmentViewPos, u_Constants.GridSize.xyz, u_Camera.Planes, input.ScreenPos, u_Camera.ScreenDimensions);

    // Compute common parameters
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    BaseInputs baseInputs = BaseInputs(
        perceptualRoughness,
		metallic,
		specularEnvironmentR0,
		specularEnvironmentR90,
        alphaRoughness,
		diffuseColor,
		specularColor
    );

    float3 lightContribution = float3(0, 0, 0);
    // Light Contribution
    if (tileIndex < Clusters.getCount()) {
        uint lightCount = Clusters[tileIndex].Count;

        for (uint i = 0; i < lightCount; i++) {
            uint idx = Clusters[tileIndex].LightIndices[i];
            LightInfo L = Lights[idx];

            switch (L.ActiveLightType) {
            case LIGHT_TYPE_POINT:
                break;

            case LIGHT_TYPE_SPOT:
                break;

            case LIGHT_TYPE_DIRECTIONAL:
                lightContribution = ComputeDirectionalLightContribution(n, v, L, baseInputs);
                break;
            }
        }
    }

    lightContribution += GetIBLContribution(NdotV, diffuseColor, perceptualRoughness, n, reflection);
    output.Color = float4(diffuseColor, baseColor.a);

    return output;
}