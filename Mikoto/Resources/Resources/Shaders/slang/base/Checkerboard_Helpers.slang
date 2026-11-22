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

#pragma once

float CheckerboardIntegral( float coordinate ) {
    return 1.0 - abs( 1.0 - 2.0 * frac( coordinate * 0.5 ) );
}

float CheckerboardAxis( float coordinate, float footprint ) {
    // Flat faces have no footprint along their normal; averaging that axis
    // across a cell boundary would incorrectly turn the entire face grey.
    if ( footprint < 1e-4 ) {
        return frac( coordinate * 0.5 ) < 0.5 ? 1.0 : -1.0;
    }

    // Integrate the alternating cells over a pixel to suppress distant shimmer.
    return ( CheckerboardIntegral( coordinate + footprint * 0.5 ) -
             CheckerboardIntegral( coordinate - footprint * 0.5 ) ) / footprint;
}

float3 CheckerboardColor( float3 worldPosition, float3 worldFootprint, float3 worldNormal,
    float3 colorA, float4 colorBAndCellSize, float3 offset ) {
    float cellSize = max( colorBAndCellSize.w, 1e-4 );
    float3 coordinate = ( worldPosition - offset ) / cellSize;
    float3 footprint = worldFootprint / cellSize;
    float3 axes = float3( CheckerboardAxis( coordinate.x, footprint.x ),
        CheckerboardAxis( coordinate.y, footprint.y ), CheckerboardAxis( coordinate.z, footprint.z ) );

    // Project onto the surface instead of multiplying all three axes. A step
    // exactly on a cell-height boundary must not change colour with roundoff
    // in its constant Y coordinate. Blend projections on sloped/curved faces.
    float3 weights = abs( worldNormal );
    weights /= max( max( weights.x, max( weights.y, weights.z ) ), 1e-20 );
    weights *= weights;
    weights *= weights;
    weights /= max( weights.x + weights.y + weights.z, 1e-8 );
    float parity = dot( weights, float3( axes.y * axes.z, axes.x * axes.z, axes.x * axes.y ) );

    return lerp( colorA, colorBAndCellSize.rgb, saturate( 0.5 - 0.5 * parity ) );
}
