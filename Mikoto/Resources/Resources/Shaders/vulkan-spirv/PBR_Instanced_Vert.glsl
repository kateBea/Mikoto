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
#include "Material_Helpers.glsl"

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(scalar, set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 Projection;
    mat4 ViewMatrix;
    mat4 InverseProjection;
    vec4 ViewPosition;
    vec2 Planes;
    vec2 ScreenDimensions;
} u_CameraParams;

layout(std430, scalar, set = PERPASS_SETINDEX, binding = 7) readonly buffer MeshSkinnedBones {
    mat4 meshBoneMatrices[];
};

layout(std430, scalar, set = STATIC_SETINDEX, binding = 1) readonly buffer MeshParametersBuffer {
    MeshParameters Meshes[];
};

// --------------------------------------------------
// Per-vertex attributes (mesh)
// --------------------------------------------------
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord0;
layout(location = 4) in vec2 a_TexCoord1;

layout(location = 5) in vec4 a_Joint;
layout(location = 6) in vec4 a_Weight;

// --------------------------------------------------
// Outputs to fragment shader
// --------------------------------------------------
layout(location = 0) out vec3 out_FragmentViewPos;
layout(location = 1) out vec3 out_FragmentWorldPos;
layout(location = 2) out vec3 out_VertexNormal;
layout(location = 3) out vec2 out_TexCoord;
layout(location = 4) out vec3 out_Color;

layout(location = 5) flat out int o_AlbedoIndex;
layout(location = 6) flat out int o_NormalIndex;
layout(location = 7) flat out int o_MetallicIndex;
layout(location = 8) flat out int o_RoughnessIndex;
layout(location = 9) flat out int o_AoIndex;
layout(location = 10) flat out vec4 o_Albedo;

layout(location = 11) flat out float o_MetallicFactor;
layout(location = 12) flat out float o_RoughnessFactor;
layout(location = 13) flat out float o_OcclusionStrength;

layout(location = 14) flat out vec3 o_EmissiveFactors;
layout(location = 15) flat out float o_EmissionIntensity;
layout(location = 16) flat out int o_EmissionIndex;
layout(location = 17) flat out float o_Alpha;

layout(location = 18) out vec2 o_TexCoord1;


void main() {
    MeshParameters meshInfo = Meshes[gl_InstanceIndex];

    mat4 model = mat4(meshInfo.Transform);

    // --------------------------------------------------
    // 1. Skin the vertex position
    // --------------------------------------------------
    vec4 totalPosition = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);

    bool skinned = (meshInfo.BonesID != -1);

    if (skinned) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            int jointIndex = int(a_Joint[i]);
            float w = a_Weight[i];

            if (w <= 0.0 || jointIndex < 0)
                continue;

            if (jointIndex >= MAX_BONES) {
                // Fallback to rigid
                totalPosition = vec4(a_Position, 1.0);
                skinnedNormal = a_Normal;
                break;
            }

            mat4 boneMatrix = meshBoneMatrices[meshInfo.BonesID * MAX_BONES + jointIndex];

            totalPosition += (boneMatrix * vec4(a_Position, 1.0)) * w;

            skinnedNormal += (mat3(boneMatrix) * a_Normal) * w;
        }
    } else {
        totalPosition = vec4(a_Position, 1.0);
        skinnedNormal = a_Normal;
    }

    // --------------------------------------------------
    // 2. Apply model transform to skinned pos + normal
    // --------------------------------------------------
    vec4 worldPos = model * totalPosition;
    vec3 worldNormal = normalize(mat3(model) * skinnedNormal);

    // --------------------------------------------------
    // 3. Write varying outputs
    // --------------------------------------------------
    out_Color          = a_Color;
    out_TexCoord       = a_TexCoord0;
    o_TexCoord1        = a_TexCoord1;

    out_VertexNormal   = worldNormal;
    out_FragmentWorldPos = worldPos.xyz;
    out_FragmentViewPos = vec3(u_CameraParams.ViewMatrix * worldPos);

    // Material outputs
    o_AlbedoIndex     = meshInfo.AlbedoIndex;
    o_NormalIndex     = meshInfo.NormalIndex;
    o_MetallicIndex   = meshInfo.MetallicIndex;
    o_RoughnessIndex  = meshInfo.RoughnessIndex;
    o_AoIndex         = meshInfo.AoIndex;

    o_Albedo          = meshInfo.Albedo;
    o_MetallicFactor  = meshInfo.MetallicFactor;
    o_RoughnessFactor = meshInfo.RoughnessFactor;
    o_OcclusionStrength = meshInfo.OcclusionStrength;
    o_Alpha           = meshInfo.AlphaCutoff;
    o_EmissiveFactors = meshInfo.EmissiveFactors;
    o_EmissionIntensity = meshInfo.EmissiveIntensity;
    o_EmissionIndex     = meshInfo.EmissiveIndex;

    // --------------------------------------------------
    // 4. Final clip-space position
    // --------------------------------------------------
    gl_Position =
        u_CameraParams.Projection *
        u_CameraParams.ViewMatrix *
        worldPos;
}
