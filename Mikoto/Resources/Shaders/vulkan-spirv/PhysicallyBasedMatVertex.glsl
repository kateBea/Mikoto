/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450

// [Uniform buffer elements]
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 View;
    mat4 Projection;
    mat4 Transform;
    mat4 NormalMat;
    vec4 Color;
} UniformBufferData;

// [Vertex Buffer elements]
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TextureCoordinates;

// [Output data]

// For usage in fragment shader
layout(location = 0) out vec4 out_Color;
layout(location = 1) out vec3 out_Normals;
layout(location = 2) out vec2 out_TexCoord;
layout(location = 3) out vec3 out_WorldPos;
layout(location = 4) out vec3 out_AttributeColor;

void main() {
    // Setup frament shader expected data
    out_TexCoord = a_TextureCoordinates;
    out_WorldPos = vec3(UniformBufferData.Transform * vec4(a_Position, 1.0));
    out_Normals = mat3(UniformBufferData.NormalMat) * a_Normal;
    out_Color = UniformBufferData.Color;
    out_AttributeColor = a_Color;

    gl_Position = UniformBufferData.Projection * UniformBufferData.View * UniformBufferData.Transform * vec4(a_Position, 1.0);

    // Temporary, fix coordinate system
    gl_Position.y *= -1;
}