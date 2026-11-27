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

// Assign scene lights to view-space clusters. Each invocation builds one bounded light list and
// records overflow; forward material shading reads the resulting cluster indices.

#pragma once

static const uint kLocalSize = 128;

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"
#include "base/Memory_Helpers.slang"

MKT_DECLARE_STRUCTURED_SRV srvBuffers;
MKT_DECLARE_STRUCTURED_UAV uavBuffers;

struct ClusteredParams {
    float4 mGridSize;

    BufferReference<LightInfo> mLightsPtr;
    RWBufferReference<Cluster> mClustersPtr;
    BufferReference<CameraInfo> mCameraPtr;

    uint mClusterCount;
    uint mActiveLightCount;
};

MKT_DECLARE_PUSH_CONSTANT( ClusteredParams, parameters );

[shader( "compute" )]
[numthreads( kLocalSize, 1, 1 )]
void main( uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint3 dispatchID : SV_DispatchThreadID ) {
    // Get the buffers
    CameraInfo camera = parameters.mCameraPtr[0];
    BufferReference<LightInfo> lights = parameters.mLightsPtr;
    RWBufferReference<Cluster> clusters = parameters.mClustersPtr;

    uint index = groupID.x * kLocalSize + groupThreadID.x;
    if ( index >= parameters.mClusterCount ) {
        return;
    }

    // we need to reset count because culling runs every frame,
    // otherwise it would accumulate.
    ClusterHeader cluster = LoadRWBufferValue<ClusterHeader>( clusters.address, index * sizeof( Cluster ) );
    cluster.mCount = 0;
    cluster.mPadding[0] = 0;

    // Test the complete active light set against this cluster. Directional lights affect every
    // cluster; local lights use conservative sphere/cone bounds.
    uint lightCount = parameters.mActiveLightCount;

    // Each cluster tests the complete active light list.
    for ( uint i = 0; i < lightCount; ++i ) {
        bool visible = false;

        switch ( lights[i].mActiveLightType ) {
            case LIGHT_TYPE_POINT:
                visible = TestSphereAABBPointLight( lights, i, cluster, camera.mViewMatrix );
                break;

            case LIGHT_TYPE_AREA: {
                LightInfo light = lights[i];
                RasterAreaShape shape = (RasterAreaShape)uint( light.mPosition.w );
                float extent = shape == RasterAreaShape::eRectangle ? length( float2( light.mAreaAxisX.w, light.mAreaAxisY.w ) ) :
                    ( shape == RasterAreaShape::eTube ? light.mAreaAxisX.w : 0.0 ) + light.mPadding;
                float3 center = mul( camera.mViewMatrix, float4( light.mPosition.xyz, 1.0 ) ).xyz;
                visible = SphereAABBIntersection( center, light.mRadius + extent, cluster.mMinPoint.xyz, cluster.mMaxPoint.xyz );
                break;
            }

            case LIGHT_TYPE_SPOT:
                visible = TestSpotAABB( lights[i], cluster, camera.mViewMatrix );
                break;

            case LIGHT_TYPE_DIRECTIONAL:
                // All clusters are affected by directional light
                // Reconsider for clusters where there is no geometry
                visible = true;
                break;

            default:
                break;
        }

        // Append only within the fixed cluster capacity. The overflow marker exposes a
        // truncated list to diagnostics instead of writing beyond the record.
        if ( visible && lights[i].mIntensity > 0 ) {
            if ( cluster.mCount < MAX_LIGHT_CLUSTERS ) {
                StoreBufferValue<uint>(
                        clusters.address,
                        index * sizeof( Cluster ) + sizeof( ClusterHeader ) + cluster.mCount * sizeof( uint ), i );
                cluster.mCount++;
            } else {
                cluster.mPadding[0] = 1;
            }
        }
    }
    StoreBufferValue<ClusterHeader>( clusters.address, index * sizeof( Cluster ), cluster );
}
