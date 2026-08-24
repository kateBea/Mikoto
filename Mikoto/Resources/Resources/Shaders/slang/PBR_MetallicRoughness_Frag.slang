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

// Further ref: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/glTF-WebGL-PBR/shaders/pbr-frag.glsl
// https://github.com/KhronosGroup/glTF-Sample-Viewer.git (branch glTF-WebGL-PBR)
#include "base/Base.slang"
#include "base/Math_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Tonemap_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"

struct DrawParams {
    uint mCameraDataID;
    uint mVerticesBufferID;
    uint mIndicesBufferID;

    uint mGeometryBufferID;
    uint mMaterialsBufferID;
    uint mSkinningBufferID;
    uint mClusterBufferID;

    uint mLightCullingBufferID;
    uint mBrdfColorTargetID;
    uint mPrefilterCubeRtID;
    uint mIrradianceCubeRtID;

    uint mSamplerCubeID;
    uint mSamplerBasicID;

    float4 mGridSize;
    float mExposure;
    float mGamma;

    uint mActiveLightCount;

    uint mEnableSsao;
    float mSsaoIntensity;

    uint mPrefilteredCubeMipLevels;
    float mScaleIblAmbient;

    uint mIsSkyboxActive;

    uint mDirectionalShadowsInfoBufferID;
    uint mDirectionalShadowCastersCount;
};

MKT_DECLARE_STRUCTURED_SRV buffers;

MKT_DECLARE_TEXTURE_SRV(Texture2D) textures2D;
MKT_DECLARE_TEXTURE_SRV(TextureCube) texturesCube;

MKT_DECLARE_SAMPLERS samplers;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

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

struct FSInput {
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD3;
    float2 TexCoord1 : TEXCOORD4;
    float4 VertexColor : TEXCOORD5;

    uint MaterialIndex : TEXCOORD6;

    float4 ScreenPos : SV_Position;
    bool IsFrontFace : SV_IsFrontFace;
};

// Calculation of the lighting contribution from an optional Image Based Light source.
float3 GetIBLContribution(float NdotV, float3 diffuseColor, float3 specularColor, float perceptualRoughness, float3 n, float3 reflection, Texture2D brdf, TextureCube irradianceMap, TextureCube prefilterMap, SamplerState sampler) {
    // Avoid rough reflections sample invalid mip levels
    float lod = perceptualRoughness * (parameters.mPrefilteredCubeMipLevels - 1);

    // retrieve a scale and bias to F0. See [1], Figure 3
    float3 brdf = brdf.Sample(sampler, float2(NdotV, 1.0 - perceptualRoughness)).rgb;

    float3 diffuseLight = SRGBtoLINEAR(Tonemap_Uncharted2(irradianceMap.Sample(sampler, n), parameters.mExposure, parameters.mGamma)).rgb;
    float3 specularLight = SRGBtoLINEAR(Tonemap_Uncharted2(prefilterMap.SampleLevel(sampler, reflection, lod), parameters.mExposure, parameters.mGamma)).rgb;

    float3 diffuse = diffuseLight * diffuseColor;
    float3 specular = specularLight * (specularColor * brdf.x + brdf.y);

    diffuse *= parameters.mScaleIblAmbient;
    specular *= parameters.mScaleIblAmbient;

    return diffuse + specular;
}

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
float3 Diffuse(PBRInfo pbrInputs) {
    return pbrInputs.diffuseColor / PI_Value;
}

// Disney Implementation of diffuse from Physically-Based Shading at Disney by Brent Burley. See Section 5.3.
// http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
float3 DiffuseBurley(PBRInfo pbrInputs) {
    float f90 = 2.0 * pbrInputs.LdotH * pbrInputs.LdotH * pbrInputs.alphaRoughness - 0.5;

    return (pbrInputs.diffuseColor / PI_Value) * (1.0 + f90 * pow((1.0 - pbrInputs.NdotL), 5.0)) *
           (1.0 + f90 * pow((1.0 - pbrInputs.NdotV), 5.0));
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
float3 SpecularReflection(PBRInfo pbrInputs) {
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
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
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games
float MicrofacetDistribution(PBRInfo pbrInputs) {
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI_Value * f * f);
}

float3 ComputeDirectionalLightContribution(float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs) {
    float3 l = normalize(-lightInfo.Direction.xyz); // Vector from surface point to light (multiply by -1 because otherwise wrong face is lit)
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
    float3 F = SpecularReflection(pbrInputs); // kS
    float G = GeometricOcclusion(pbrInputs);
    float D = MicrofacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution, ks + kD = 1 with kD = 1.0 - F
    float3 diffuseContrib = (1.0 - F) * DiffuseBurley(pbrInputs);
    float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float3 color = NdotL * lightInfo.Diffuse * (diffuseContrib + specContrib);

    return color;
}

// Ref: Moving frostbite to PBR
// https://www.ea.com/frostbite/news/moving-frostbite-to-pb
float SmoothDistanceAtt(float d2, float invR2) {
    float howFar = d2 * invR2;                       
    float dontDie = saturate(1.0 - howFar * howFar); 
    return dontDie * dontDie;                       
}

float GetDistanceAtt(float3 L_unorm, float invR2) {
    float d2 = dot(L_unorm, L_unorm);   
    float invSq = 1.0 / max(d2, 0.0001);

    float fade = SmoothDistanceAtt(d2, invR2); 

    return invSq * fade;
}

float GetAngleAtt(float3 L, float3 lightDir, float scale, float offset) {
    float howInsideCone = dot(lightDir, L); // are we in the cone or nah

    float cone = saturate(howInsideCone * scale + offset);
    cone *= cone;                                         

    return cone; 
}

float3 ComputePointlLightContribution(float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos) {
    // Light vector and distance (v, l)
    const float3 lightVector = lightInfo.Position.xyz - worldPos;
    const float distance = length(lightVector);

    float attenuation = 1;

    const float radius = lightInfo.Radius;
    const float lightInvSqrAttRadius = 1 / (radius * radius);

    attenuation *= GetDistanceAtt(lightVector, lightInvSqrAttRadius);
    
    const float intensityBaseFactor = 1000.0f;
    const float3 lightIntensity = lightInfo.Diffuse * (lightInfo.Intensity * intensityBaseFactor) * attenuation;
    
    const float3 l = lightVector / distance;
    const float3 h = normalize(l + v);

    const float NdotL = clamp(dot(n, l), 0.001, 1.0);
    const float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    const float NdotH = clamp(dot(n, h), 0.0, 1.0);
    const float LdotH = clamp(dot(l, h), 0.0, 1.0);
    const float VdotH = clamp(dot(v, h), 0.0, 1.0);

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
    const float3 F = SpecularReflection(pbrInputs); // kS
    const float G = GeometricOcclusion(pbrInputs);
    const float D = MicrofacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution, ks + kD = 1 with kD = 1.0 - F
    const float3 diffuseContrib = (1.0 - F) * DiffuseBurley(pbrInputs);
    const float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    const float3 color = NdotL * lightIntensity * (diffuseContrib + specContrib);

    return color;
}

float3 ComputeSpotLightContribution(float3 n, float3 v, LightInfo lightInfo, BaseInputs inputs, float3 worldPos) {
    float3 lightVector = lightInfo.Position.xyz - worldPos;

    float distance = length(lightVector);
    float3 l = lightVector / distance;

    // distance attenuation (Smooth fallof not physically correct)
    float distanceAttenuation = saturate(1.0 - distance / lightInfo.Radius);
    distanceAttenuation *= distanceAttenuation;

    // spotlight cone
    float theta = dot(normalize(-lightInfo.Direction.xyz), l);

    float epsilon = lightInfo.CutOff - lightInfo.OuterCutOff;
    float coneAttenuation = saturate((theta - lightInfo.OuterCutOff) / epsilon);

    // final attenuation
    float attenuation = distanceAttenuation * coneAttenuation;

    const float intensityBaseFactor = 1000.0f;
    float3 lightIntensity = lightInfo.Diffuse * (lightInfo.Intensity * intensityBaseFactor) * attenuation;

    float3 h = normalize(l + v);

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
    float3 F = SpecularReflection(pbrInputs); // kS
    float G = GeometricOcclusion(pbrInputs);
    float D = MicrofacetDistribution(pbrInputs);

    // Calculation of analytical lighting contribution, ks + kD = 1 with kD = 1.0 - F
    float3 diffuseContrib = (1.0 - F) * DiffuseBurley(pbrInputs);
    float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);

    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float3 color = NdotL * lightIntensity * (diffuseContrib + specContrib);

    return color;
}

struct FSOutput {
    float4 Color : SV_Target0;
};

float ComputeShadow(float4 fragLightSpaceCoord, LightInfo light, float3 normal, Texture2D shadowMap, SamplerState sampler) {
    const float shadowVal = 0.2f;
    const float noShadowVal = 1.0f;

    // Perspective divide
    float3 projCoords = fragLightSpaceCoord.xyz / fragLightSpaceCoord.w;

    // Vulkan z is already within [0, 1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // This is something that I tried from memory, I remember needing this for proper texture 
    // coordinates on legacy Mikoto, this need further investigation, but still does not make any sense
    // not doing this, gives wrong results, remove line and see results
    projCoords.y = 1 - projCoords.y;

    if (projCoords.z > 1.0f) {
        return shadowVal;
    }

    float currentDpeth = projCoords.z;
    float closestDepth = shadowMap.Sample( sampler, projCoords.xy ).r;

    float bias = max(0.05 * (1.0 - dot(normal, -light.Direction.xyz)), 0.005);
    float shadow = (currentDpeth - bias) > closestDepth ? noShadowVal : shadowVal;

    return shadow;
}

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput output;

    // Fecth buffers
    ByteAddressBuffer materialsBuffer = buffers[parameters.mMaterialsBufferID];
    MaterialParameters material = materialsBuffer.Read<MaterialParameters>(0, input.MaterialIndex);

    ByteAddressBuffer clusterBuffer = buffers[parameters.mClusterBufferID];
    ByteAddressBuffer lightCullingBuffer = buffers[parameters.mLightCullingBufferID];

    CameraInfo camera = buffers[parameters.mCameraDataID].Read<CameraInfo>(0, 0);

    // Fectch textures and samplers as needed, based on the material properties
    SamplerState samplerCube = samplers[parameters.mSamplerCubeID]; // nearest clamped sampler for cube maps
    TextureCube prefilteredMap = texturesCube[parameters.mPrefilterCubeRtID];
    TextureCube irradianceMap = texturesCube[parameters.mIrradianceCubeRtID];

    Texture2D brdfLUT = textures2D[parameters.mBrdfColorTargetID];

    SamplerState samplerBasic = samplers[parameters.mSamplerBasicID]; // Normal 2D samplers

    float4 baseColor = float4(0.0, 0.0, 0.0, 0.0);

    // Handle alpha masking (glTF alphaMode = MASK)
    // Fragments with alpha below the cutoff are discarded.
    //
    // For OPAQUE and BLEND modes no special handling is required in the shader.
    // OPAQUE ignores the alpha channel entirely, while BLEND uses the alpha
    // value during framebuffer blending which is handled via pipeline state.
    if (material.AlphaMask == PBR_ALPHA_MASK) {
        if (material.AlbedoIndex != INVALID_UV_SET) {
            Texture2D colorMap = textures2D[material.AlbedoIndex];
            float2 uvSet = material.BaseColorTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;

            baseColor = SRGBtoLINEAR(colorMap.Sample(samplerBasic, uvSet)) * material.BaseColorFactor;
        } else {
            baseColor = material.BaseColorFactor;
        }

        if (baseColor.a < material.AlphaMaskCutoff) {
            discard;
        }
    }

    float metallic = 0.0f;
    float perceptualRoughness = 0.0;

    if (material.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS) {
        // Metallic and Roughness material properties are packed together
        // In glTF, these factors can be specified by fixed scalar values
        // or from a metallic-roughness map
        perceptualRoughness = material.RoughnessFactor;
        metallic = material.MetallicFactor;

        if (material.MetallicRoughnessIndex != INVALID_TEXTURE_INDEX) {
            // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
            // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
            Texture2D metallicRoughnessMap = textures2D[material.MetallicRoughnessIndex];
            float4 mrSample = metallicRoughnessMap.Sample(samplerBasic, material.MetallicRoughnessTextureSet == 0 ? input.TexCoord0 : input.TexCoord1);

            perceptualRoughness = mrSample.g * perceptualRoughness;
            metallic = mrSample.b * metallic;
        } else {
            perceptualRoughness = clamp(perceptualRoughness, MIN_ROUGHNESS, 1.0);
            metallic = clamp(metallic, 0.0, 1.0);
        }
        // Roughness is authored as perceptual roughness; as is convention,
        // convert to material roughness by squaring the perceptual roughness [2].

        // The albedo may be defined from a base texture or a flat color
        if (material.AlbedoIndex != INVALID_TEXTURE_INDEX) {
            Texture2D colorMap = textures2D[material.AlbedoIndex];
            baseColor = SRGBtoLINEAR(colorMap.Sample(samplerBasic, material.BaseColorTextureSet == 0 ? input.TexCoord0 : input.TexCoord1)) * material.BaseColorFactor;
        } else {
            baseColor = material.BaseColorFactor;
        }
    }

    if (material.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS) {
        // Values from specular glossiness workflow are converted to metallic roughness
        if (material.SpecularGlossinessIndex != INVALID_TEXTURE_INDEX) {
            Texture2D specularGlossinessMap = textures2D[material.SpecularGlossinessIndex];
            perceptualRoughness = 1.0 - specularGlossinessMap.Sample(samplerBasic, material.SpecularGlossinessSet == 0 ? input.TexCoord0 : input.TexCoord1).a;
        } else {
            perceptualRoughness = 0.0;
        }

        const float epsilon = 1e-6;

        // Diffuse index because specular glossiness
        Texture2D colorMap = textures2D[material.DiffuseIndex];
        Texture2D specularGlossinessMap = textures2D[material.SpecularGlossinessIndex];

        float4 diffuse = SRGBtoLINEAR(colorMap.Sample(samplerBasic, input.TexCoord0));
        float3 specular = SRGBtoLINEAR(specularGlossinessMap.Sample(samplerBasic, input.TexCoord0)).rgb;

        float maxSpecular = max(max(specular.r, specular.g), specular.b);

        // Convert metallic value from specular glossiness inputs
        metallic = ConvertMetallic(diffuse.rgb, specular, maxSpecular);

        float3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - MIN_ROUGHNESS) / max(1 - metallic, epsilon)) * material.DiffuseFactor.rgb;
        float3 baseColorSpecularPart = specular - (float3(MIN_ROUGHNESS) * (1 - metallic) * (1 / max(metallic, epsilon))) * material.SpecularFactor.rgb;
        baseColor = float4(lerp(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);
    }

    baseColor *= float4(input.VertexColor);
    
    // dielectrics default to 4%
    float3 f0 = float3(0.04);
    
    // diffuse should disappear as the material becomes metallic.
    float diffuseFactor = 1.0f - metallic;

    float3 diffuseColor = baseColor.rgb * (float3(1.0) - f0) * diffuseFactor;
    float3 specularColor = lerp(f0, baseColor.rgb, metallic);

    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    float3 specularEnvironmentR0 = specularColor.rgb;
    float3 specularEnvironmentR90 = float3(1.0, 1.0, 1.0) * reflectance90;

    float2 normalUvSet = material.NormalTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;
    float3 n = (material.NormalIndex != INVALID_UV_SET) ? 
        GetNormalFromMap(textures2D[material.NormalIndex], samplerBasic, normalUvSet, input.Normal, input.FragmentWorldPos) : 
        normalize(input.Normal);

    if (!input.IsFrontFace) {
        n.y *= -1.0f;
    }

    float3 v = normalize(camera.ViewPosition.xyz - input.FragmentWorldPos); // Vector from surface point to camera
    float3 reflection = normalize(reflect(-v, n));

    BaseInputs inputs;
    inputs.perceptualRoughness = perceptualRoughness;
    inputs.metallic = metallic;
    inputs.specularEnvironmentR0 = specularEnvironmentR0;
    inputs.specularEnvironmentR90 = specularEnvironmentR90;
    inputs.alphaRoughness = alphaRoughness;
    inputs.diffuseColor = diffuseColor;
    inputs.specularColor = specularColor;

    float3 lightContribution = float3(0.0);

    // Cluster resolve
    // TODO: Check first if there are active lights 
    uint clusterIndex = ComputeTileIndex(input.FragmentViewPos, parameters.mGridSize.xyz, 
        camera.Planes.xy, input.ScreenPos, camera.ScreenDimensions.xy);

    Cluster cluster = clusterBuffer.Read<Cluster>(0, clusterIndex);

    for (uint i = 0; i < cluster.Count && parameters.mActiveLightCount != 0; ++i) {
        uint lightIndex = cluster.LightIndices[i];
        LightInfo light = lightCullingBuffer.Read<LightInfo>(0, lightIndex);

        if (light.ActiveLightType == LIGHT_TYPE_DIRECTIONAL) {

            if (parameters.mDirectionalShadowCastersCount == 0) {
                lightContribution += ComputeDirectionalLightContribution(n, v, light, inputs);
            } else {
                for (uint count = 0; count < parameters.mDirectionalShadowCastersCount; ++count) {
                    DirectionalShadowMapInfo shadowMapInfo = buffers[parameters.mDirectionalShadowsInfoBufferID]
                        .Read<DirectionalShadowMapInfo>(0, count);
                    float4 fragPosLightSpace = shadowMapInfo.mLightViewProjection * float4(input.FragmentWorldPos, 1.0f);

                    float shadow = ComputeShadow(fragPosLightSpace, light, n, textures2D[shadowMapInfo.mCasterMapID], samplerBasic);
                    lightContribution += ComputeDirectionalLightContribution(n, v, light, inputs) * (1.0f - shadow);
                }
            }
            continue;
        }

        if (light.ActiveLightType == LIGHT_TYPE_POINT) {
            lightContribution += ComputePointlLightContribution(n, v, light, inputs, input.FragmentWorldPos);
            continue;
        }

        if (light.ActiveLightType == LIGHT_TYPE_SPOT) {
            lightContribution += ComputeSpotLightContribution(n, v, light, inputs, input.FragmentWorldPos);
        }
    }

    // IBL Contribution
    float NdotV = clamp(dot(n, v), 0.001, 1.0);
    float3 iblLighting = float3(0.0);

    if (parameters.mIsSkyboxActive == MKT_SHADER_TRUE) {
        iblLighting = GetIBLContribution(NdotV, diffuseColor, specularColor, perceptualRoughness, n, reflection, brdfLUT, irradianceMap, prefilteredMap, samplerBasic);
    }

    // Final color
    float3 color = lightContribution + iblLighting;

    // AO
    float ssao = 1.0f;

    if (parameters.mEnableSsao != MKT_SHADER_FALSE) {
        float2 uv = input.ScreenPos.xy / camera.ScreenDimensions.xy;

        //ssao = parameters.mBl == MKT_SHADER_TRUE ? u_SSAOBlur.Sample(uv).r : u_SSAO.Sample(uv).r;
        //ssao = pow(ssao, u_Constants.SSAOIntensity);
    }

    float3 ssaoContrib = float3(ssao, ssao, ssao);

    if (material.AoIndex != INVALID_TEXTURE_INDEX) {
        Texture2D aoMap = textures2D[material.AoIndex];
        float2 aoUVSet = material.OcclusionTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;

        float ao = aoMap.Sample(samplerBasic, aoUVSet).r;

        color *= lerp(color, color * ao, parameters.mSsaoIntensity) * ssaoContrib;
    }

    // Emission
    float3 emissiveContrib = material.EmissiveFactor.xyz * material.EmissiveStrength;
    if (material.EmissiveIndex != INVALID_TEXTURE_INDEX) {
        Texture2D emissiveMap = textures2D[material.EmissiveIndex];
        float2 emissiveUvSet = material.EmissiveTextureSet == 0 ? input.TexCoord0 : input.TexCoord1;

        float3 emissive = emissiveMap.Sample(samplerBasic, emissiveUvSet).rgb;

        emissiveContrib *= emissive;
    }

    color += emissiveContrib;
    output.Color = float4(color, baseColor.a);

    return output;
}