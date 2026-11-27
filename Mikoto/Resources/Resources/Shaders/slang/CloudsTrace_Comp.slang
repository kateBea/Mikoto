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

// March camera rays through the union of cloud intervals, stopping at opaque geometry. Output
// RGB is integrated cloud radiance; alpha is the weighted cloud distance, while metadata
// carries RGB transmittance and surface distance.

#include "base/CloudsRender_Helpers.slang"
#include "Clouds_Common.slang"

// Temporal samples rotate by the golden-ratio fraction; optical/radiance caps keep finite HDR outputs.
static const uint kCloudJitterPeriod = 1024u;
static const float kCloudJitterRotation = .61803398875;
static const float kCloudMaxOpticalDepth = 80.0;
static const float kCloudMaxRadiance = 60000.0;

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.mScreen.zw ) ) ) {
        return;
    }
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    float2 uv = ( float2( pixel ) + .5 ) / frame.mScreen.zw;
    float3 origin;
    float3 direction;
    CloudCameraRay( camera, uv, origin, direction );
    int2 fullPixel = clamp( int2( uv * frame.mScreen.xy ), int2( 0 ), int2( frame.mScreen.xy ) - 1 );
    float depth = cloudImages[parameters.mDepth].Load( int3( fullPixel, 0 ) ).r;
    float surface = CloudSurfaceDistance( frame, camera, uv, depth );
    // Clip the cloud interval union against the first opaque surface. Intervals behind geometry
    // are excluded before spending density and lighting samples.
    float start = surface;
    float end = 0;
    for ( uint index = 0; index < frame.mControl.x; ++index ) {
        float2 interval;
        if ( CloudInterval( LoadCloud( frame, index ), origin, direction, interval ) ) {
            start = min( start, max( interval.x, 0 ) );
            end = max( end, min( interval.y, surface ) );
        }
    }
    // Track radiance and RGB transmittance separately. The opacity-weighted mean distance
    // provides the cloud-space point used for temporal reprojection.
    float3 radiance = 0;
    float3 transmission = 1;
    float weightedDistance = 0;
    float totalWeight = 0;
    float minimumStep = CloudMinimumVoxelStep( frame );
    uint seed = pixel.x * 1973u + pixel.y * 9277u;
    float staticPhase = CloudRandom( seed );
    float phase = frac( staticPhase + float( frame.mControl.y % kCloudJitterPeriod ) * kCloudJitterRotation );
    // Anchor every integration segment to one ray grid. Empty-space skipping
    // advances whole segments, so toggling acceleration cannot move the samples.
    float nominalSpacing = max( minimumStep, max( end - start, 0 ) / max( frame.mFiltering.x, 1u ) );
    uint segmentIndex = 0;
    uint iterations = 0;
    float nearestSurface = 1e20;
    float distance = start;
    for ( ; iterations < frame.mFiltering.x && distance < end; ++iterations ) {
        float3 current = origin + direction * distance;
        float emptyDistance = 1e20;
        for ( uint index = 0; index < frame.mControl.x; ++index ) {
            float4 field = CloudField( frame, LoadCloud( frame, index ), current );
            emptyDistance = min( emptyDistance, field.r );
            nearestSurface = min( nearestSurface, abs( field.r ) );
        }
        // The skip bound belongs to the current point, never a jittered point
        // ahead of it. This preserves small formations and thin silhouettes.
        if ( emptyDistance > minimumStep * 2 ) {
            uint advance = max( uint( floor( emptyDistance * .8 / nominalSpacing ) ), 1u );
            segmentIndex += advance;
            distance = start + float( segmentIndex ) * nominalSpacing;
            continue;
        }
        float spacing = min( nominalSpacing, end - distance );
        float jitter = distance < frame.mDetail.w ? phase : staticPhase;
        float3 position = origin + direction * ( distance + spacing * jitter );
        float3 extinction = 0;
        float3 scattering = 0;
        for ( uint index = 0; index < frame.mControl.x; ++index ) {
            CloudRecord cloud = LoadCloud( frame, index );
            float4 field = CloudField( frame, cloud, position );
            float density = CloudDensity( frame, cloud, position, field );
            if ( density <= 0 ) {
                continue;
            }
            extinction += cloud.mExtinction.rgb * density;
            scattering += cloud.mScatteringPhase.rgb * density * CloudReceiverLighting( frame, cloud, index, position, direction, field );
        }
        // Integrate one homogeneous segment analytically with Beer-Lambert extinction. Incoming
        // transmittance weights the scattering added by this segment before it is attenuated
        // for the next one.
        float3 segment = exp( -min( extinction * spacing, kCloudMaxOpticalDepth ) );
        float3 integral = ( 1 - segment ) / max( extinction, 1e-7 );
        radiance += transmission * scattering * integral;
        float weight = dot( transmission * ( 1 - segment ), float3( 1.0 / 3.0 ) );
        weightedDistance += ( distance + spacing * .5 ) * weight;
        totalWeight += weight;
        transmission *= segment;
        ++segmentIndex;
        distance = start + float( segmentIndex ) * nominalSpacing;
        if ( all( transmission < frame.mClock.w ) ) {
            transmission = 0;
            break;
        }
    }
    if ( frame.mControl.w == 1 ) {
        radiance = 1 - transmission;
    }
    if ( frame.mControl.w == 2 ) {
        float v = exp( -nearestSurface / max( frame.mMarch.x * 8, 1 ) );
        radiance = float3( v, v * .3, 1 - v );
    }
    if ( frame.mControl.w == 3 ) {
        float v = float( iterations ) / max( frame.mFiltering.x, 1u );
        radiance = float3( v, 1 - abs( v * 2 - 1 ), 1 - v );
    }
    cloudOutputs[parameters.mOutput][pixel] = float4( min( radiance, kCloudMaxRadiance ), totalWeight > 1e-6 ? weightedDistance / totalWeight : -1 );
    cloudOutputs[parameters.mMetadata][pixel] = float4( saturate( transmission ), surface );
}
