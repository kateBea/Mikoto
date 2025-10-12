/**************************************************
    Shader for the PBR material with outline expansion.
    Using vec4s and mat4s for simplicity with uniform
    buffer alignment.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450

// Push constant for outline width and color
layout(push_constant) uniform PushConstants {
    mat4 MVP; // Model * View * Projection
    vec4 TextColor;
} pushConstants;

// [Vertex Device elements]
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;
layout(location = 2) in vec2 a_TextureCoordinates;

// For usage in fragment shader
layout(location = 0) out vec4 outVertexColor;
layout(location = 1) out vec2 outVertexTexCoord;

void main() {
    outVertexColor = vec4(a_Color + pushConstants.TextColor.xyz, pushConstants.TextColor.w);
    outVertexTexCoord = a_TextureCoordinates;

    gl_Position = pushConstants.MVP * vec4(a_Position, 1.0);
}
