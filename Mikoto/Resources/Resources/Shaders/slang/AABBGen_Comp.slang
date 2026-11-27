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

// Build a view-space AABB for each screen tile and logarithmic depth slice. Light culling tests
// light bounds against these cluster volumes.

#pragma once

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct ClusteredParams {
    float4 mGridSize;

    RWBufferReference<Cluster> mClustersPtr;
    BufferReference<CameraInfo> mCameraInfoPtr;
};

MKT_DECLARE_PUSH_CONSTANT( ClusteredParams, parameters );

// Intersect the corner camera ray with the requested view-depth plane. Four corners at each
// slice boundary enclose the cluster frustum segment.
float3 SliceCorner( float2 pixel, float depth, CameraInfo camera ) {
    // Convert the screen-space corner to normalized device coordinates.
    float2 ndc = pixel / camera.mScreenDimensions.xy * 2 - 1;
    ndc.y = -ndc.y;

    // Unproject both ends of the view ray so orthographic cameras work too.
    float4 nearH = mul( camera.mInverseProjection, float4( ndc, 0, 1 ) );
    float4 farH = mul( camera.mInverseProjection, float4( ndc, 1, 1 ) );

    float3 nearPoint = nearH.xyz / nearH.w;
    float3 farPoint = farH.xyz / farH.w;

    // Intersect the ray with the slice plane. Forward is negative Z in view space.
    float t = ( -depth - nearPoint.z ) / ( farPoint.z - nearPoint.z );
    return lerp( nearPoint, farPoint, t );
}

[shader( "compute" )]
[numthreads( 1, 1, 1 )]
void main( uint3 groupID : SV_GroupID ) {
    uint3 grid = uint3( parameters.mGridSize.xyz );
    if ( any( groupID >= grid ) ) {
        return;
    }

    CameraInfo camera = parameters.mCameraInfoPtr[0];

    uint tileIndex =
        groupID.x +
        groupID.y * grid.x +
        groupID.z * grid.x * grid.y;

    float2 tileSize = camera.mScreenDimensions.xy / parameters.mGridSize.xy;

    // Tile in screen-space: keep all four corners when building the bounds.
    float2 minTileScreen = float2( groupID.xy ) * tileSize;
    float2 maxTileScreen = ( float2( groupID.xy ) + 1 ) * tileSize;
    float2 topRightScreen = float2( maxTileScreen.x, minTileScreen.y );
    float2 bottomLeftScreen = float2( minTileScreen.x, maxTileScreen.y );

    // Depth slice slicing: distribute the planes logarithmically from near to far.
    float nearZ = max( camera.mPlanes.x, 1e-4 );
    float farZ = max( camera.mPlanes.y, nearZ + 1e-4 );

    float planeNear = nearZ * pow( farZ / nearZ, float( groupID.z ) / grid.z );
    float planeFar = nearZ * pow( farZ / nearZ, float( groupID.z + 1 ) / grid.z );

    // Intersection points on the near slice plane, in view space.
    float3 minPointNear = SliceCorner( minTileScreen, planeNear, camera );
    float3 topRightNear = SliceCorner( topRightScreen, planeNear, camera );
    float3 bottomLeftNear = SliceCorner( bottomLeftScreen, planeNear, camera );
    float3 maxPointNear = SliceCorner( maxTileScreen, planeNear, camera );

    // Intersection points on the far slice plane, in view space.
    float3 minPointFar = SliceCorner( minTileScreen, planeFar, camera );
    float3 topRightFar = SliceCorner( topRightScreen, planeFar, camera );
    float3 bottomLeftFar = SliceCorner( bottomLeftScreen, planeFar, camera );
    float3 maxPointFar = SliceCorner( maxTileScreen, planeFar, camera );

    // Bounds of each plane, then bounds enclosing the entire depth slice.
    float3 nearMin = min( min( minPointNear, topRightNear ), min( bottomLeftNear, maxPointNear ) );
    float3 nearMax = max( max( minPointNear, topRightNear ), max( bottomLeftNear, maxPointNear ) );

    float3 farMin = min( min( minPointFar, topRightFar ), min( bottomLeftFar, maxPointFar ) );
    float3 farMax = max( max( minPointFar, topRightFar ), max( bottomLeftFar, maxPointFar ) );

    float3 minimum = min( nearMin, farMin );
    float3 maximum = max( nearMax, farMax );

    // Store the cluster AABB and reset its light count without writing the light list.
    ClusterHeader cluster = ( ClusterHeader )0;
    cluster.mMinPoint = float4( minimum, 0 );
    cluster.mMaxPoint = float4( maximum, 0 );
    cluster.mCenter = float4( ( minimum + maximum ) * .5, 0 );
    cluster.mCount = 0;
    cluster.mPadding[0] = 0;

    StoreBufferValue<ClusterHeader>( parameters.mClustersPtr.address, tileIndex * sizeof( Cluster ), cluster );
}
