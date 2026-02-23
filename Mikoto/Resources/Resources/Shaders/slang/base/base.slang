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