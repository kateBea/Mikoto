#version 450

// Input data (geometry information)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TextureCoordinates;

// Output data (geometry information) for usage in fragment shader
layout(location = 0) out vec4 vertexPosition;
layout(location = 1) out vec3 vertexNormals;
layout(location = 2) out vec4 vertexColor;
layout(location = 3) out vec2 vertexTextureCoord;

// Uniform buffer data
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 View;
    mat4 Projection;
    mat4 Transform;
    vec4 Color;
} UniformBufferData;

void main() {
    // 1. [Compute world position]
    gl_Position = UniformBufferData.Projection * UniformBufferData.View * UniformBufferData.Transform * vec4(a_Position, 1.0);

    // Temporary, fix coordinate system
    gl_Position.y *= -1;


    // 2. [Setup output fragment shader data]
    vertexPosition = gl_Position;

    // TODO: Compute normals in world space
    vertexNormals = a_Normal;

    vertexColor = UniformBufferData.Color;
    vertexTextureCoord = a_TextureCoordinates;
}