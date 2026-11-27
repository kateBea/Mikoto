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

// Analytically filter the procedural checkerboard over the pixel footprint. Integrating cell
// coverage avoids distant checker frequencies folding into visible moire bands.

#pragma once

// Integrate a periodic alternating checker axis rather than selecting a single cell.
// Differences of this integral yield average coverage over a finite pixel interval.
float CheckerboardIntegral( float coordinate ) {
    return 1.0 - abs( 1.0 - 2.0 * frac( coordinate * 0.5 ) );
}

// Average one checker axis over its derivative-sized footprint. Very small footprints fall back
// to the local cell evaluation.
float CheckerboardAxis( float coordinate, float footprint ) {
    // Flat faces have no footprint along their normal; averaging that axis
    // across a cell boundary would incorrectly turn the entire face grey.
    if ( footprint < 1e-4 ) {
        return frac( coordinate * 0.5 ) < 0.5 ? 1.0 : -1.0;
    }

    float average = ( CheckerboardIntegral( coordinate + footprint * 0.5 ) -
                      CheckerboardIntegral( coordinate - footprint * 0.5 ) ) / footprint;
    if ( footprint <= 0.125 ) {
        return average;
    }
    if ( footprint >= 1.0 ) {
        return 0.0;
    }

    // A square wave has arbitrarily high harmonics. A box integral alone
    // retains enough of them to beat into bands even before the fundamental
    // crosses Nyquist. Taper its Fourier harmonics with a Fejer window and drop
    // every harmonic above the pixel sampling limit. Four odd terms cover the
    // transition; nearby squares keep the exact area integral above.
    float4 harmonics = float4( 1.0, 3.0, 5.0, 7.0 );
    float phase = 6.28318530718 * frac( coordinate * 0.5 );
    float4 weights = max( 1.0 - footprint * harmonics, 0.0 );
    float filtered = 1.27323954474 * dot( sin( phase * harmonics ), weights / harmonics );
    return lerp( average, filtered, smoothstep( 0.125, 0.25, footprint ) );
}

// Combine filtered checker axes into the two-color pattern. As cells become subpixel, the
// result converges toward their average instead of generating distant bands.
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
    // A checker multiplies two axes. On a diagonal view their frequencies add
    // along a screen pixel, so filtering each axis alone can still produce beats.
    // Use their combined footprint for the final contrast roll-off.
    float3 projectedFootprint = float3( footprint.y + footprint.z,
        footprint.x + footprint.z, footprint.x + footprint.y );
    float3 contrast = 1.0 - smoothstep( float3( 0.35 ), float3( 1.0 ), projectedFootprint );
    float parity = dot( weights, float3( axes.y * axes.z, axes.x * axes.z, axes.x * axes.y ) * contrast );

    return lerp( colorA, colorBAndCellSize.rgb, saturate( 0.5 - 0.5 * parity ) );
}
