#version 430 core

layout (location = 0) out vec4 color;

in vec3 v_Position;
uniform vec4 u_Color;

void main() {
    color = u_Color;
}