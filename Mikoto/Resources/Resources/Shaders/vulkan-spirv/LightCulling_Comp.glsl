//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**************************************************
    Light Culling Compute Shader
    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

#extension GL_EXT_scalar_block_layout : require

#include "ShaderBase.glsl"
#include "ClusteredShading.glsl"

#define LOCAL_SIZE 128
layout(local_size_x = LOCAL_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(scalar, set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 ViewMatrix;
    mat4 InverseProjection;

    vec4 GridSize;
    vec4 ViewPosition;

    // xy = Planes, zw = ScreenDimensions
    vec4 Screen;

    // x = show heat map
    vec4 ShowHeatMap;
} Camera;

layout(std430, scalar, set = PERPASS_SETINDEX, binding = 1) buffer ClusterSSBO  {
    Cluster Clusters[];
};

layout(std430, scalar, set = PERPASS_SETINDEX, binding = 2) buffer LightSSBO {
    LightInfo Lights[];
};

layout(scalar, set = PERPASS_SETINDEX, binding = 3) uniform LightCullingUBO {
    uint ActiveLightCount;
} u_CullingParams;


bool SphereAABBIntersection(vec3 center, float radius, vec3 aabbMin, vec3 aabbMax)  {
    // closest point on the AABB to the sphere center
    vec3 closestPoint = clamp(center, aabbMin, aabbMax);

    // squared distance between the sphere center and closest point
    float distanceSquared = dot(closestPoint - center, closestPoint - center);

    return distanceSquared <= radius * radius;
}

// this just unpacks data for sphereAABBIntersection
bool TestSphereAABBPointLight(uint i, Cluster cluster)  {
    vec3 center = (Camera.ViewMatrix * vec4(Lights[i].Position.xyz, 1.0)).xyz;
    float radius = GetPointLightRadius(Lights[i].Intensity);

    vec3 aabbMin = cluster.MinPoint.xyz;
    vec3 aabbMax = cluster.MaxPoint.xyz;

    return SphereAABBIntersection(center, radius, aabbMin, aabbMax);
}

void main() {
    uint index = gl_WorkGroupID.x * LOCAL_SIZE + gl_LocalInvocationID.x;

    if (index >= Clusters.length())
        return;

    Cluster cluster = Clusters[index];

    // we need to reset count because culling runs every frame.
    // otherwise it would accumulate.
    cluster.Count = 0;

    uint lightCount = uint(u_CullingParams.ActiveLightCount);
    // I go up until lightCount because I want to find amongts all
    // lights in the scene which ones affect this cluster
    for (uint i = 0; i < lightCount; ++i)
    {
        bool visible = false;

        switch (Lights[i].ActiveLightType)
        {
            case LIGHT_TYPE_POINT:
                visible = TestSphereAABBPointLight(i, cluster);
                break;

            case LIGHT_TYPE_SPOT:
                // TODO: improve. https://simoncoenen.com/blog/programming/graphics/SpotlightCulling
                visible = TestSphereAABBPointLight(i, cluster);
                break;

            case LIGHT_TYPE_DIRECTIONAL:
            // All clusters are affected by directional light
            // Reconsider for clusters where there is no geometry
                visible = true;
                break;
        }

        if (visible && cluster.Count < MAX_LIGHT_CLUSTERS)  {
            cluster.LightIndices[cluster.Count++] = i;
        }
    }

    Clusters[index] = cluster;
}
