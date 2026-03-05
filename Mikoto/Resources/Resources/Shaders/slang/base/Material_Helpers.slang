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

// ============================================================================
// Constants
// ============================================================================
static const int PBR_WORKFLOW_METALLIC_ROUGHNESS = 0;
static const int PBR_WORKFLOW_SPECULAR_GLOSSINESS = 1;
static const int PBR_WORKFLOW_UNLIT = 2;

static const int PBR_ALPHA_OPAQUE = 0;
static const int PBR_ALPHA_MASK = 1;
static const int PBR_ALPHA_BLEND = 2;

static const int INVALID_UV_SET = -1;

static const float MIN_ROUGHNESS = 0.04;

// ============================================================================
// Structs
// ============================================================================
struct MaterialParameters {
    float4 BaseColorFactor;
    float4 EmissiveFactor;
    float4 DiffuseFactor;
    float4 SpecularFactor;

    int    Workflow;

    float  MetallicFactor;
    float  RoughnessFactor;
    float  AlphaMask;
    float  AlphaMaskCutoff;
    float  EmissiveStrength;

    int    BaseColorTextureSet;
    int    MetallicRoughnessTextureSet;
    int    SpecularGlossinessSet;
    int    NormalTextureSet;
    int    OcclusionTextureSet;
    int    EmissiveTextureSet;

    int    AlbedoIndex;
    int    DiffuseIndex;
    int    NormalIndex;
    int    MetallicIndex;
    int    RoughnessIndex;
    int    AoIndex;
    int    EmissiveIndex;
    int    MetallicRoughnessIndex;
    int    SpecularGlossinessIndex;
};

struct MeshParameters {
    float4x4 Transform;
    float4x4 InverseModelView;

    int AnimatorID;
};

struct VertexData {
    float3 Position;
    float3 Normal;
    float3 Color;
    float2 TexCoord0;
    float2 TexCoord1;

    float4 Joint;
    float4 Weight;
};

struct IndexData {
    uint Index;
};

// ============================================================================
// Helpers
// ============================================================================
float3 SRGBtoLinear(float3 srgb) {
    // sRGB uses a piecewise curve:
    //  - linear segment for low values
    //  - power curve for higher values

    float3 cutoff = float3(0.04045);
    float3 low = srgb / 12.92;
    float3 high = pow((srgb + 0.055) / 1.055, 2.4);

    // step(x, y):
    //   returns 0 when y < x, otherwise 1
    //
    // So:
    //   less = (srgb <= 0.04045)
    float3 useHigh = step(cutoff, srgb);

    // choose low or high per-channel, branchless
    return lerp(low, high, useHigh);
}

float4 SRGBtoLinear(float4 srgb) {
    return float4(SRGBtoLinear(srgb.xyz), srgb.w);
}

// Gets metallic factor from specular glossiness workflow inputs
float ConvertMetallic(float3 diffuse, float3 specular, float maxSpecular) {
    float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
    float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);
    if (perceivedSpecular < MIN_ROUGHNESS) {
        return 0.0;
    }
    float a = MIN_ROUGHNESS;
    float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - MIN_ROUGHNESS) + perceivedSpecular - 2.0 * MIN_ROUGHNESS;
    float c = MIN_ROUGHNESS - perceivedSpecular;
    float D = max(b * b - 4.0 * a * c, 0.0);
    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}