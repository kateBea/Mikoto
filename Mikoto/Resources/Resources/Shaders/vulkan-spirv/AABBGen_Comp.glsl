/**************************************************
    AABB Compute Shader

    Stage: Compute
    Version: GLSL 4.5.0
**************************************************/

#version 450

#include "ShaderBase.glsl"
#include "ClusteredShading.glsl"

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 ViewMatrix;
    mat4 InverseProjection;
    vec4 GridSize;

    // x = zNear
    // y = zFar
    // z, w = Screen dimensions (width, height)
    vec4 Screen;
} Camera;

layout(std430, set = PERPASS_SETINDEX, binding = 1) restrict buffer ClusterSSBO {
    Cluster clusters[];
};

// Returns the intersection point of an infinite line and a
// plane perpendicular to the Z-axis
vec3 LineIntersectionWithZPlane(vec3 startPoint, vec3 endPoint, float zDistance) {
    vec3 direction = endPoint - startPoint;
    vec3 normal = vec3(0.0, 0.0, -1.0); // plane normal

    // skip check if the line is parallel to the plane.
    float t = (zDistance - dot(normal, startPoint)) / dot(normal, direction);
    return startPoint + t * direction; // the parametric form of the line equation
}

vec3 ScreenToView(vec2 screenCoord)  {
    // Convert screen coords to NDC
    vec2 ndcXY = screenCoord / Camera.Screen.zw * 2.0f - 1.0f;

    // Vulkan NDC: z = 0 at near plane
    vec4 ndc = vec4(ndcXY, 0.0, 1.0);

    vec4 viewCoord = Camera.InverseProjection * ndc;
    viewCoord /= viewCoord.w;

    return viewCoord.xyz;
}

/*
 context: glViewport is referred to as the "screen"
 clusters are built based on a 2d screen-space grid and depth slices.
 Later when shading, it is easy to figure what cluster a fragment is in based on
 gl_FragCoord.xy and the fragment's z depth from camera
*/
void main() {
    uint tileIndex = uint(gl_WorkGroupID.x + (gl_WorkGroupID.y * Camera.GridSize.x) + (gl_WorkGroupID.z * Camera.GridSize.x * Camera.GridSize.y));
    vec2 tileSize = Camera.Screen.zw / Camera.GridSize.xy;

    // tile in screen-space
    vec2 minTile_screenspace = vec2(gl_WorkGroupID.xy * tileSize);
    vec2 maxTile_screenspace = vec2((gl_WorkGroupID.xy + 1) * tileSize);

    // convert tile to view space sitting on the near plane
    vec3 minTile = ScreenToView(minTile_screenspace);
    vec3 maxTile = ScreenToView(maxTile_screenspace);

    float planeNear =
    Camera.Screen.x * pow(Camera.Screen.y / Camera.Screen.x, gl_WorkGroupID.z / float(Camera.GridSize.z));
    float planeFar =
    Camera.Screen.x * pow(Camera.Screen.y / Camera.Screen.x, (gl_WorkGroupID.z + 1) / float(Camera.GridSize.z));

    // the line goes from the eye position in view space (0, 0, 0)
    // through the min/max points of a tile to intersect with a given cluster's near-far planes
    vec3 minPointNear = LineIntersectionWithZPlane(vec3(0, 0, 0), minTile, planeNear);
    vec3 minPointFar = LineIntersectionWithZPlane(vec3(0, 0, 0), minTile, planeFar);
    vec3 maxPointNear = LineIntersectionWithZPlane(vec3(0, 0, 0), maxTile, planeNear);
    vec3 maxPointFar = LineIntersectionWithZPlane(vec3(0, 0, 0), maxTile, planeFar);

    clusters[tileIndex].MinPoint = vec4(min(minPointNear, minPointFar), 0.0);
    clusters[tileIndex].MaxPoint = vec4(max(maxPointNear, maxPointFar), 0.0);
}
