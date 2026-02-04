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
    Simple Compute Shader for debugging

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

#include "ShaderBase.glsl"

#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 10) in;

layout(std140, scalar, set = PERPASS_SETINDEX, binding = 0) buffer RandomBuffer {
    uint values[];
};

void main() {
    uint id = gl_GlobalInvocationID.x;

    // For testing
    values[id] = id;
}