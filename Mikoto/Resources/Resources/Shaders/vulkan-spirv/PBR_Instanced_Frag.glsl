//    Copyright 2025 ケイト
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

#version 450

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "ShaderBase.glsl"
#include "ClusteredShading.glsl"

#define INVALID_TEXTURE_INDEX -1

layout(location = 0) in vec3 in_FragmentWorldPos;
layout(location = 1) in vec3 in_Normals;
layout(location = 2) in vec2 in_TexCoord;
layout(location = 3) in vec3 in_VertexColor;
layout(location = 4) in vec3 in_CameraPos;

layout(location = 5) flat in int in_AlbedoIndex;
layout(location = 6) flat in int in_NormalIndex;
layout(location = 7) flat in int in_MetallicIndex;
layout(location = 8) flat in int in_RoughnessIndex;
layout(location = 9) flat in int in_AoIndex;
layout(location = 10) flat in vec4 in_Albedo;
layout(location = 11) flat in vec4 in_Factors;

layout(location = 12) in vec3 in_FragmentViewPos;

layout(location = 13) flat in vec3 v_EmissiveFactors;
layout(location = 14) flat in float v_EmissionIntensity;
layout(location = 15) flat in int v_EmissionIndex;

layout(location = 0) out vec4 out_Color;

layout(set = TEXTURES_SETINDEX, binding = 0) uniform sampler2D g_BindlessTextures[];

layout(scalar, set = PERPASS_SETINDEX, binding = 1) uniform CameraUBO {
    mat4 ViewMatrix;
    mat4 InverseProjection;

    vec4 GridSize;
    vec4 ViewPosition;

    // xy = Planes, zw = ScreenDimensions
    vec4 Screen;

    // x = show heat map
    vec4 ShowHeatMap;
} u_Camera;

layout(scalar, set = PERPASS_SETINDEX, binding = 5) uniform SkyBoxUBO {
    mat4 View;
    mat4 Projection;
    float Exposure;
    float Gamma;
} u_SkyboxParams;

layout(std430, scalar, set = PERPASS_SETINDEX, binding = 2) readonly buffer ClusterSSBO {
    Cluster Clusters[];
};

layout(std430, scalar, set = PERPASS_SETINDEX, binding = 3) readonly buffer LightSSBO {
    LightInfo Lights[];
};

layout (set = PERPASS_SETINDEX, binding = 6) uniform sampler2D u_SamplerBRDFLUT;
layout (set = PERPASS_SETINDEX, binding = 7) uniform samplerCube u_PrefilteredMap;
layout (set = PERPASS_SETINDEX, binding = 8) uniform samplerCube u_SamplerIrradiance;

vec3 GetNormalFromMap(sampler2D normalMap) {
    vec3 tangentNormal = texture(normalMap, in_TexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(in_FragmentWorldPos);
    vec3 Q2  = dFdy(in_FragmentWorldPos);
    vec2 st1 = dFdx(in_TexCoord);
    vec2 st2 = dFdy(in_TexCoord);

    vec3 N   = normalize(in_Normals);
    vec3 T  = normalize( Q1 * st2.t - Q2 * st1.t);
    vec3 B  = -normalize(cross( N, T ));
    mat3 TBN = mat3( T, B, N );

    return normalize(TBN * tangentNormal);
}

vec3 PrefilteredReflection(vec3 R, float roughness) {
    // TODO: Make this configurable from CPU
    const float MAX_REFLECTION_LOD = 9.0;

    float lod = roughness * MAX_REFLECTION_LOD;
    float lodf = floor(lod);
    float lodc = ceil(lod);
    vec3 a = textureLod(u_PrefilteredMap, R, lodf).rgb;
    vec3 b = textureLod(u_PrefilteredMap, R, lodc).rgb;
    return mix(a, b, lod - lodf);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot( N, H ), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = ( NdotH2 * ( a2 - 1.0 ) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = ( roughness + 1.0 );
    float k = ( r * r ) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * ( 1.0 - k ) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot( N, V ), 0.0);
    float NdotL = max(dot( N, L ), 0.0);
    float ggx2 = GeometrySchlickGGX( NdotV, roughness );
    float ggx1 = GeometrySchlickGGX( NdotL, roughness );

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + ( 1.0 - F0 ) * pow( clamp( 1.0 - cosTheta, 0.0, 1.0 ), 5.0 );
}

float ComputeAttenuation(float distance) {
    // Physically correct attenuation
    return 1.0 / ( distance * distance );
}

float ComputeAttenuation(float distance, float radius) {
    return pow( max( 1.0 - pow( distance / radius, 4.0 ), 0.0 ), 2.0 ) / ( distance * distance + 1.0 );
}

vec3 ComputePointLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo, LightInfo lightInfo) {
    vec3 Lo = vec3(0.0);

    vec3 L = normalize(lightInfo.Position.xyz - in_FragmentWorldPos);
    vec3 H = normalize( V + L );
    float distance = length(lightInfo.Position.xyz - in_FragmentWorldPos);
    float attenuation = ComputeAttenuation(distance, lightInfo.Radius );

    vec3 radiance = lightInfo.Diffuse.xyz * attenuation * lightInfo.Intensity;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX( N, H, roughness );
    float G   = GeometrySmith( N, V, L, roughness );
    vec3 F    = FresnelSchlick( max(dot(H, V), 0.0), F0 );

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max( dot( N, V ), 0.0) * max( dot( N, L ), 0.0 ) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;

    // kS is equal to Fresnel
    vec3 kS = F;

    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;

    // multiply kD by the inverse metalness such that only non-metals
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);

    // add to outgoing radiance Lo
    // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    return Lo;
}

vec3 ComputeSpotLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo, LightInfo lightInfo) {
    vec3 Lo = vec3(0.0);

    // calculate per-light radiance
    vec3 L = normalize(lightInfo.Position.xyz - in_FragmentWorldPos);
    float distance = length(lightInfo.Position.xyz - in_FragmentWorldPos);
    float attenuation = ComputeAttenuation(distance, lightInfo.Radius);
    // This vec 3 should be the light color, we assume it is full white for now
    vec3 radiance = lightInfo.Diffuse.xyz * attenuation * lightInfo.Intensity;

    // Spotlight intensity based on angle
    float theta = dot(L, normalize(-vec3(lightInfo.Direction.xyz)));
    float epsilon = lightInfo.CutOff - lightInfo.OuterCutOff;
    float intensity = clamp((theta - lightInfo.OuterCutOff) / epsilon, 0.0, 1.0) * lightInfo.Radius;
    radiance *= intensity;

    vec3 H = normalize(V + L);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;

    // kS is equal to Fresnel
    vec3 kS = F;

    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;

    // multiply kD by the inverse metalness such that only non-metals
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);

    // add to outgoing radiance Lo
    // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    return Lo;
}

vec3 ComputeDirectionalLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo, LightInfo lightInfo) {
    vec3 Lo = vec3(0.0);

    vec3 L = normalize( -vec3(lightInfo.Direction.xyz ) );
    vec3 H = normalize( V + L );

    // This vec 3 should be the light color, we assume it is full white for now
    vec3 radiance = lightInfo.Diffuse.xyz * lightInfo.Intensity;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX( N, H, roughness );
    float G   = GeometrySmith( N, V, L, roughness );
    vec3 F    = FresnelSchlick( clamp( dot( H, V ), 0.0, 1.0 ), F0 );

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max( dot( N, V ), 0.0 ) * max( dot( N, L ), 0.0 ) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;

    // kS is equal to Fresnel
    vec3 kS = F;

    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;

    // multiply kD by the inverse metalness such that only non-metals
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);

    // add to outgoing radiance Lo
    // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    return Lo;
}

vec4 DetermineOutFragmentColor(vec3 N, vec3 color, float metallic, float roughness, float ao, int displayMode) {
    vec4 result = vec4(0.0);

    switch (displayMode) {
        case DISPLAY_COLOR:
        result = vec4(color , 1.0);
        break;

        case DISPLAY_NORMAL:
        result = vec4(N , 1.0);
        break;

        case DISPLAY_METAL:
        result = vec4(metallic, metallic, metallic , 1.0);
        break;

        case DISPLAY_AO:
        result = vec4(ao, ao, ao , 1.0);
        break;

        case DISPLAY_ROUGH:
        result = vec4(roughness, roughness, roughness , 1.0);
        break;
    }

    return result;
}

vec3 CalculateEmissive() {
    // Base emissive factor (user-defined color)
    vec3 emissive = v_EmissiveFactors;

    // Default: no texture means "use emissive color directly"
    vec3 emissiveTex = vec3(1.0);

    // But if a texture is present, multiply it in
    if (v_EmissionIndex != INVALID_TEXTURE_INDEX) {
        emissiveTex = texture(g_BindlessTextures[v_EmissionIndex], in_TexCoord).rgb;
    }

    // Final emissive
    emissive *= emissiveTex;
    emissive *= v_EmissionIntensity;

    return emissive;
}

void main() {

    vec3 albedo     = in_AlbedoIndex != INVALID_TEXTURE_INDEX ?
        pow(texture(g_BindlessTextures[in_AlbedoIndex], in_TexCoord).rgb, vec3(2.2)) :
        in_Albedo.xyz;

    float metallic  = in_MetallicIndex != INVALID_TEXTURE_INDEX ?
        texture(g_BindlessTextures[in_MetallicIndex], in_TexCoord).r :
        in_Factors.x;

    float roughness = in_RoughnessIndex != INVALID_TEXTURE_INDEX ?
        texture(g_BindlessTextures[in_RoughnessIndex], in_TexCoord).r :
        in_Factors.y;

    float ao        = in_AoIndex != INVALID_TEXTURE_INDEX ?
        texture(g_BindlessTextures[in_AoIndex], in_TexCoord).r :
        in_Factors.z;

    vec3 N = in_NormalIndex != INVALID_TEXTURE_INDEX ?
        GetNormalFromMap(g_BindlessTextures[in_NormalIndex]) :
        normalize(in_Normals);

    vec3 V = normalize(in_CameraPos - in_FragmentWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // Locating which cluster this fragment is part of
    uint zTile = uint((log(abs(in_FragmentViewPos.z) / u_Camera.Screen.y) * u_Camera.GridSize.z) / log(u_Camera.Screen.x / u_Camera.Screen.y));
    vec2 tileSize = u_Camera.Screen.zw / u_Camera.GridSize.xy;

    uvec3 tile = uvec3(gl_FragCoord.xy / tileSize, zTile);
    uint tileIndex = uint(tile.x + (tile.y * u_Camera.GridSize.x) + (tile.z * u_Camera.GridSize.x * u_Camera.GridSize.y));

    uint lightCount = Clusters[tileIndex].Count;

    for(int i = 0; i < lightCount; ++i)  {
        uint lightIndex = Clusters[tileIndex].LightIndices[i];
        LightInfo light = Lights[lightIndex];

        switch (light.ActiveLightType) {
            case LIGHT_TYPE_POINT:
                Lo += ComputePointLightContribution(N, V, F0, roughness, metallic, albedo, light);
                break;
            case LIGHT_TYPE_SPOT:
                Lo += ComputeSpotLightContribution(N, V, F0, roughness, metallic, albedo, light);
                break;
            case LIGHT_TYPE_DIRECTIONAL:
                Lo += ComputeDirectionalLightContribution(N, V, F0, roughness, metallic, albedo, light);
                break;
            default:
                break;
        }
    }

    vec3 R = reflect(-V, N);

    vec2 brdf = texture(u_SamplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 reflection = PrefilteredReflection(R, roughness).rgb;
    vec3 irradiance = texture(u_SamplerIrradiance, N).rgb;

    // Diffuse based on irradiance
    vec3 diffuse = irradiance * albedo;

    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

    // Specular reflectance
    vec3 specular = reflection * (F * brdf.x + brdf.y);

    // Ambient part
    vec3 kD = 1.0 - F;
    kD *= 1.0 - metallic;
    vec3 ambient = (kD * diffuse + specular) * vec3(ao, ao, ao);

    vec3 color = ambient + Lo;

    // Tone mapping
    color = Uncharted2Tonemap(color * u_SkyboxParams.Exposure);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

    // Gamma correction
    color = pow(color, vec3(1.0f / u_SkyboxParams.Gamma));

    // Emission
    vec3 emissive = CalculateEmissive();
    vec3 finalColor = color + emissive;

    out_Color = vec4(finalColor , 1.0);

    if (u_Camera.ShowHeatMap.x == MKT_SHADER_TRUE) {
        out_Color = mix(vec4(GetHeatMapColor(lightCount), 1.0), out_Color, 0.67f);
    }
}