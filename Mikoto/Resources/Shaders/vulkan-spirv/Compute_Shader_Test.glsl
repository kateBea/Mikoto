/**************************************************
    Simple Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

layout(set = 0, binding = 0) buffer RandomBuffer {
    float values[];
};

layout(local_size_x = 256) in;

void main() {
    uint id = gl_GlobalInvocationID.x;

    // For testing
    values[1] = values[0];
}