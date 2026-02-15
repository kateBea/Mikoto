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
#include "text_render/TextMaterial.glsl"

layout(std430, scalar, set = STATIC_SETINDEX, binding = 0) readonly buffer FontRenderParamsBuffer {
    FontRenderParams params[];
};

layout(location = 0) in vec3 a_Position;
layout(location = 1) in uint a_TexCoordIndex;

layout(location = 0) out vec2 out_TexCoord;
layout(location = 1) out float out_TexIndex;
layout(location = 2) out vec4 out_Color;

void main() {
    FontRenderParams u_Parameters = params[gl_InstanceIndex];

    out_TexCoord = u_Parameters.TextureCoords[a_TexCoordIndex];
    out_TexIndex = float(u_Parameters.TextureIndex);
    out_Color = u_Parameters.Color;

    vec3 pos = a_Position * u_Parameters.Size.xyz + u_Parameters.Position.xyz;

    gl_Position = u_Parameters.Projection * u_Parameters.View * u_Parameters.Model * vec4(pos, 1.0);
}