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
static const int INVALID_TEXTURE_INDEX = -1;

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

    int    IsBloomy;
};

struct MeshParameters {
    float4x4 Transform;
    float4x4 InverseModelView;

    // For vertex pulling, this tells the offset into the  array of vertices
    uint VertexOffset;
    uint IndexOffset;

    int AnimatorID; // MatricesIndex
    int HasArmature;
};

struct VertexDescription {
    float3 position   : POSITION;
    float3 normal     : NORMAL;
    float4 color      : COLOR;

    float2 uv0        : TEXCOORD0;
    float2 uv1        : TEXCOORD1;

    float4 joints0    : BLENDINDICES0;
    float4 weights0   : BLENDWEIGHT0;

    float4 joints1    : BLENDINDICES1;
    float4 weights1   : BLENDWEIGHT1;
};

struct MeshGeometryInfo {
    float4x4 Transform;
    float4x4 InverseModelView; // inverse(view * model) computed in CPU for performance

    // For vertex pulling, this tells the
    // offset into the array of vertices
    uint IndexOffset;
    uint VertexOffset;

    // Index into the buffer
    // of list of skinning matrices
    int SkinningMatricesID;
    int HasArmature;

    // TODO: add a list of shadow casters. This will be an index into the array of shadow casters
    // buffer to know from which lights this entity needs shadows casted from, ofc there will be a limit you cannot
    // just slap a unlimited amount of shadow casters

    uint ObjectID;
};

// ============================================================================
// Helpers
// ============================================================================º
float4 SRGBtoLINEAR(float4 srgbIn) {
    float3 linOut = pow(srgbIn.xyz, float3(2.2));
    return float4(linOut, srgbIn.a);
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

// Inverse square attenuation for point lights (more expensive)
float SphereLightAttenuation(float distance, float radius) {
    float d2 = distance * distance;
    float r2 = radius * radius;

    return (2.0 / r2) * (1.0 - (distance / sqrt(d2 + r2)));
}