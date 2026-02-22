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

layout(location = 0) out vec3 o_Pos;

layout(scalar, push_constant) uniform PrefilterConstants {
    mat4 MVP;
    float Roughness;
    uint NumSamples;
} u_Parameters;

void main() {
    o_Pos = a_Pos;
    gl_Position = u_Parameters.MVP * vec4(a_Pos, 1.0);
}
