#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 ProjectionView;
    mat4 Transform;
} TransformData;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TextureCoordinates;

layout(location = 0) out vec3 vertexColor;
layout(location = 1) out vec2 vertexTexCoord;

void main() {
    vertexColor = a_Color;
    vertexTexCoord = a_TextureCoordinates;
    gl_Position = TransformData.ProjectionView * TransformData.Transform * vec4(a_Position, 1.0);
}