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

// Reference types
// https://docs.shader-slang.org/en/latest/coming-from-glsl.html#option-2-glsl-style-layout-syntax

// Command validate slang offline (Example)
// slangc Skybox_Vert.slang -profile glsl_450 -target spirv -entry main -stage vertex

// ============================================================================
// Descriptor Set Indices
// ============================================================================
static const int TEXTURES_SET_INDEX  = 0;
static const int DYNAMIC_SET_INDEX   = 2;
static const int STATIC_SET_INDEX    = 3;

static const int INVALID_TEXTURE_INDEX = -1;

// ============================================================================
// Boolean constants (to be removed)
// ============================================================================
static const int MKT_SHADER_TRUE  = 1;
static const int MKT_SHADER_FALSE = 0;

// ============================================================================
// Math constants
// ============================================================================
static const float PI = 3.1415926535897932384626433832795;

// ============================================================================
// Structs
// ============================================================================

struct MeshInfo {
    float4x4 Transform;

    float4   Albedo;
    float4   Factors;

    float3   EmissiveFactors;
    float    EmissiveIntensity;

    float    Alpha;

    int      AlbedoIndex;
    int      NormalIndex;
    int      MetallicIndex;
    int      RoughnessIndex;
    int      AoIndex;
    int      EmissiveIndex;
};

// ============================================================================
// Helper functions
// ============================================================================
float4x4 RemoveTranslation(float4x4 m) {
    float3x3 r = float3x3(
        m[0].xyz,
        m[1].xyz,
        m[2].xyz
    );

    return float4x4(
        float4(r[0], 0.0),
        float4(r[1], 0.0),
        float4(r[2], 0.0),
        float4(0.0, 0.0, 0.0, 1.0)
    );
}