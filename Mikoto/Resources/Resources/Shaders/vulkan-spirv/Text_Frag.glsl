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

#extension GL_EXT_nonuniform_qualifier : enable

#include "ShaderBase.glsl"

layout(location = 0) in vec2 in_TexCoord;
layout(location = 1) in float in_AtlasIndex;
layout(location = 2) in vec4 in_Color;

layout(location = 3) in vec4 in_OutlineColor;

layout(set = TEXTURES_SETINDEX, binding = 0) uniform sampler2D g_BindlessTextures[];

layout(location = 0) out vec4 outColor;

float Median(vec3 msd) {
    return max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
}

float ScreenPxRange() {
    vec2 unitRange = vec2(2.0) / vec2(textureSize(g_BindlessTextures[int(in_AtlasIndex)], 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(in_TexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

float ScreenPxRange3D(sampler2D msdf) {
    float pxRange = 2.0; // taken from font factory

    vec2 unitRange = vec2(pxRange)/vec2(textureSize(msdf, 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(in_TexCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

void main() {
    float threshold = 0.5;
    vec3 msd = texture(g_BindlessTextures[int(in_AtlasIndex)], in_TexCoord).rgb;
    float sd = Median(msd);

    float pxRange = ScreenPxRange3D(g_BindlessTextures[int(in_AtlasIndex)]);
    float screenPxDistance = pxRange * (sd - threshold);
    float opacity = clamp(screenPxDistance + threshold, 0.0, 1.0);

    vec4 bgColor = vec4(0.0);
    outColor = mix(bgColor, in_Color, opacity);
}