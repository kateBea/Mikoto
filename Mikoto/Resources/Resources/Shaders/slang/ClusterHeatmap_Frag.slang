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

// Visualize how many lights affect the cluster under this pixel. The selected mode either
// replaces scene color or blends the heatmap over it.

#pragma once

#include "base/Base.slang"
#include "base/ClusterHeatmap_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct HeatmapParams {
    BufferReference<Cluster> mClusters;
    BufferReference<CameraInfo> mCamera;
    float4 mGridSize;
    uint mSceneTexture;
    uint mDepthTexture;
    uint mSampler;
    uint mClusterCount;
    uint mOverlay;
    float mOpacity;
};

MKT_DECLARE_PUSH_CONSTANT( HeatmapParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_SAMPLERS samplers;

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    bool overlay = parameters.mOverlay != 0;
    float3 sceneColor = float3( 0.0 );
    if ( overlay ) {
        sceneColor = textures[parameters.mSceneTexture].SampleLevel( samplers[parameters.mSampler], uv, 0 ).rgb;
    }

    uint width;
    uint height;
    textures[parameters.mDepthTexture].GetDimensions( width, height );
    int2 pixel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float depth = textures[parameters.mDepthTexture].Load( int3( pixel, 0 ) ).r;
    float3 background = overlay ? sceneColor : float3( 0.015 );
    if ( depth >= 1.0 || parameters.mClusterCount == 0 || any( parameters.mGridSize.xyz < 1.0 ) ) {
        // No opaque surface: there is no surface cluster to visualize. Sky
        // stays unchanged in overlay mode and dark in the standalone heatmap.
        return float4( background, 1.0 );
    }

    CameraInfo camera = parameters.mCamera[0];
    float2 pixelUv = ( float2( pixel ) + 0.5 ) / float2( width, height );
    float3 viewPosition = ClusterHeatmapViewPosition( pixelUv, depth, camera.mInverseProjection );
    if ( viewPosition.z >= 0.0 ) {
        return float4( background, 1.0 );
    }

    uint index =
            ComputeTileIndex( viewPosition, parameters.mGridSize.xyz, camera.mPlanes.xy,
                              float4( pixelUv * camera.mScreenDimensions.xy, 0.0, 1.0 ), camera.mScreenDimensions.xy );
    if ( index >= parameters.mClusterCount ) {
        return float4( background, 1.0 );
    }

    // Read only the header; do not copy the cluster's large light-index array.
    ClusterHeader cluster = LoadClusterHeader( parameters.mClusters.address, index );
    uint count = cluster.mCount;
    bool overflow = cluster.mPadding[0] != 0;
    float3 heat = ClusterHeatmapColor( count, overflow, float2( pixel ) );
    return float4( ComposeClusterHeatmap( sceneColor, heat, overlay, parameters.mOpacity ), 1.0 );
}
