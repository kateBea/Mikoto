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
    AABB Compute Shader
    Stage: Compute
    Version: GLSL 4.5.0
    @ref https://github.com/DaveH355/clustered-shading
**************************************************/

#version 450

#include "ShaderBase.glsl"
#include "ClusteredShading.glsl"

#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(scalar, set = PERPASS_SETINDEX, binding = 0) uniform CameraUBO {
    mat4 Projection;
    mat4 ViewMatrix;
    mat4 InverseProjection;
    vec4 ViewPosition;
    vec2 Planes;
    vec2 ScreenDimensions;
} u_Camera;

layout(std430, scalar,set = STATIC_SETINDEX, binding = 1) buffer ClusterSSBO {
    Cluster clusters[];
};

layout(scalar, push_constant) uniform ClusteredShadingParams {
    vec4 GridSize;
    uint ShowHeatMap;
    uint ActiveLightCount;
} u_Parameters;

// Returns the intersection point of an infinite line and a
// plane perpendicular to the Z-axis
vec3 LineIntersectionWithZPlane(vec3 startPoint, vec3 endPoint, float zDistance) {
    vec3 direction = endPoint - startPoint;
    vec3 normal = vec3(0.0, 0.0, -1.0); // plane normal

    // skip check if the line is parallel to the plane.
    float t = (zDistance - dot(normal, startPoint)) / dot(normal, direction);
    return startPoint + t * direction; // the parametric form of the line equation
}

vec3 ScreenToView(vec2 screenCoord) {
    vec4 ndc = vec4(screenCoord / u_Camera.ScreenDimensions * 2.0 - 1.0, 0.0f, 1.0);

    vec4 viewCoord = u_Camera.InverseProjection * ndc;
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
    uint tileIndex = uint(gl_WorkGroupID.x + (gl_WorkGroupID.y * u_Parameters.GridSize.x) + (gl_WorkGroupID.z * u_Parameters.GridSize.x * u_Parameters.GridSize.y));
    vec2 tileSize = u_Camera.ScreenDimensions / u_Parameters.GridSize.xy;

    // tile in screen-space
    vec2 minTile_screenspace = vec2(gl_WorkGroupID.xy * tileSize);
    vec2 maxTile_screenspace = vec2((gl_WorkGroupID.xy + 1) * tileSize);

    // convert tile to view space sitting on the near plane
    vec3 minTile = ScreenToView(minTile_screenspace);
    vec3 maxTile = ScreenToView(maxTile_screenspace);

    float planeNear = u_Camera.Planes.y * pow(u_Camera.Planes.x / u_Camera.Planes.y, gl_WorkGroupID.z / float(u_Parameters.GridSize.z));
    float planeFar = u_Camera.Planes.y * pow(u_Camera.Planes.x / u_Camera.Planes.y, (gl_WorkGroupID.z + 1) / float(u_Parameters.GridSize.z));

    // the line goes from the eye position in view space (0, 0, 0)
    // through the min/max points of a tile to intersect with a given cluster's near-far planes
    vec3 cameraPositionViewSpace = vec3(0, 0, 0);

    vec3 minPointNear = LineIntersectionWithZPlane(cameraPositionViewSpace, minTile, planeNear);
    vec3 minPointFar = LineIntersectionWithZPlane(cameraPositionViewSpace, minTile, planeFar);
    vec3 maxPointNear = LineIntersectionWithZPlane(cameraPositionViewSpace, maxTile, planeNear);
    vec3 maxPointFar = LineIntersectionWithZPlane(cameraPositionViewSpace, maxTile, planeFar);

    clusters[tileIndex].MinPoint = vec4(min(minPointNear, minPointFar), 0.0);
    clusters[tileIndex].MaxPoint = vec4(max(maxPointNear, maxPointFar), 0.0);
}
