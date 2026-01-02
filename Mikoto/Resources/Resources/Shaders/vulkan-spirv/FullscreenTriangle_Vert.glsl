#version 450

layout(location = 0) out vec2 v_TexCoord;
layout(location = 1) out vec3 vColor;

void main()
{
    // 4 vertices of a square (two triangles)
    const vec2 positions[4] = vec2[](
    vec2(-0.5, -0.5), // bottom-left
    vec2( 0.5, -0.5), // bottom-right
    vec2(-0.5,  0.5), // top-left
    vec2( 0.5,  0.5)  // top-right
    );

    const vec2 texCoords[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0)
    );

    const vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);

    v_TexCoord = texCoords[gl_VertexIndex];
    vColor = colors[gl_VertexIndex];
}