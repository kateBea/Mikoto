/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

// Input variables
layout (location = 0) in vec3 inFragmentPos;
layout (location = 1) in vec3 inNormals;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec2 inVertexColor;
layout (location = 4) in vec4 outOutlineColor;

// Output variables
layout (location = 0) out vec4 outColor;

void main() {

    outColor = outOutlineColor;
}