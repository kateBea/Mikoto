#version 450

#include "ShaderBase.glsl"

layout(set = PERPASS_SETINDEX, binding = 0) uniform DirectionalShadowMapUBO {
    mat4 LightView;
    mat4 LightProjection;
} u_CamerasInfo;

layout(std430, set = PERPASS_SETINDEX, binding = 1) readonly buffer MeshInfoSSBO {
    MeshInfo Meshes[];
};

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord;

void main()  {
    MeshInfo meshInfo = Meshes[gl_InstanceIndex];
    mat4 model = mat4(meshInfo.Transform);

    gl_Position =  u_CamerasInfo.LightProjection * u_CamerasInfo.LightView * model * vec4(a_Position, 1.0);
}