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

/**************************************************
    Shader for the SkyBox.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "ShaderBase.glsl"
#include "IBL_Helpers.glsl"

layout (location = 0) in vec3 v_Direction;
layout (location = 1) in float v_Exposure;
layout (location = 2) in float v_Gamma;

layout (location = 0) out vec4 o_Color;

layout (set = STATIC_SETINDEX, binding = 1) uniform samplerCube u_Skybox;

layout(scalar, push_constant) uniform SkyBoxParameters {
    float Exposure;
    float Gamma;

    float MaxReflectionLOD;
    float MaxMipLevel;

    int IsSkyboxActive;
} u_IBLParams;

void main() {
    const float blur = 3.5f;
    vec3 color = vec3(0.0f);

    if (u_IBLParams.MaxMipLevel > blur) {
        color = texture(u_Skybox, v_Direction, blur).rgb;
    } else {
        color = texture(u_Skybox, v_Direction).rgb;
    }

    // Tone mapping
    color = Uncharted2Tonemap(color * v_Exposure);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

    // Gamma correction
    color = pow(color, vec3(1.0f / v_Gamma));

    o_Color = vec4(color, 1.0);
}
