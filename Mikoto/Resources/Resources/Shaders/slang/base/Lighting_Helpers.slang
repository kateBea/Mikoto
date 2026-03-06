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

#pragma once

#include "LightInfo.slang"
#include "Math_Helpers.slang"

// ============================================================================
// Structures
// ============================================================================
struct SkyBoxParameters {
    float Exposure;
    float Gamma;
    float MaxReflectionLOD;
    float MaxMipLevel;
    int IsSkyboxActive;
};

// ============================================================================
// Helpers
// ============================================================================

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 Uncharted2Tonemap(float3 color) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return ((color * (A * color + C * B) + D * E) /
            (color * (A * color + B) + D * F)) - (E / F);
}

float3 GetNormalFromMap(Sampler2D normalMap, float2 uv, float3 inNormal, float3 worldPos) {
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    float3 tangentNormal = normalMap.Sample(uv).xyz * 2.0 - 1.0;

    float3 q1 = ddx(worldPos);
    float3 q2 = ddy(worldPos);
    float2 st1 = ddx(uv);
    float2 st2 = ddy(uv);

    float3 N = normalize(inNormal);
    float3 T = normalize(q1 * st2.y - q2 * st1.y);
    float3 B = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    return normalize(mul(TBN, tangentNormal));
}

float LinearDepth(float depth, float2 planes) {
    float z = depth * 2.0 - 1.0;

    return (2.0 * planes.x * planes.y) /
           (planes.y + planes.x - z * (planes.y - planes.x));
}

float3 CalculateEmissive( float3 emissiveFactors, float emissiveIntensity, int emissionIndex, Sampler2D emissiveMap, float2 uv) {
    // base emissive color
    float3 emissive = emissiveFactors;

    // emissive texture defaults to white
    float3 emissiveTex = float3(1.0, 1.0, 1.0);

    // if emissive texture present, multiply it in
    emissiveTex = emissiveMap.Sample(uv).rgb;

    // multiply by color and intensity
    return emissive * emissiveTex * emissiveIntensity;
}

float3 CalculateEmissive(float3 emissiveFactors, float emissiveIntensity, int emissionIndex, float2 uv) {
    // base emissive color
    float3 emissive = emissiveFactors;

    // emissive texture defaults to white
    float3 emissiveTex = float3(1.0, 1.0, 1.0);

    // multiply by color and intensity
    return emissive * emissiveTex * emissiveIntensity;
}

// Based omn http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float Random(float2 co) {
    const float a = 12.9898;
    const float b = 78.233;
    const float c = 43758.5453;

    float dt = dot(co, float2(a, b));
    float sn = fmod(dt, 3.14);
    return frac(sin(sn) * c);
}

float2 Hammersley2D(uint i, uint N) {
    // Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = (i << 16) | (i >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);

    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 ImportanceSample_GGX(float2 Xi, float roughness, float3 normal) {
    // Maps a 2D point to a hemisphere with spread based on roughness
    float alpha = roughness * roughness;
    float phi = 2.0 * PI_Value * Xi.x + Random(normal.xz) * 0.1;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha * alpha - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Tangent space
    float3 up = abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangentX = normalize(cross(up, normal));
    float3 tangentY = normalize(cross(normal, tangentX));

    // Convert to world Space
    return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

float2 SampleSphericalMap(float3 v) {
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5f;
    return uv;
}

float3 PrefilteredReflection( SamplerCube prefilteredEnv, float3 R, float roughness, float maxReflectionLOD) {
    float lod = roughness * maxReflectionLOD;
    float lodFloor = floor(lod);
    float lodCeil = ceil(lod);

    float3 a = prefilteredEnv.SampleLevel(R, lodFloor).rgb;
    float3 b = prefilteredEnv.SampleLevel(R, lodCeil).rgb;

    return lerp(a, b, lod - lodFloor);
}

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI_Value * denom * denom;

    return a2 / denom;
}

float3 F_SchlickR(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float ComputeAttenuation(float distance) {
    return 1.0 / (distance * distance);
}

float ComputeAttenuation(float distance, float radius) {
    float x = distance / radius;
    float s = max(1.0 - pow(x, 4.0), 0.0);
    return (s * s) / (distance * distance + 1.0);
}

float3 ComputeDirectionalLight(float3 N, float3 V, float3 F0, float roughness, float metallic, float3 albedo, LightInfo lightInfo) {
    float3 L = normalize(-lightInfo.Direction.xyz);
    float3 H = normalize(V + L);

    float3 radiance = lightInfo.Diffuse.xyz * lightInfo.Intensity;

    // NDF, Geometry, Fresnel
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular = numerator / denom;

    // Diffuse term
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    return (kD * (albedo / PI_Value) + specular) * radiance * NdotL;
}

float3 ComputeSpotLight(float3 N, float3 V, float3 F0, float roughness, float metallic, float3 albedo, LightInfo lightInfo, float3 worldPos) {
    float3 L = normalize(lightInfo.Position.xyz - worldPos);
    float3 H = normalize(V + L);

    float dist = length(lightInfo.Position.xyz - worldPos);

    float attenuation = pow(max(1.0 - pow(dist / lightInfo.Radius, 4.0), 0.0), 2.0) /
                        (dist * dist + 1.0);

    // Base radiance
    float3 radiance = lightInfo.Diffuse.xyz * attenuation * lightInfo.Intensity;

    // Spot angle shaping
    float theta = dot(L, normalize(-lightInfo.Direction.xyz));
    float eps = lightInfo.CutOff - lightInfo.OuterCutOff;
    float intensity = clamp((theta - lightInfo.OuterCutOff) / eps, 0.0, 1.0);
    radiance *= intensity;

    // BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular = numerator / denom;

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    return (kD * (albedo / PI_Value) + specular) * radiance * NdotL;
}

float3 ComputePointLight(float3 N, float3 V, float3 F0, float roughness, float metallic, float3 albedo, LightInfo lightInfo, float3 worldPos) {
    float3 L = normalize(lightInfo.Position.xyz - worldPos);
    float3 H = normalize(V + L);

    float dist = length(lightInfo.Position.xyz - worldPos);

    // Physically based attenuation
    float attenuation = pow(max(1.0 - pow(dist / lightInfo.Radius, 4.0), 0.0), 2.0) /
                        (dist * dist + 1.0);

    float3 radiance = lightInfo.Diffuse.xyz * attenuation * lightInfo.Intensity;

    // BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular = numerator / denom;

    // Diffuse term
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    return (kD * (albedo / PI_Value) + specular) * radiance * NdotL;
}

uint ComputeTileIndex(float3 viewPos, float3 gridSize, float2 planes, float4 fragCoord, float2 screenDimensions) {
    uint zTile =
        uint((log(abs(viewPos.z) / planes.y) *
              gridSize.z) / log(planes.x / planes.y));

    float2 tileSize = screenDimensions / gridSize.xy;

    uint xTile = uint(fragCoord.x / tileSize.x);
    uint yTile = uint(fragCoord.y / tileSize.y);

    uint tileIndex = xTile +
                     yTile * uint(gridSize.x) +
                     zTile * uint(gridSize.x * gridSize.y);

    return tileIndex;
}