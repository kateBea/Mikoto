#version 430 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec2 a_TextureCoordinates;

uniform mat4 u_ProjectionView;
uniform mat4 u_Transform;

out vec2 textureCoordinates;

void main() {
    textureCoordinates = a_TextureCoordinates;
    gl_Position = u_ProjectionView * u_Transform * vec4(a_Position, 1.0);
}