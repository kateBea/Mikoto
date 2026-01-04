/**************************************************
    Light Batching Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

#include "ShaderBase.glsl"

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#define MAX_LIGHTS 50
#define MAX_LIGHTS_PER_TILE 64

struct LightInfo {
    vec4 Position;
    vec4 Direction;
    vec4 CutOffValues;
    vec4 Diffuse;
    vec4 AttenuationParams;
    int  ActiveLightType;
};

layout(std140, set = PERPASS_SETINDEX, binding = 0) uniform LightUBO {
    LightInfo Lights[MAX_LIGHTS];
    int ActiveLightsCount;
    int DisplayMode;
} Lighting;

struct TileAABB {
    vec4 min;
    vec4 max;
};

layout(std430, set = PERPASS_SETINDEX, binding = 1) buffer TileAABBs {
    TileAABB aabbs[];
};

layout(std430, set = PERPASS_SETINDEX, binding = 2) buffer TileLightOffsets {
    uint offsets[];
};

layout(std430, set = PERPASS_SETINDEX, binding = 3) buffer TileLightIndices {
    uint indices[];
};

bool SphereIntersectsAABB(vec3 center, float radius, vec3 minP, vec3 maxP) {
    vec3 closest = clamp(center, minP, maxP);
    float dist2 = dot(closest - center, closest - center);
    return dist2 <= radius * radius;
}

void main() {
    uint tile = gl_GlobalInvocationID.x;
    TileAABB box = aabbs[tile];

    uint base = tile * MAX_LIGHTS_PER_TILE;
    uint writeIdx = 0;

    for (uint i = 0; i < Lighting.ActiveLightsCount; ++i) {
        if (Lighting.Lights[i].ActiveLightType <= 0)
        continue;

        vec3 pos = Lighting.Lights[i].Position.xyz;
        float radius = Lighting.Lights[i].AttenuationParams.y;

        if (SphereIntersectsAABB(pos, radius, box.min.xyz, box.max.xyz)) {
            if (writeIdx < MAX_LIGHTS_PER_TILE) {
                indices[base + writeIdx] = i;
                writeIdx++;
            }
        }
    }

    offsets[tile] = writeIdx;
}