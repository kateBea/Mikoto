#version 430 core

layout (location = 0) out vec4 color;

in vec2 v_TextureCoordinates;

uniform vec4 u_Color;
uniform sampler2D u_TextSampler;

void main() {
    color = mix(u_Color, texture(u_TextSampler, v_TextureCoordinates), .0f);
}