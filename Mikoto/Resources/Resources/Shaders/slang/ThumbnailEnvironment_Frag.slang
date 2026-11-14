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

#include "base/Thumbnail_Helpers.slang"

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    PreviewScene scene = ( ( BufferReference<PreviewScene> )preview.mScene )[0];

    // Undo the top-left texture mapping before reconstructing the camera ray.
    float2 clip = uv * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 );
    float4 farPoint = mul( scene.mInverseViewProjection, float4( clip, 1.0, 1.0 ) );
    float3 direction = normalize( farPoint.xyz / farPoint.w - scene.mCamera.xyz );

    float3 color = previewCubes[preview.mPrefilter].SampleLevel( previewSamplers[preview.mClampSampler], direction,
        clamp(scene.mSettings.w, 0.0, float(preview.mPrefilterMipCount - 1)) ).rgb * scene.mSettings.z;
    float3 world, ray;
    bool hit = GridPlaneIntersection( scene.mInverseViewProjection, clip, world, ray );
    float2 footprint = max( fwidth( world.xz ), float2( 1e-6 ) );
    if ( scene.mPreviewOptions.x > 0 && hit ) {
        float4 grid = PreviewGrid( world, footprint, scene.mCamera.xyz );
        color = grid.rgb + color * ( 1 - grid.a );
    }
    return float4( color, 1 );
}
