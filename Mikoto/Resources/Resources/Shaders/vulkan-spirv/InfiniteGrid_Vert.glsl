#version 450

#include "ShaderBase.glsl"

layout(set = PERPASS_SETINDEX, binding = 0) uniform SceneMatrices {
    mat4 View;
    mat4 Proj;
    vec4 CameraPos;
} u_CameraInfo;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outCameraWorldPos;

const float gridSize = 100.0;

const vec3 positions[4] = {
    vec3(-1.0, 0.0, -1.0),      // bottom left
    vec3( 1.0, 0.0, -1.0),      // bottom right
    vec3( 1.0, 0.0,  1.0),      // top right
    vec3(-1.0, 0.0,  1.0)       // top left
};

const int Indices[6] = int[6](0, 2, 1, 2, 0, 3);

void main() {

    int index = Indices[gl_VertexIndex];

    vec3 worldPos = vec3(positions[index] * gridSize);

    worldPos.x += u_CameraInfo.CameraPos.x;
    worldPos.z += u_CameraInfo.CameraPos.z;

    vec4 vPos4 = vec4(worldPos, 1.0);

    gl_Position = u_CameraInfo.Proj * u_CameraInfo.View * vPos4;

    outWorldPos = worldPos;
    outCameraWorldPos = u_CameraInfo.CameraPos.xyz;
}
