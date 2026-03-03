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

// Camera
[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

// Materials
[[vk::binding(Slot_0, UNBOUNDED_IV_SAMPLERS_SET_INDEX)]] Sampler2D g_BindlessTextures[];

[[vk::binding(Slot_2, UAV_SET_INDEX)]] StructuredBuffer<MaterialParameters, ScalarDataLayout> Materials;

// Lighting
//[[vk::binding(Slot_3, UAV_SET_INDEX)]] StructuredBuffer<Cluster, ScalarDataLayout> Clusters;
//[[vk::binding(Slot_4, UAV_SET_INDEX)]] StructuredBuffer<LightInfo, ScalarDataLayout> Lights;

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
float3 GetIBLContribution(PBRInfo pbrInputs, float3 n, float3 reflection) {
    float lod = (pbrInputs.perceptualRoughness * u_Constants.PrefilteredCubeMipLevels);
    // retrieve a scale and bias to F0. See [1], Figure 3
    float3 brdf = u_SamplerBRDFLUT.Sample(float2(pbrInputs.NdotV, 1.0 - pbrInputs.perceptualRoughness)).rgb;
    float3 diffuseLight = SRGBtoLinear(Tonemap(u_SamplerIrradiance.Sample(n))).rgb;

    float3 specularLight = SRGBtoLinear(Tonemap(u_PrefilteredMap.SampleLevel(reflection, lod))).rgb;

    float3 diffuse = diffuseLight * pbrInputs.diffuseColor;
    float3 specular = specularLight * (pbrInputs.specularColor * brdf.x + brdf.y);

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

struct FSOutput {
    float4 Color : SV_Target0;
};

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput output;

    MaterialParameters material = Materials[input.MaterialIndex];

    float4 baseColor = float4(0.4, 0.2, 0.4, 0.0);

    if (material.DiffuseIndex != -1) {
        baseColor = g_BindlessTextures[material.DiffuseIndex].Sample( material.BaseColorTextureSet == 0 ? input.TexCoord0 : input.TexCoord1);
    } else if (material.AlbedoIndex != -1) {
        baseColor = g_BindlessTextures[material.AlbedoIndex].Sample( material.BaseColorTextureSet == 0 ? input.TexCoord0 : input.TexCoord1);
    } else {
        baseColor = material.BaseColorFactor;
    }

    output.Color = baseColor;

    return output;
}