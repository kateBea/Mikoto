/**************************************************
    Light Culling Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

#include "ShaderBase.glsl"

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint idx = gl_GlobalInvocationID.x;

    // Fake "culling"

}