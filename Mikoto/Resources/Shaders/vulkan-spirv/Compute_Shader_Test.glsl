/**************************************************
    Simple Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

// Binding for the Storage Buffer
//layout(std430, binding = 0) buffer DataBuffer {  uint data[]; };

void main() {
    // Get the index of the current work item
    uint index = gl_GlobalInvocationID.x;

    // Store the random number in the buffer
    //data[index] = index + 1;
}