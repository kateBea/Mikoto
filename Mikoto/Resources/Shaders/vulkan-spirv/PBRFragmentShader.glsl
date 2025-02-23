/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

// Input variables
layout (location = 0) in vec3 fragmentPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec3 inNormals;
layout (location = 3) in vec2 inTexCoord;
layout (location = 4) in vec2 inVertexColor;

// Output variables
layout (location = 0) out vec4 fragmentColor;

void main() {

    fragmentColor = vec4(inColor.rgb, 1.0);
}