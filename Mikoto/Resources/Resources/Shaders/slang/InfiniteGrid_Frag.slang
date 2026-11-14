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

#pragma once

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/InfiniteGrid_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct FSInput {
    float2 TexCoord : TEXCOORD0;

    float4 FragPos : SV_Position;
};

struct FSOutput {
    float4 Color : SV_Target0;
    float FragDepth : SV_Depth;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT( GridDrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    CameraInfo camera = buffers[parameters.Camera.x].Read<CameraInfo>( 0, 0 );

    // Intersect this pixel's view ray with y = 0 instead of drawing a finite
    // ground mesh. The helper supports perspective and orthographic cameras.
    float3 world, ray;
    bool hit = GridPlaneIntersection( camera.mInverseViewProjection, input.TexCoord * 2.0 - 1.0, world, ray );

    // Estimate how much ground one pixel covers to keep lines antialiased as
    // the camera moves. Evaluate derivatives before any neighboring lanes discard.
    float2 footprint = max( fwidth( world.xz ), float2( 1e-6 ) );

    // Use the ground intersection's depth, not the fullscreen triangle's depth,
    // so scene geometry can hide the grid. Reject hits outside the visible ray.
    float4 view = mul( camera.mViewMatrix, float4( world, 1.0 ) );
    float4 clip = mul( camera.mProjection, view );

    if ( !hit || clip.w <= 0.0 ) {
        discard;
    }

    float depth = clip.z / clip.w;

    if ( depth < 0.0 || depth >= 1.0 ) {
        discard;
    }

    // The shared helper combines the grid scales and colored axes using
    // premultiplied alpha, avoiding bright overlaps where their lines meet.
    float4 color = InfiniteGridColor( world.xz, footprint, parameters );

    // Fade grazing rays and distant lines before they crowd together at the
    // horizon or abruptly reach the camera's far clipping plane.
    float horizonFade = smoothstep( 0.005, 0.05, abs( ray.y ) / max( length( ray ), 1e-6 ) );
    float distanceFade = 1.0 - smoothstep( camera.mPlanes.y * 0.25, camera.mPlanes.y * 0.9, -view.z );

    if ( color.a <= 1e-5 ) {
        discard;
    }

    // The render target uses straight-alpha blending. Unpremultiply only after
    // rejecting empty coverage, then apply the fades to opacity alone.
    FSOutput output;
    output.Color = float4( color.rgb / color.a, saturate( color.a * horizonFade * distanceFade ) );

    // Bias away from the camera prevents fighting with coplanar floors.
    // The pipeline tests scene depth but never writes into it.
    output.FragDepth = min( depth + 1e-6, 1.0 );

    return output;
}
