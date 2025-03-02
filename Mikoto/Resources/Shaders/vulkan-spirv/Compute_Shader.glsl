/**************************************************
    Simple Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

layout(std140, binding = 0) buffer RandomBuffer {
    uint values[];
};

layout(local_size_x = 10) in;

void main() {
    uint id = gl_GlobalInvocationID.x;

    // For testing
    values[id] = id;
}