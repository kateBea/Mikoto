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

// Cluster-list layouts, screen/depth indexing and light-bound intersection tests. The producer
// and forward shading consumers share the same cluster address calculation.

#pragma once

#include "LightInfo.slang"

// ============================================================================
// CONSTANTS
// ============================================================================

static const uint MAX_LIGHT_CLUSTERS = 256;
static const uint MAX_LIGHTS = 50;


static const float LIGHT_MIN_INFLUENCE = 0.001f;

// ============================================================================
// STRUCTS
// ============================================================================

struct ClusterHeader {
    float4 mCenter;
    float4 mClosestPoint;
    float4 mDistanceSquared;

    float4 mMinPoint;
    float4 mMaxPoint;

    // Active lights affecting this cluster
    uint mCount;
    uint mPadding[3];
};

struct Cluster {
    float4 mCenter;
    float4 mClosestPoint;
    float4 mDistanceSquared;

    float4 mMinPoint;
    float4 mMaxPoint;

    // Active lights affecting this cluster
    uint mCount;
    uint mPadding[3];

    // Indices into the global lights list
    uint mLightIndices[MAX_LIGHT_CLUSTERS];
};

// The fixed-size header and individual indices can be loaded independently.
ClusterHeader LoadClusterHeader( uint64_t address, uint clusterIndex ) {
    return LoadBufferValue<ClusterHeader>( address, clusterIndex * sizeof( Cluster ) );
}

uint LoadClusterLight( uint64_t address, uint clusterIndex, uint lightIndex ) {
    return LoadBufferValue<uint>(
            address, clusterIndex * sizeof( Cluster ) + sizeof( ClusterHeader ) + lightIndex * sizeof( uint ) );
}

// ============================================================================
// HELPERS
// ============================================================================

// Map fragment screen coordinates and view depth to the flattened cluster grid. Logarithmic
// depth slices allocate more cluster detail near the camera.
uint ComputeTileIndex( float3 viewPos, float3 gridSize, float2 planes, float4 fragCoord, float2 screenDimensions ) {
    // Share the exact lookup between forward shading and debug presentation.
    // X/Y use top-left render pixels; Z inverts AABBGen's logarithmic slices.
    // AABBGen uses these slices for perspective and orthographic cameras alike.
    float nearZ = max( planes.x, 1e-4 );
    float farZ = max( planes.y, nearZ + 1e-4 );
    uint zTile = uint( clamp( floor( log( max( -viewPos.z, nearZ ) / nearZ ) * gridSize.z / log( farZ / nearZ ) ), 0.0,
                              gridSize.z - 1 ) );
    float2 tileSize = screenDimensions / gridSize.xy;
    uint xTile = uint( clamp( floor( fragCoord.x / tileSize.x ), 0.0, gridSize.x - 1 ) );
    uint yTile = uint( clamp( floor( fragCoord.y / tileSize.y ), 0.0, gridSize.y - 1 ) );
    return xTile + yTile * uint( gridSize.x ) + zTile * uint( gridSize.x * gridSize.y );
}

float GetPointLightRadius( float intensity ) {
    // dist = sqrt(I / threshold)
    return sqrt( intensity / LIGHT_MIN_INFLUENCE );
}

float GetLightAttenuation( float dist ) {
    return 1.0 / ( dist * dist );
}

float3 GetHeatMapColor( uint count ) {
    float intensity = clamp( float( count ) / float( MAX_LIGHT_CLUSTERS ), 0.0, 1.0 );

    float3 color = float3( 0.0 );
    color.r = intensity;
    color.g = 1.0 - abs( intensity - 0.5 ) * 2.0;
    color.b = 1.0 - intensity;
    return color;
}

float3 LineIntersectionWithZPlane( float3 startPoint, float3 endPoint, float zDistance ) {
    float3 direction = endPoint - startPoint;
    float3 normal = float3( 0.0, 0.0, -1.0 );

    float t = ( zDistance - dot( normal, startPoint ) ) / dot( normal, direction );
    return startPoint + t * direction;
}

float3 ScreenToView( float2 screenCoord, float2 screenDimensions, float4x4 viewInverseProj ) {
    float2 res = screenDimensions;

    float2 xy = screenCoord / res * 2.0 - 1.0;
    xy.y = -xy.y;// Vulkan uses a negative-height viewport; screen y grows downward.
    float4 ndc = float4( xy, 0.0, 1.0 );
    float4 v = mul( viewInverseProj, ndc );
    v /= v.w;

    return v.xyz;
}

// Find the squared distance from a sphere center to the closest point of the cluster AABB. The
// sphere intersects when that distance is within its squared radius.
bool SphereAABBIntersection( float3 center, float radius, float3 aabbMin, float3 aabbMax ) {
    float3 closest = clamp( center, aabbMin, aabbMax );

    float3 diff = closest - center;
    float distSq = dot( diff, diff );

    return distSq <= ( radius * radius );
}

bool TestSphereAABBPointLight( BufferReference<LightInfo> lightPtr, uint32_t index, ClusterHeader cluster,
                               float4x4 view ) {
    float3 center = mul( view, float4( lightPtr[index].mPosition.xyz, 1.0 ) ).xyz;

    float radius = max( lightPtr[index].mRadius, 0.0 );

    float3 aabbMin = cluster.mMinPoint.xyz;
    float3 aabbMax = cluster.mMaxPoint.xyz;

    return SphereAABBIntersection( center, radius, aabbMin, aabbMax );
}

// Conservatively test a spotlight cone against the cluster volume. The test must retain
// possible contributors even when the cone tip lies outside the box.
bool TestSpotAABB( LightInfo light, ClusterHeader cluster, float4x4 view ) {
    float3 apex = mul( view, float4( light.mPosition.xyz, 1 ) ).xyz;
    if ( light.mRadius <= 0 ||
         !SphereAABBIntersection( apex, light.mRadius, cluster.mMinPoint.xyz, cluster.mMaxPoint.xyz ) ) {
        return false;
    }
    float3 axis = normalize( mul( view, float4( light.mDirection.xyz, 0 ) ).xyz );
    float3 center = ( cluster.mMinPoint.xyz + cluster.mMaxPoint.xyz ) * 0.5;
    float radius = length( ( cluster.mMaxPoint.xyz - cluster.mMinPoint.xyz ) * 0.5 );
    float3 offset = center - apex;
    float axial = dot( offset, axis );
    float radial = sqrt( max( dot( offset, offset ) - axial * axial, 0 ) );
    float cosine = clamp( light.mOuterCutOff, 0.001, 1 );
    float sine = sqrt( max( 1 - cosine * cosine, 0 ) );
    // Bounding sphere against the infinite cone and its forward half-space.
    // The preceding finite-range sphere test limits the cone without false negatives.
    return axial >= -radius && radial * cosine - axial * sine <= radius;
}
