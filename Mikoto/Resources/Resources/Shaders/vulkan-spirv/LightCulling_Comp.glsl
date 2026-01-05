/**************************************************
    Light Culling Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

#include "ShaderBase.glsl"
#include "ClusteredShading.glsl"

#define LOCAL_SIZE 128
layout(local_size_x = LOCAL_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 ViewMatrix;
    mat4 InverseProjection;
    vec4 GridSize;

    // x = zNear
    // y = zFar
    // z, w = Screen dimensions (width, height)
    vec4 Screen;
} Camera;

layout(std430, set = PERPASS_SETINDEX, binding = 1) buffer ClusterSSBO  {
    Cluster Clusters[];
};

layout(std430, set = PERPASS_SETINDEX, binding = 2) buffer LightSSBO {
    LightInfo Lights[];
};

bool SphereAABBIntersection(vec3 center, float radius, vec3 aabbMin, vec3 aabbMax)  {
    // closest point on the AABB to the sphere center
    vec3 closestPoint = clamp(center, aabbMin, aabbMax);
    // squared distance between the sphere center and closest point
    float distanceSquared = dot(closestPoint - center, closestPoint - center);
    return distanceSquared <= radius * radius;
}

// this just unpacks data for sphereAABBIntersection
bool TestSphereAABB(uint i, Cluster cluster)  {
    vec3 center = vec3(Camera.ViewMatrix * Lights[i].Position);
    float radius = Lights[i].Radius;

    vec3 aabbMin = cluster.MinPoint.xyz;
    vec3 aabbMax = cluster.MaxPoint.xyz;

    return SphereAABBIntersection(center, radius, aabbMin, aabbMax);
}

void main() {
    uint lightCount = Lights.length();
    uint index = gl_WorkGroupID.x * LOCAL_SIZE + gl_LocalInvocationID.x;
    Cluster cluster = Clusters[index];

    // we need to reset count because culling runs every frame.
    // otherwise it would accumulate.
    cluster.Count = 0;

    for (uint i = 0; i < lightCount; ++i)
    {
        if (TestSphereAABB(i, cluster) && cluster.Count < 100)
        {
            cluster.LightIndices[cluster.Count] = i;
            cluster.Count++;
        }
    }
    Clusters[index] = cluster;
}
