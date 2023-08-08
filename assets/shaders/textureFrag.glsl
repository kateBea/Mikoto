#version 430 core

layout (location = 0) out vec4 color;

in vec2 textureCoordinates;
uniform sampler2D u_TextSampler;


void main() {
    color = texture(u_TextSampler, textureCoordinates);
}