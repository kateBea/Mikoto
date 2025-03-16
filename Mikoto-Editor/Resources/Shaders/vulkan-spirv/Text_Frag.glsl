/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

// Input variables
layout(location = 0) in vec4 intVertexColor;
layout(location = 1) in vec2 inVertexTexCoord;

layout (set = 0,binding = 0) uniform sampler2D textSampler;

// Output variables
layout (location = 0) out vec4 outColor;

void main() {
    vec4 texel = texture(textSampler, inVertexTexCoord);
    outColor.rgb = texel.rgb * intVertexColor.rgb * intVertexColor.a * texel.a;
    outColor.a = texel.a * intVertexColor.a;
}