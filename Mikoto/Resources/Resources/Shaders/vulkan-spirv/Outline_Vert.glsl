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
    Shader for the PBR material with outline expansion.
    Using vec4s and mat4s for simplicity with uniform
    buffer alignment.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450

#extension GL_EXT_scalar_block_layout : require

layout(scalar, set = 0, binding = 0) uniform UniformBufferObject {
    mat4 View;
    mat4 Projection;
    mat4 Transform;
} u_CameraParams;

// Push constant for outline width and color
layout(push_constant) uniform PushConstants {
    vec4 outlineColor;
    float outlineWidth;
} pushConstants;

// [Vertex Device elements]
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TextureCoordinates;

// For usage in fragment shader
layout(location = 0) out vec3 outFragmentPos;
layout(location = 1) out vec3 outVertexNormals;
layout(location = 2) out vec2 outVertexTexCoord;
layout(location = 3) out vec3 outVertexColor;
layout(location = 4) out vec4 outOutlineColor;

void main() {
    outVertexNormals = a_Normal;
    outVertexColor = a_Color;
    outVertexTexCoord = a_TextureCoordinates;
    outOutlineColor = pushConstants.outlineColor;

    float outlineWidth = pushConstants.outlineWidth;

    vec3 pos = vec3(a_Position + a_Normal * outlineWidth);

    gl_Position = u_CameraParams.Projection * u_CameraParams.View * u_CameraParams.Transform * vec4(pos, 1.0);
}
