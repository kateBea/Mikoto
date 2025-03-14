/**************************************************
    Shader for the PBR material with outline expansion.
    Using vec4s and mat4s for simplicity with uniform
    buffer alignment.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450

// [Uniform buffer elements]
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 View;
    mat4 Projection;
    mat4 Transform;
} UniformBufferData;

// [Vertex Buffer elements]
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TextureCoordinates;

// [Output data]

// For usage in fragment shader
layout(location = 0) out vec3 outFragmentPos;
layout(location = 1) out vec3 outVertexNormals;
layout(location = 2) out vec2 outVertexTexCoord;
layout(location = 3) out vec3 outVertexColor;

void main() {
    outVertexNormals = a_Normal;
    outVertexColor = a_Color;
    outVertexTexCoord = a_TextureCoordinates;

    gl_Position = UniformBufferData.Projection * UniformBufferData.View * UniformBufferData.Transform * vec4(a_Position, 1.0);
}
