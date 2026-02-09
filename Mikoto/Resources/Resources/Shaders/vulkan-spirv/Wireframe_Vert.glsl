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
#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderBase.glsl"

layout(scalar, set = PERPASS_SETINDEX, binding = 0) uniform FrameUBO {
    mat4 View;
    mat4 Projection;
    vec4 CameraPosition;
} u_CameraInfo;

layout(std430, scalar, set = PERPASS_SETINDEX, binding = 1) readonly buffer MeshInfoSSBO {
    MeshInfo Meshes[];
};

layout(location = 0) in vec3 a_Position;

void main() {
    MeshInfo meshInfo = Meshes[gl_InstanceIndex];

    mat4 model = mat4(meshInfo.Transform);
    gl_Position = u_CameraInfo.Projection * u_CameraInfo.View * model * vec4(a_Position, 1.0);
}
