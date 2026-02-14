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

// Credits: https://github.com/SaschaWillems/Vulkan/tree/master/examples

#version 450

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "ShaderBase.glsl"

layout(scalar, push_constant) uniform IrradianceCamUBO {
    mat4 MVP;
    float DeltaPhi;
    float DeltaTheta;
} u_Parameters;

layout(location = 0) in vec3 v_Pos;

layout (location = 0) out vec4 o_Color;

layout (set = STATIC_SETINDEX, binding = 0) uniform samplerCube u_SamplerEnv;

void main() {
    vec3 N = normalize(v_Pos);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = cross(N, right);

    const float TWO_PI = PI * 2.0;
    const float HALF_PI = PI * 0.5;

    vec3 color = vec3(0.0);
    uint sampleCount = 0u;

    for (float phi = 0.0; phi < TWO_PI; phi += u_Parameters.DeltaPhi) {
        for (float theta = 0.0; theta < HALF_PI; theta += u_Parameters.DeltaTheta) {
            vec3 tempVec = cos(phi) * right + sin(phi) * up;
            vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
            color += texture(u_SamplerEnv, sampleVector).rgb * cos(theta) * sin(theta);
            sampleCount++;
        }
    }

    o_Color = vec4(PI * color / float(sampleCount), 1.0);
}