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

struct GridDrawParams {
    uint4 Camera;// x = buffer descriptor; explicit padding matches the CPU.

    float4 MinorColor;
    float4 MajorColor;
    float4 XAxisColor;
    float4 ZAxisColor;
    
    float4 Widths;// minor, major, X axis, Z axis, in pixels
};

bool GridPlaneIntersection( float4x4 inverseViewProjection, float2 ndc, out float3 world, out float3 ray ) {
    // An interior depth avoids cancellation/division by zero at a very distant
    // or infinite far plane. Works for both perspective and orthographic views.
    float4 a = mul( inverseViewProjection, float4( ndc, 0.0, 1.0 ) );
    float4 b = mul( inverseViewProjection, float4( ndc, 0.5, 1.0 ) );

    world = 0;
    ray = 0;

    if ( abs( a.w ) < 1e-10 || abs( b.w ) < 1e-10 ) {
        return false;
    }

    float3 origin = a.xyz / a.w;
    ray = b.xyz / b.w - origin;
    if ( abs( ray.y ) <= 1e-6 * length( ray ) ) {
        return false;
    }

    float t = -origin.y / ray.y;
    if ( t < 0.0 ) {
        return false;
    }

    world = origin + t * ray;
    return true;
}

float GridLineCoverage( float coordinate, float footprint, float width ) {
    footprint = max( footprint, 1e-6 );
    float distance = abs( frac( coordinate + 0.5 ) - 0.5 );
    float coverage = 1.0 - smoothstep( max( 0.0, width * 0.5 - 0.5 ), width * 0.5 + 0.5, distance / footprint );

    // Remove frequencies finer than a pixel instead of drawing a solid sheet.
    return coverage * ( 1.0 - smoothstep( 0.25, 0.75, footprint ) );
}

float GridCoverage( float2 position, float2 footprint, float spacing, float width ) {
    return max( GridLineCoverage( position.x / spacing, footprint.x / spacing, width ),
                GridLineCoverage( position.y / spacing, footprint.y / spacing, width ) );
}

float AxisCoverage( float distance, float footprint, float width ) {
    return 1.0 - smoothstep( max( 0.0, width * 0.5 - 0.5 ), width * 0.5 + 0.5, abs( distance ) / max( footprint, 1e-6 ) );
}

// Accumulate premultiplied color, then convert once for straight-alpha blending.
float4 GridOver( float4 behind, float4 color, float coverage ) {
    float alpha = saturate( color.a * coverage );
    return float4( color.rgb * alpha + behind.rgb * ( 1.0 - alpha ), alpha + behind.a * ( 1.0 - alpha ) );
}

float4 InfiniteGridColor( float2 world, float2 footprint, GridDrawParams parameters ) {
    // Three levels cross-fade continuously as a pixel covers more world space.
    float lod = max( 0.0, log10( max( footprint.x, footprint.y ) * 8.0 ) );
    float spacing = pow( 10.0, floor( lod ) );
    float blend = frac( lod );
    float fine = GridCoverage( world, footprint, spacing, parameters.Widths.x );
    float medium = GridCoverage( world, footprint, spacing * 10.0, lerp( parameters.Widths.y, parameters.Widths.x, blend ) );

    float coarse = GridCoverage( world, footprint, spacing * 100.0, parameters.Widths.y );
    float4 color = GridOver( float4( 0 ), parameters.MinorColor, fine * ( 1.0 - blend ) );

    color = GridOver( color, lerp( parameters.MajorColor, parameters.MinorColor, blend ), medium );
    color = GridOver( color, parameters.MajorColor, coarse * blend );

    // X runs along z=0, Z along x=0. Draw each once after the gray lattice.
    color = GridOver( color, parameters.XAxisColor, AxisCoverage( world.y, footprint.y, parameters.Widths.z ) );

    return GridOver( color, parameters.ZAxisColor, AxisCoverage( world.x, footprint.x, parameters.Widths.w ) );
}
