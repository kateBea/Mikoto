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

// =========================================================================
// Original implementation references:
// https://github.com/skaarj1989/SupernovaEngine
// http://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/
// =========================================================================

// Intersect camera rays with the editor grid and axes, then test against scene depth. Pixel
// footprints filter line coverage to keep the distant grid stable near the horizon.

#pragma once

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/InfiniteGrid_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct FSInput {
    float2 mTexCoord : TEXCOORD0;

    float4 mFragPos : SV_Position;
};

struct FSOutput {
    float4 mColor : SV_Target0;
    float mFragDepth : SV_Depth;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT( GridDrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    CameraInfo camera = buffers[parameters.mCamera.x].Read<CameraInfo>( 0, 0 );

    // Both draws reconstruct the ray in camera space to avoid distant-world
    // cancellation. The plane and vertical axis keep independent depth tests.
    float3 origin;
    float3 ray;
    float3 world;
    bool valid = GridViewRay( camera.mInverseProjection, float3x3( camera.mViewMatrix ),
        camera.mViewPosition.xyz, camera.mProjection[3][3] == 0, input.mTexCoord * 2.0 - 1.0, origin, ray );
    bool verticalAxis = parameters.mGrid.z > 0.5;
    bool hit;
    float4 color;
    float horizonFade = 1;
    if ( verticalAxis ) {
        float separation;
        hit = GridYAxisIntersection( origin, ray, world, separation );
        float coverage = AxisCoverage( separation, fwidth( separation ), parameters.mWidths.z );
        color = GridOver( float4( 0 ), parameters.mYAxisColor, coverage );
    }
    else {
        hit = GridPlaneIntersection( origin, ray, world );
        // Evaluate derivatives before any neighboring lanes discard.
        float2 footprint = max( fwidth( world.xz ), float2( 1e-6 ) );
        color = InfiniteGridColor( world.xz, footprint, parameters );
        horizonFade = smoothstep( 0.005, 0.05, abs( ray.y ) / max( length( ray ), 1e-6 ) );
    }

    // Use the world intersection's depth, not the fullscreen triangle's depth,
    // so scene geometry can hide the grid. Reject hits outside the visible ray.
    float4 view = mul( camera.mViewMatrix, float4( world, 1.0 ) );
    float4 clip = mul( camera.mProjection, view );

    if ( !valid || !hit || clip.w <= 0.0 ) {
        discard;
    }

    float depth = clip.z / clip.w;

    if ( depth < 0.0 || depth >= 1.0 ) {
        discard;
    }

    // Fade distant lines before the far plane. Only the XZ lattice fades at
    // the horizon; the vertical Y axis remains visible above it.
    float distanceFade = 1.0 - smoothstep( camera.mPlanes.y * 0.25, camera.mPlanes.y * 0.9, -view.z );

    if ( color.a <= 1e-5 ) {
        discard;
    }

    // The render target uses straight-alpha blending. Unpremultiply only after
    // rejecting empty coverage, then apply the fades to opacity alone.
    FSOutput output;
    output.mColor = float4( color.rgb / color.a, saturate( color.a * horizonFade * distanceFade * parameters.mGrid.y ) );

    // Bias away from the camera prevents fighting with coplanar floors.
    // The pipeline tests scene depth but never writes into it.
    // A fixed 1e-6 depth offset spans many metres near a distant far plane.
    // One representable depth step keeps the overlay bias independent of range.
    output.mFragDepth = asfloat( min( asuint( depth ) + 1u, asuint( 1.0 ) ) );

    return output;
}
