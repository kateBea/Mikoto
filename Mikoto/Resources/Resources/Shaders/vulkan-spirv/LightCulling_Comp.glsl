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
    vec4 ViewPosition;

    // xy = Planes, zw = ScreenDimensions
    vec4 Screen;

    // x = show heat map
    vec4 ShowHeatMap;
} Camera;

layout(std430, set = PERPASS_SETINDEX, binding = 1) buffer ClusterSSBO  {
    Cluster Clusters[];
};

layout(std430, set = PERPASS_SETINDEX, binding = 2) buffer LightSSBO {
    LightInfo Lights[];
};

layout(set = PERPASS_SETINDEX, binding = 3) uniform LightCullingUBO {
    // x = Active light count
    vec4 LightInfo;
} CullingInfo;

bool SphereAABBIntersection(vec3 center, float radius, vec3 aabbMin, vec3 aabbMax)  {
    // closest point on the AABB to the sphere center
    vec3 closestPoint = clamp(center, aabbMin, aabbMax);

    // squared distance between the sphere center and closest point
    float distanceSquared = dot(closestPoint - center, closestPoint - center);

    return distanceSquared <= radius * radius;
}

// this just unpacks data for sphereAABBIntersection
bool TestSphereAABBPointLight(uint i, Cluster cluster)  {
    vec3 center = vec3(Camera.ViewMatrix * Lights[i].Position);
    float radius = Lights[i].Radius;

    vec3 aabbMin = cluster.MinPoint.xyz;
    vec3 aabbMax = cluster.MaxPoint.xyz;

    return SphereAABBIntersection(center, radius, aabbMin, aabbMax);
}

bool ConeCenterTest(vec3 lightPos, vec3 lightDir, float cosOuter, vec3 aabbMin, vec3 aabbMax) {
    vec3 center = 0.5 * (aabbMin + aabbMax);
    vec3 L = center - lightPos;

    float len2 = dot(L, L);
    if (len2 == 0.0) {
        return true;
    }

    float invLen = inversesqrt(len2);
    float cosTheta = dot(L * invLen, lightDir);

    return cosTheta >= cosOuter;
}

bool ConeAABBIntersect(vec3 coneTip, vec3 coneDir, float cosOuter, vec3 aabbMin, vec3 aabbMax) {
    // Compute closest point on AABB to the cone axis
    // (projected onto plane perpendicular to axis)
    vec3 boxCenter = 0.5 * (aabbMin + aabbMax);
    vec3 boxExtent = 0.5 * (aabbMax - aabbMin);

    // Vector from cone tip to box center
    vec3 v = boxCenter - coneTip;

    // Project v onto cone axis
    float d = dot(v, coneDir);

    // Box completely behind the cone
    if (d <= 0.0) {
        return false;
    }

    // Distance from axis to box center
    vec3 closestPoint = boxCenter - coneDir * d;

    // Conservative radius of box around center
    float r = boxExtent.x + boxExtent.y + boxExtent.z;

    float dist2 = dot(closestPoint, closestPoint);
    float coneRadiusAtD = d * sqrt(max(1.0 - cosOuter * cosOuter, 0.0));

    return dist2 <= (coneRadiusAtD + r) * (coneRadiusAtD + r);
}

bool TestSphereAABBSpotLight(uint i, Cluster cluster)  {
    vec3 lightPosVS = vec3(Camera.ViewMatrix * Lights[i].Position);
    vec3 lightDirVS = normalize(vec3(Camera.ViewMatrix * vec4(Lights[i].Direction.xyz, 0.0)));

    float radius = Lights[i].Radius;
    float cosOuter = cos(Lights[i].OuterCutOff); // MUST be cos(angle)

    vec3 aabbMin = cluster.MinPoint.xyz;
    vec3 aabbMax = cluster.MaxPoint.xyz;

    // 1. Sphere vs AABB
    if (!SphereAABBIntersection(lightPosVS, radius, aabbMin, aabbMax)) {
        return false;
    }

    // 2. Cheap cone center test
    if (!ConeCenterTest(lightPosVS, lightDirVS, cosOuter, aabbMin, aabbMax)) {
        return false;
    }

    // 3. Tight cone–AABB test
    return ConeAABBIntersect(lightPosVS, lightDirVS, cosOuter, aabbMin, aabbMax);
}

void main() {
    uint lightCount = uint(CullingInfo.LightInfo.x);
    uint index = gl_WorkGroupID.x * LOCAL_SIZE + gl_LocalInvocationID.x;

    if (index >= Clusters.length())
        return;

    Cluster cluster = Clusters[index];

    // we need to reset count because culling runs every frame.
    // otherwise it would accumulate.
    cluster.Count = 0;

    // I go up until lightCount because I want to find amongts all
    // lights in the scene which ones affect this cluster
    for (uint i = 0; i < lightCount; ++i)
    {
        bool visible = false;

        switch (Lights[i].ActiveLightType)
        {
            case LIGHT_TYPE_POINT:
                visible = TestSphereAABBPointLight(i, cluster);
                visible = true;
                break;

            case LIGHT_TYPE_SPOT:
                visible = TestSphereAABBSpotLight(i, cluster);
                break;

            case LIGHT_TYPE_DIRECTIONAL:
                visible = true;
                break;
        }

        if (visible && cluster.Count < MAX_LIGHT_CLUSTERS)  {
            cluster.LightIndices[cluster.Count++] = i;
        }
    }

    Clusters[index] = cluster;
}
