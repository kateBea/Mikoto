#version 450

#include "ShaderBase.glsl"

layout(set = PERPASS_SETINDEX, binding = 0) uniform DirectionalShadowMapUBO {
    mat4 DepthMVP;
} u_CamerasInfo;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord;

void main()  {
    gl_Position =  u_CamerasInfo.DepthMVP * vec4(a_Position, 1.0);
}