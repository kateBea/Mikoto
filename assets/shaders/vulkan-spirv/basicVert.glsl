#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 ProjectionView;
    mat4 Transform;
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
    vertexColor = vec4(a_Color, 1.0f);
    vertexTexCoord = a_TextureCoordinates;

    gl_Position = UniformBufferData.ProjectionView * UniformBufferData.Transform * vec4(a_Position, 1.0);

    // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
    gl_Position.y = -gl_Position.y;

}