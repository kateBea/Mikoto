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

layout(scalar, set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 Projection;
    mat4 ViewMatrix;
    mat4 InverseProjection;
    vec4 ViewPosition;
    vec2 Planes;
    vec2 ScreenDimensions;
} u_CameraParams;

layout(std430, scalar, set = PERPASS_SETINDEX, binding = 1) readonly buffer MeshInfoSSBO {
    MeshInfo Meshes[];
};

// --------------------------------------------------
// Per-vertex attributes (mesh)
// --------------------------------------------------
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord;

// --------------------------------------------------
// Outputs to fragment shader
// --------------------------------------------------
layout(location = 0) out vec3 out_FragmentWorldPos;
layout(location = 1) out vec3 out_VertexNormal;
layout(location = 2) out vec2 out_TexCoord;
layout(location = 3) out vec3 out_Color;
layout(location = 4) out vec3 out_CameraPos;

// instance output
layout(location = 5) flat out int out_AlbedoIndex;
layout(location = 6) flat out int out_NormalIndex;
layout(location = 7) flat out int out_MetallicIndex;
layout(location = 8) flat out int out_RoughnessIndex;
layout(location = 9) flat out int out_AoIndex;
layout(location = 10) flat out vec4 out_Albedo;
layout(location = 11) flat out vec4 out_Factors;

layout(location = 12) out vec3 out_FragmentViewPos;

layout(location = 13) flat out vec3 out_EmissiveFactors;
layout(location = 14) flat out float out_EmissionIntensity;
layout(location = 15) flat out int out_EmissionIndex;

// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {
    MeshInfo meshInfo = Meshes[gl_InstanceIndex];

    mat4 model = mat4(meshInfo.Transform);

    // Per-vertex
    out_Color        = a_Color;
    out_TexCoord     = a_TexCoord;
    out_CameraPos    = u_CameraParams.ViewPosition.xyz;

    // Normal transform
    out_VertexNormal  = transpose(inverse(mat3(model))) * a_Normal;

    // Fragment position
    out_FragmentWorldPos = vec3(model * vec4(a_Position, 1.0));
    out_FragmentViewPos = vec3(u_CameraParams.ViewMatrix * vec4(a_Position, 1.0));

    // Per-instance material values
    out_AlbedoIndex    = meshInfo.AlbedoIndex;
    out_NormalIndex    = meshInfo.NormalIndex;
    out_MetallicIndex  = meshInfo.MetallicIndex;
    out_RoughnessIndex = meshInfo.RoughnessIndex;
    out_AoIndex        = meshInfo.AoIndex;
    out_Albedo         = meshInfo.Albedo;
    out_Factors        = meshInfo.Factors;

    out_EmissiveFactors = meshInfo.EmissiveFactors;
    out_EmissionIntensity = meshInfo.EmissiveIntensity;
    out_EmissionIndex = meshInfo.EmissiveIndex;

    gl_Position = u_CameraParams.Projection * u_CameraParams.ViewMatrix * model * vec4(a_Position, 1.0);
}
