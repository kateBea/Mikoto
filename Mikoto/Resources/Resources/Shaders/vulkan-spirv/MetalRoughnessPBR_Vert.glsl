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
#extension GL_GOOGLE_include_directive : require

#include "ShaderBase.glsl"
#include "Material_Helpers.glsl"

layout(scalar, set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 Projection;
    mat4 ViewMatrix;
    mat4 InverseProjection;
    vec4 ViewPosition;
    vec2 Planes;
    vec2 ScreenDimensions;
} u_CameraParams;

layout(std430, scalar, set = STATIC_SETINDEX, binding = 1) readonly buffer MeshInfoStorage {
    MeshParameters Meshes[];
};

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_UV0;
layout(location = 4) in vec2 a_UV1;

layout(location = 5) in vec4 a_Jointa;
layout(location = 6) in vec4 a_Weights;

layout(location = 0) out vec3 out_WorldPos;
layout(location = 1) out vec3 out_VertexNormal;
layout(location = 2) out vec2 out_TexCoord;
layout(location = 3) out vec3 out_Color;
layout(location = 4) out vec3 out_CameraPos;
layout(location = 5) out vec3 out_ViewPos;

void main() {
    MeshParameters meshInfo = Meshes[gl_InstanceIndex];

    mat4 model = mat4(meshInfo.Transform);

    // Per-vertex
    out_Color        = a_Color;
    out_TexCoord     = a_UV0;
    out_CameraPos    = u_CameraParams.ViewPosition.xyz;

    // Normal transform
    out_VertexNormal  = transpose(inverse(mat3(model))) * a_Normal;

    // Fragment position
    out_WorldPos = vec3(model * vec4(a_Position, 1.0));
    out_ViewPos = vec3(u_CameraParams.ViewMatrix * vec4(a_Position, 1.0));

    gl_Position = u_CameraParams.Projection * u_CameraParams.ViewMatrix * model * vec4(a_Position, 1.0);
}
