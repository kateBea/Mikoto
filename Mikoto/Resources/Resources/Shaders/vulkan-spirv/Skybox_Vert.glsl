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

#extension GL_EXT_scalar_block_layout : require

#include "ShaderBase.glsl"

layout(location = 0) in vec3 a_Pos;

layout(location = 0) out vec3 v_Direction;
layout(location = 1) flat out float v_Exposure;
layout(location = 2) flat out float v_Gamma;

layout(scalar, push_constant) uniform SkyBoxParameters {
    float Exposure;
    float Gamma;

    float MaxReflectionLOD;

    int IsSkyboxActive;
} u_IBLParams;

layout(scalar, set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 Projection;
    mat4 ViewMatrix;
    mat4 InverseProjection;
    vec4 ViewPosition;
    vec2 Planes;
    vec2 ScreenDimensions;
} u_Camera;

void main() {
    v_Exposure = u_IBLParams.Exposure;
    v_Gamma = u_IBLParams.Gamma;

    vec3 pos = vec3(a_Pos);

    // Remove translation from view matrix
    mat4 view = mat4(mat3(u_Camera.ViewMatrix));

    // Vulkan cubemap convention fix
    v_Direction = pos;
    //v_Direction.xy *= -1;

    // Force skybox to far plane
    vec4 clip = u_Camera.Projection * view * vec4(pos, 1.0);

    gl_Position = clip;
    //gl_Position = clip.xyww;
}
