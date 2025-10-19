#version 450

// This shader generates a triangle centered without a vertex buffer.
// It uses gl_VertexIndex (0, 1, 2) to produce 3 vertices.

layout(location = 0) out vec3 v_Color;

void main()
{
    vec2 positions[3] = vec2[](
    vec2(0.0, 0.5), // Top
    vec2(-0.5, -0.5), // Bottom-left
    vec2(0.5, -0.5)// Bottom-right
    );

    // Simple color interpolation per vertex
    vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // Red
    vec3(0.0, 1.0, 0.0), // Green
    vec3(0.0, 0.0, 1.0)// Blue
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    v_Color = colors[gl_VertexIndex];
}