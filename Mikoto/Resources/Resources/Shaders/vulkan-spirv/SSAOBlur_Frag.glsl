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

#include "ShaderBase.glsl"

layout (set = PERPASS_SETINDEX, binding = 0) uniform sampler2D u_samplerSSAO;

layout (location = 0) in vec2 v_UV;

layout (location = 0) out float o_Color;

void main() {
    int n = 0;
    const int blurRange = 2;

    vec2 texelSize = 1.0 / vec2(textureSize(u_samplerSSAO, 0));

    float result = 0.0;
    for (int x = -blurRange; x <= blurRange; x++) {
        for (int y = -blurRange; y <= blurRange; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(u_samplerSSAO, v_UV + offset).r;
            n++;
        }
    }

    o_Color = result / (float(n));
}