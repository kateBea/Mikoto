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

layout(set = TEXTURES_SETINDEX, binding = 0) uniform sampler2D g_BindlessTextures[];

layout(scalar, push_constant) uniform HelloTextureUniformBuffer {
    int TextureIndex;
} u_MetaData;

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec3 v_Color;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(v_Color, 1.0);

    if (u_MetaData.TextureIndex != -1) {
        outColor = texture(g_BindlessTextures[u_MetaData.TextureIndex], v_TexCoord);
    }
}