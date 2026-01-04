/**************************************************
    Simple Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

#include "ShaderBase.glsl"

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#define TILE_SIZE 16

layout(set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 Projection;
    mat4 InvProjection;
    vec2 ScreenSize;
} Camera;

struct TileAABB {
    vec4 min;
    vec4 max;
};

layout(std430, set = PERPASS_SETINDEX, binding = 1) buffer TileAABBs {
    TileAABB aabbs[];
};

vec3 Unproject(vec2 ndc, float z) {
    vec4 p = Camera.InvProjection * vec4(ndc, z, 1.0);
    return p.xyz / p.w;
}

void main() {
    uint tile = gl_GlobalInvocationID.x;

    uint tilesX = uint(Camera.ScreenSize.x) / TILE_SIZE;
    uint tileX = tile % tilesX;
    uint tileY = tile / tilesX;

    vec2 pixelMin = vec2(tileX, tileY) * TILE_SIZE;
    vec2 pixelMax = pixelMin + TILE_SIZE;

    vec2 ndcMin = (pixelMin / Camera.ScreenSize) * 2.0 - 1.0;
    vec2 ndcMax = (pixelMax / Camera.ScreenSize) * 2.0 - 1.0;

    // Near & far corners
    vec3 corners[8];
    corners[0] = Unproject(ndcMin, -1.0);
    corners[1] = Unproject(vec2(ndcMax.x, ndcMin.y), -1.0);
    corners[2] = Unproject(ndcMax, -1.0);
    corners[3] = Unproject(vec2(ndcMin.x, ndcMax.y), -1.0);

    corners[4] = Unproject(ndcMin, 1.0);
    corners[5] = Unproject(vec2(ndcMax.x, ndcMin.y), 1.0);
    corners[6] = Unproject(ndcMax, 1.0);
    corners[7] = Unproject(vec2(ndcMin.x, ndcMax.y), 1.0);

    vec3 minP = corners[0];
    vec3 maxP = corners[0];

    for (int i = 1; i < 8; ++i) {
        minP = min(minP, corners[i]);
        maxP = max(maxP, corners[i]);
    }

    aabbs[tile].min = vec4(minP, 1.0);
    aabbs[tile].max = vec4(maxP, 1.0);
}
