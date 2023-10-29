#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 View;
    mat4 Projection;
    mat4 Transform;
    vec4 Color;
} UniformBufferData;

// Vertex Buffer elements
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TextureCoordinates;

// For usage in fragment shader
layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 vertexTexCoord;

void main() {
    // Setup frament shader expected data
    vertexColor = UniformBufferData.Color;
    vertexTexCoord = a_TextureCoordinates;

    gl_Position = UniformBufferData.Projection * UniformBufferData.View * UniformBufferData.Transform * vec4(a_Position, 1.0);

    // Temporary, fix coordinate system
    gl_Position.y *= -1;
}