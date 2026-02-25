//    Copyright 2026 ケイト
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

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"

[[vk::binding(0, DYNAMIC_BUFFERS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(1, DYNAMIC_RESOURCE_SET_INDEX)]]
RWStructuredBuffer<Cluster, ScalarDataLayout> Clusters;

[[vk::push_constant]]
ClusteredShadingParams u_Params;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 groupID: SV_GroupID,
          uint3 groupIndex: SV_GroupThreadID,
          uint3 dispatchID: SV_DispatchThreadID)
{
    uint tileIndex =
        groupID.x +
        groupID.y * uint(u_Params.GridSize.x) +
        groupID.z * uint(u_Params.GridSize.x * u_Params.GridSize.y);

    float2 tileSize = u_Camera.ScreenDimensions / u_Params.GridSize.xy;

    // Tile in screen-space
    float2 minTile_screen = float2(groupID.xy) * tileSize;
    float2 maxTile_screen = float2(groupID.xy + 1) * tileSize;

    // Convert tile to view space sitting on the near plane
    float3 minTile = ScreenToView(minTile_screen, u_Camera.ScreenDimensions, u_Camera.InverseProjection);
    float3 maxTile = ScreenToView(maxTile_screen, u_Camera.ScreenDimensions, u_Camera.InverseProjection);

    // Depth slice slicing
    float nearZ = u_Camera.Planes.y;
    float farZ = u_Camera.Planes.x;

    float planeNear = nearZ * pow(farZ / nearZ, float(groupID.z) / u_Params.GridSize.z);
    float planeFar = nearZ * pow(farZ / nearZ, float(groupID.z + 1) / u_Params.GridSize.z);

    float3 cameraPosViewSpace = float3(0, 0, 0);

    // Intersection points
    float3 minPointNear = LineIntersectionWithZPlane(cameraPosViewSpace, minTile, planeNear);
    float3 minPointFar = LineIntersectionWithZPlane(cameraPosViewSpace, minTile, planeFar);
    float3 maxPointNear = LineIntersectionWithZPlane(cameraPosViewSpace, maxTile, planeNear);
    float3 maxPointFar = LineIntersectionWithZPlane(cameraPosViewSpace, maxTile, planeFar);

    // Store cluster AABB
    Clusters[tileIndex].MinPoint = float4(min(minPointNear, minPointFar), 0.0);
    Clusters[tileIndex].MaxPoint = float4(max(maxPointNear, maxPointFar), 0.0);
}