#version 430 core

layout (location = 0) out vec4 outputColor;

in vec3 vertexColor;
in vec2 vertexTexCoord;

uniform vec4 u_Color;
uniform sampler2D u_TextSampler;

void main() {
    outputColor = mix(u_Color, texture(u_TextSampler, vertexTexCoord), .0f);
}