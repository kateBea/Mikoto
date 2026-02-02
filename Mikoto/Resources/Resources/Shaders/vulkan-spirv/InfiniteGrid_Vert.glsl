#version 450

#include "ShaderBase.glsl"

layout(set = PERPASS_SETINDEX, binding = 0) uniform SceneMatrices {
    mat4 View;
    mat4 Proj;
    vec4 CameraPos;
} v_CameraInfo;

layout(location = 0) out vec3 outCameraPos;
layout(location = 1) out vec2 outCoords;

const float gridSize = 100.0;

void main() {
    const vec4 positions[4] = {
        vec4(-1.0, 0.0,  1.0, 1.0),
        vec4( 1.0, 0.0,  1.0, 1.0),
        vec4(-1.0, 0.0, -1.0, 1.0),
        vec4( 1.0, 0.0, -1.0, 1.0)
    };

    vec4 worldPos = vec4(positions[gl_VertexIndex]);
    worldPos.xyz *= gridSize;

    // Follow the camera in XZ so the grid appears infinite
    worldPos.xz += v_CameraInfo.CameraPos.xz;

    // Force grid to fixed height
    float gridHeight = 0.0f;
    worldPos.y = gridHeight;

    outCameraPos = v_CameraInfo.CameraPos.xyz;
    outCoords = worldPos.xz;

    gl_Position = v_CameraInfo.Proj * v_CameraInfo.View * worldPos;
}
