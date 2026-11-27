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

// Analytic editor-grid intersections and pixel-filtered line coverage. The grid and axes are
// evaluated from a camera ray, then composited using scene-depth visibility.

#pragma once

struct GridDrawParams {
    uint4 mCamera;// x = buffer descriptor; explicit padding matches the CPU.

    float4 mMinorColor;
    float4 mMajorColor;
    float4 mXAxisColor;
    float4 mZAxisColor;

    float4 mWidths;// minor, major, X axis, Z axis, in pixels
    float4 mYAxisColor;
    float4 mGrid;// cell size, opacity, vertical-axis draw, reserved
};

// Reconstruct in camera space before rotating into the world. Two unprojected
// world points near the eye lose their small difference at kilometre coordinates.
// Build the unnormalized view ray directly from inverse projection. Ray/plane intersection
// remains stable without reconstructing a distant finite point.
bool GridViewRay( float4x4 inverseProjection, float3x3 viewRotation,
    float3 cameraPosition, bool perspective, float2 ndc, out float3 origin, out float3 ray ) {
    float4 a = mul( inverseProjection, float4( ndc, .5, 1 ) );
    origin = cameraPosition;
    ray = 0;
    if ( abs( a.w ) < 1e-10 ) {
        return false;
    }
    float3 local = a.xyz / a.w;
    if ( perspective ) {
        ray = mul( transpose( viewRotation ), a.xyz );
    }
    else {
        origin += mul( transpose( viewRotation ), float3( local.xy, 0 ) );
        ray = mul( transpose( viewRotation ), float3( 0, 0, -1 ) );
    }
    return dot( ray, ray ) > 1e-12;
}

bool GridPlaneIntersection( float3 origin, float3 ray, out float3 world ) {
    world = 0;
    if ( abs( ray.y ) <= 1e-6 * length( ray ) ) {
        return false;
    }
    float distance = -origin.y / ray.y;
    if ( distance < 0 ) {
        return false;
    }
    world = float3( origin.x + distance * ray.x, 0, origin.z + distance * ray.z );
    return true;
}

bool GridPlaneIntersection( float4x4 inverseProjection, float3x3 viewRotation,
    float3 cameraPosition, bool perspective, float2 ndc, out float3 world, out float3 ray ) {
    float3 origin;
    bool valid = GridViewRay( inverseProjection, viewRotation, cameraPosition, perspective, ndc, origin, ray );
    return GridPlaneIntersection( origin, ray, world ) && valid;
}

// Find the closest point between this view ray and the world Y axis. The signed
// separation has stable derivatives across the line, unlike length() at zero.
bool GridYAxisIntersection( float3 origin, float3 ray, out float3 world, out float separation ) {
    world = 0;
    separation = 0;
    float denominator = dot( ray.xz, ray.xz );
    if ( denominator <= 1e-12 * dot( ray, ray ) ) {
        return false;
    }
    float distance = -dot( origin.xz, ray.xz ) / denominator;
    world = float3( 0, origin.y + distance * ray.y, 0 );
    separation = ( origin.x * ray.z - origin.z * ray.x ) * rsqrt( denominator );
    return distance > 0;
}

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

// Integrate the line width over the local pixel footprint. This is the antialiasing step that
// turns subpixel grid lines into partial coverage.
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

// Combine minor/major grid lines and the enabled axes at their analytic intersections.
// Distance/angle fading limits unstable detail near the horizon.
float4 InfiniteGridColor( float2 world, float2 footprint, GridDrawParams parameters ) {
    // Three levels cross-fade continuously as a pixel covers more world space.
    float cellSize = max( parameters.mGrid.x, 0.01 );
    float lod = max( 0.0, log10( max( footprint.x, footprint.y ) * 8.0 / cellSize ) );
    float spacing = cellSize * pow( 10.0, floor( lod ) );
    float blend = frac( lod );
    float fine = GridCoverage( world, footprint, spacing, parameters.mWidths.x );
    float medium = GridCoverage( world, footprint, spacing * 10.0, lerp( parameters.mWidths.y, parameters.mWidths.x, blend ) );

    float coarse = GridCoverage( world, footprint, spacing * 100.0, parameters.mWidths.y );
    float4 color = GridOver( float4( 0 ), parameters.mMinorColor, fine * ( 1.0 - blend ) );

    color = GridOver( color, lerp( parameters.mMajorColor, parameters.mMinorColor, blend ), medium );
    color = GridOver( color, parameters.mMajorColor, coarse * blend );

    // X runs along z=0, Z along x=0. Draw each once after the gray lattice.
    color = GridOver( color, parameters.mXAxisColor, AxisCoverage( world.y, footprint.y, parameters.mWidths.z ) );

    return GridOver( color, parameters.mZAxisColor, AxisCoverage( world.x, footprint.x, parameters.mWidths.w ) );
}
