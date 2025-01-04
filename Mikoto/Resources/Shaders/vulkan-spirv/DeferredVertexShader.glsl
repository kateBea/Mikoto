#version 450

// Input data (geometry information)
layout(location = 0) out vec2 textureCoordinates;

void main() {
    textureCoordinates = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(textureCoordinates * 2.0f - 1.0f, 0.0f, 1.0f);
}