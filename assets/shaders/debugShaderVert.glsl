#version 430 core
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec2 a_TextureCoordinates;

out vec2 v_TextureCoordinates;

uniform mat4 u_ProjectionView;
uniform mat4 u_Transform;

void main() {
    v_TextureCoordinates = a_TextureCoordinates;
    gl_Position = u_ProjectionView * u_Transform * vec4(a_Position, 1.0);
}