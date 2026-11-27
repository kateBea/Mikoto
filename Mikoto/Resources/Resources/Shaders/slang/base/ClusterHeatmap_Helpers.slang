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

// Reconstruct the visible cluster and map its light count to a debug color. Composition
// supports a standalone heatmap and an overlay on existing scene radiance.

#pragma once

#include "Camera_Helpers.slang"
#include "ClusteringLights_Helpers.slang"

float3 ClusterHeatmapViewPosition( float2 uv, float depth, float4x4 inverseProjection ) {
    // Undo the depth projection, not a perspective-only linearization formula.
    // NDC Y is up while render pixels/UVs start at the top-left. This matches
    // AABBGen and also reconstructs orthographic camera depth correctly.
    float2 ndc = uv * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 );
    float4 view = mul( inverseProjection, float4( ndc, depth, 1.0 ) );
    return view.xyz / view.w;
}

float3 ClusterHeatmapColor( uint count, bool overflow, float2 pixel ) {
    if ( overflow ) {
        // mCount is capped at the list capacity. Overflow means forward shading
        // scans the entire active list, so distinguish it from a merely full list.
        bool stripe = ( ( ( uint( pixel.x ) + uint( pixel.y ) ) / 8u ) & 1u ) != 0;
        return stripe ? float3( 1.0, 0.0, 1.0 ) : float3( 0.4, 0.0, 0.4 );
    }

    // Log spacing exposes differences in lightly populated clusters while still
    // covering the full 256-entry list. Approximately: blue 0, green 16, red 256.
    float heat = saturate( log2( 1.0 + min( count, MAX_LIGHT_CLUSTERS ) ) / log2( 1.0 + MAX_LIGHT_CLUSTERS ) );
    float segment = heat * 4.0;
    if ( segment < 1.0 ) {
        return lerp( float3( 0.02, 0.12, 0.8 ), float3( 0.0, 0.75, 1.0 ), segment );
    }
    if ( segment < 2.0 ) {
        return lerp( float3( 0.0, 0.75, 1.0 ), float3( 0.05, 0.9, 0.2 ), segment - 1.0 );
    }
    if ( segment < 3.0 ) {
        return lerp( float3( 0.05, 0.9, 0.2 ), float3( 1.0, 0.85, 0.02 ), segment - 2.0 );
    }
    return lerp( float3( 1.0, 0.85, 0.02 ), float3( 1.0, 0.05, 0.02 ), segment - 3.0 );
}

float3 ComposeClusterHeatmap( float3 sceneColor, float3 heatColor, bool overlay, float opacity ) {
    // Both colors are already display colors. Do not tone-map the heat palette.
    return overlay ? lerp( sceneColor, heatColor, saturate( opacity ) ) : heatColor;
}
