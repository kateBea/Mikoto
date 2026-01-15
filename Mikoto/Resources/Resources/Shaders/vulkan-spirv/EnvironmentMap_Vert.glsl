#version 450

#include "ShaderBase.glsl"

layout(location = 0) out vec3 v_LocalPos;

layout(set = PERPASS_SETINDEX, binding = 0) uniform SkyBoxUBO {
    mat4 View;
    mat4 Projection;
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
    vec3 localPos = vec3(SKYBOX_VERTICES[gl_VertexIndex]);

    v_LocalPos = localPos;
    gl_Position =  u_Camera.Projection * u_Camera.View * vec4(localPos, 1.0);
}
