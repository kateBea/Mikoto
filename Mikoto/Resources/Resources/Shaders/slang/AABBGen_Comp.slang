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
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"

MKT_DECLARE_STRUCTURED_SRV SRV_Buffers;
MKT_DECLARE_STRUCTURED_UAV UAV_Buffers;

struct ClusteredParams {
    float4 GridSize;
    uint ClusterBufferID;
    uint CameraInfoBufferID;
};

MKT_DECLARE_PUSH_CONSTANT(ClusteredParams) Parameters;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 groupID: SV_GroupID,
          uint3 groupIndex: SV_GroupThreadID,
          uint3 dispatchID: SV_DispatchThreadID)
{
    // Get the buffers
    RWByteAddressBuffer clusters = UAV_Buffers[Parameters.ClusterBufferID];
    CameraInfo camera = SRV_Buffers[Parameters.CameraInfoBufferID].Read<CameraInfo>(0, 0);

    uint tileIndex =
        groupID.x +
        groupID.y * uint(Parameters.GridSize.x) +
        groupID.z * uint(Parameters.GridSize.x * Parameters.GridSize.y);

    float2 tileSize = camera.ScreenDimensions.xy / Parameters.GridSize.xy;

    // Tile in screen-space
    float2 minTile_screen = float2(groupID.xy) * tileSize;
    float2 maxTile_screen = float2(groupID.xy + 1) * tileSize;

    // Convert tile to view space sitting on the near plane
    float3 minTile = ScreenToView(minTile_screen, camera.ScreenDimensions.xy, camera.InverseProjection);
    float3 maxTile = ScreenToView(maxTile_screen, camera.ScreenDimensions.xy, camera.InverseProjection);

    // Depth slice slicing
    float nearZ = camera.Planes.y;
    float farZ = camera.Planes.x;

    float planeNear = nearZ * pow(farZ / nearZ, float(groupID.z) / Parameters.GridSize.z);
    float planeFar = nearZ * pow(farZ / nearZ, float(groupID.z + 1) / Parameters.GridSize.z);

    float3 cameraPosViewSpace = float3(0, 0, 0);

    // Intersection points
    float3 minPointNear = LineIntersectionWithZPlane(cameraPosViewSpace, minTile, planeNear);
    float3 minPointFar = LineIntersectionWithZPlane(cameraPosViewSpace, minTile, planeFar);
    float3 maxPointNear = LineIntersectionWithZPlane(cameraPosViewSpace, maxTile, planeNear);
    float3 maxPointFar = LineIntersectionWithZPlane(cameraPosViewSpace, maxTile, planeFar);

    // Store cluster AABB
    Cluster c;
    c.MinPoint = float4(min(minPointNear, minPointFar), 0.0);
    c.MaxPoint = float4(max(maxPointNear, maxPointFar), 0.0);

    clusters.Write(0, tileIndex, c);
}