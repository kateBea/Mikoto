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

// Generate camera rays with pixel and lens sampling. Depth of field offsets the ray origin on
// the aperture and aims it at the shared focal plane.

#pragma once

// Construct camera rays and sample the thin-lens aperture.

// PBRT's concentric mapping preserves uniform aperture area.
// https://pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions
// Map the 2D sample into the camera aperture domain. Lens sampling changes the ray origin while
// the focus point remains shared.
float2 SampleLensDisk( float2 sample ) {
    float2 offset = 2.0 * sample - 1.0;
    if ( all( offset == 0.0 ) ) {
        return float2( 0.0 );
    }

    float radius;
    float angle;
    if ( abs( offset.x ) > abs( offset.y ) ) {
        radius = offset.x;
        angle = ( PI * 0.25 ) * ( offset.y / offset.x );
    }
    else {
        radius = offset.y;
        angle = ( PI * 0.5 ) - ( PI * 0.25 ) * ( offset.x / offset.y );
    }
    return radius * float2( cos( angle ), sin( angle ) );
}

// Offset the pinhole ray to a sampled lens point and aim it at the focal plane. A zero aperture
// leaves the original camera ray unchanged.
void ApplyDepthOfField( float4 right, float4 up, float3 forward, float2 sample,
    inout float3 origin, inout float3 direction ) {
    // Intersect the pinhole ray with a plane perpendicular to camera forward.
    // https://pbr-book.org/4ed/Cameras_and_Film/Projective_Camera_Models
    float3 focus = origin + direction * ( up.w / max( dot( direction, forward ), 1e-6 ) );
    float2 lens = right.w * SampleLensDisk( sample );
    origin += right.xyz * lens.x + up.xyz * lens.y;
    direction = normalize( focus - origin );
}

// Convert a pixel sample into a world ray using the uploaded camera matrices/basis. Pixel
// jitter controls subpixel coverage independently from lens sampling.
void GenerateCameraRay( uint2 pixel, float2 jitter, out float3 origin, out float3 direction ) {
    float2 ndc = ( float2( pixel ) + jitter ) / float2( DispatchRaysDimensions().xy ) * 2.0 - 1.0;
    float4 nearPoint = mul( Parameters.mInverseViewProjection, float4( ndc.x, -ndc.y, 0.0, 1.0 ) );
    float4 farPoint = mul( Parameters.mInverseViewProjection, float4( ndc.x, -ndc.y, 0.5, 1.0 ) );
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    // An orthographic ray starts on the near plane at this pixel.
    origin = Parameters.mCamera.w != 0.0 ? nearPoint.xyz : Parameters.mCamera.xyz;
    direction = normalize( farPoint.xyz - origin );
}
