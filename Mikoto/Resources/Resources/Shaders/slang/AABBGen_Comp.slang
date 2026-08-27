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

struct ClusteredParams {
    float4 mGridSize;

    uint64_t mClustersPtr;
    uint64_t mCameraInfoPtr;
};

MKT_DECLARE_PUSH_CONSTANT(ClusteredParams) parameters;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint3 groupID: SV_GroupID,
          uint3 groupIndex: SV_GroupThreadID,
          uint3 dispatchID: SV_DispatchThreadID)
{
    CameraInfo camera = *(CameraInfo *)parameters.mCameraInfoPtr;

    uint tileIndex =
        groupID.x +
        groupID.y * uint(parameters.mGridSize.x) +
        groupID.z * uint(parameters.mGridSize.x * parameters.mGridSize.y);

    float2 tileSize = camera.mScreenDimensions.xy / parameters.mGridSize.xy;

    // Tile in screen-space
    float2 minTileScreen = float2(groupID.xy) * tileSize;
    float2 maxTileScreen = float2(groupID.xy + 1) * tileSize;

    // Convert tile to view space sitting on the near plane
    float3 minTile = ScreenToView(minTileScreen, camera.mScreenDimensions.xy, camera.mInverseProjection);
    float3 maxTile = ScreenToView(maxTileScreen, camera.mScreenDimensions.xy, camera.mInverseProjection);

    // Depth slice slicing
    float nearZ = camera.mPlanes.y;
    float farZ = camera.mPlanes.x;

    float planeNear = nearZ * pow(farZ / nearZ, float(groupID.z) / parameters.mGridSize.z);
    float planeFar = nearZ * pow(farZ / nearZ, float(groupID.z + 1) / parameters.mGridSize.z);

    float3 cameraPosViewSpace = float3(0, 0, 0);

    // Intersection points
    float3 minPointNear = LineIntersectionWithZPlane(cameraPosViewSpace, minTile, planeNear);
    float3 minPointFar = LineIntersectionWithZPlane(cameraPosViewSpace, minTile, planeFar);
    float3 maxPointNear = LineIntersectionWithZPlane(cameraPosViewSpace, maxTile, planeNear);
    float3 maxPointFar = LineIntersectionWithZPlane(cameraPosViewSpace, maxTile, planeFar);

    // Store cluster AABB
    Cluster* clusters = (Cluster*)parameters.mClustersPtr;
    
    //Cluster c;
    //c.mMinPoint = float4(min(minPointNear, minPointFar), 0.0);
    //c.mMaxPoint = float4(max(maxPointNear, maxPointFar), 0.0);

    // error: line 279: OpPhi's number of incoming blocks (3) does not match block's predecessor count (2).
    // % 212 = OpPhi % int % 213 % 20 % int_0 % 17 % int_0 % 17
    //clusters[tileIndex] = c;

    // Need to assign the fields directly to avoid the OpPhi error
    clusters[tileIndex].mMinPoint = float4(min(minPointNear, minPointFar), 0.0);
    clusters[tileIndex].mMaxPoint = float4(max(maxPointNear, maxPointFar), 0.0);
}