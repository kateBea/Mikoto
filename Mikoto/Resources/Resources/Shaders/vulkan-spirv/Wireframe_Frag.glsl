/**************************************************
    Shader for the standard material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

// Output variables
layout (location = 0) out vec4 outColor;

void main() {

    outColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}