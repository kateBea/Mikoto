#version 450

#include "ShaderBase.glsl"

layout(location = 0) out vec3 v_Direction;
layout(location = 1) flat out float v_Exposure;
layout(location = 2) flat out float v_Gamma;

layout(set = PERPASS_SETINDEX, binding = 0) uniform SkyBoxUBO {
    mat4 View;
    mat4 Projection;
    float Exposure;
    float Gamma;
} u_Camera;

// https://learnopengl.com/code_viewer.php?code=advanced/cubemaps_skybox_data
const vec3 SKYBOX_VERTICES[36] = vec3[](
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0,  1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),

    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),

    vec3(-1.0, -1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3(-1.0, -1.0,  1.0),

    vec3(-1.0,  1.0, -1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0, -1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3( 1.0, -1.0,  1.0)
);

void main() {
    v_Exposure = u_Camera.Exposure;
    v_Gamma = u_Camera.Gamma;

    vec3 pos = vec3(SKYBOX_VERTICES[gl_VertexIndex]);

    // Remove translation from view matrix
    mat4 view = mat4(mat3(u_Camera.View));

    // Vulkan cubemap convention fix
    v_Direction = pos;
    v_Direction.xy *= -1;

    // Force skybox to far plane
    vec4 clip = u_Camera.Projection * view * vec4(pos, 1.0);

    gl_Position = clip;
    //gl_Position = clip.xyww;
}
