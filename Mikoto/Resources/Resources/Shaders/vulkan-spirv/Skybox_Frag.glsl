/**************************************************
    Shader for the SkyBox.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderBase.glsl"

layout (location = 0) in vec3 v_Direction;

layout (location = 0) out vec4 o_Color;

layout (set = PERPASS_SETINDEX, binding = 1) uniform samplerCube u_Skybox;

void main() {
    o_Color = texture(u_Skybox, v_Direction);
}
