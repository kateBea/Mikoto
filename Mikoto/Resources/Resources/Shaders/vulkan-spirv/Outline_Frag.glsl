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
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

// Input variables
layout (location = 0) in vec3 inFragmentPos;
layout (location = 1) in vec3 inNormals;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec2 inVertexColor;
layout (location = 4) in vec4 outOutlineColor;

// Output variables
layout (location = 0) out vec4 o_Color;

void main() {

    o_Color = outOutlineColor;
}