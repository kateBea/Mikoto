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

// Accumulate the cloud trace using camera reprojection and cloud wind motion. Radiance and
// transmittance histories share distance rejection and neighborhood clipping.

#include "Clouds_Common.slang"

// Reproject cloud-space motion as well as camera motion. Reject changed
// occluders/cuts and clip history to the current neighborhood to limit trails.
// A 3x3 current-frame neighborhood bounds reprojected cloud history.
static const int kCloudHistoryClampRadius = 1;

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.mScreen.zw ) ) ) {
        return;
    }
    float4 current = cloudImages[parameters.mInput].Load( int3( pixel, 0 ) );
    float4 transmission = cloudImages[parameters.mInputMetadata].Load( int3( pixel, 0 ) );
    float3 result = current.rgb;
    float3 resultTransmission = transmission.rgb;
    if ( frame.mControl.z != 0 && frame.mControl.w == 0 && current.a > 0 ) {
        float2 uv = ( float2( pixel ) + .5 ) / frame.mScreen.zw;
        float3 origin;
        float3 direction;
        CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
        CloudCameraRay( camera, uv, origin, direction );
        float3 position = origin + direction * current.a;
        float3 velocity = 0;
        for ( uint index = 0; index < frame.mControl.x; ++index ) {
            CloudRecord cloud = LoadCloud( frame, index );
            float3 local = mul( cloud.mWorldToLocal, float4( position, 1 ) ).xyz;
            if ( all( abs( local ) <= .5 ) ) {
                velocity = cloud.mWind.xyz;
                break;
            }
        }
        // Undo cloud advection before projecting into the previous camera. Camera reprojection
        // alone would incorrectly treat a moving formation as static geometry.
        float3 previousPosition = position - velocity * frame.mClock.y;
        float4 clip = mul( frame.mPreviousViewProjection, float4( previousPosition, 1 ) );
        float2 oldUV = float2( clip.x / clip.w * .5 + .5, .5 - clip.y / clip.w * .5 );
        if ( clip.w > 0 && all( oldUV >= 0 ) && all( oldUV <= 1 ) ) {
            float4 history = cloudImages[parameters.mHistory].SampleLevel( cloudSamplers[frame.mResources.y], oldUV, 0 );
            float4 historyTransmission = cloudImages[parameters.mHistoryMetadata].SampleLevel( cloudSamplers[frame.mResources.y], oldUV, 0 );
            float expectedDistance = length( previousPosition - frame.mPreviousCamera.xyz );
            float tolerance = max( frame.mMarch.x * 8, current.a * .04 );
            // Reject history when the cloud distance or foreground-occluder distance changed.
            // Otherwise an old cloud/background layer can leak across a newly exposed edge.
            bool sameCloud = history.a > 0 && abs( history.a - expectedDistance ) < tolerance;
            bool sameOccluder = abs( historyTransmission.a - transmission.a ) < max( 2, transmission.a * .02 );
            if ( sameCloud && sameOccluder ) {
                // Bound the accepted history by current neighboring radiance and transmission.
                // Clipping suppresses stale bright or dark trails while retaining useful
                // temporal samples.
                float3 minimum = current.rgb;
                float3 maximum = current.rgb;
                float3 lowT = transmission.rgb;
                float3 highT = transmission.rgb;
                for ( int y = -kCloudHistoryClampRadius; y <= kCloudHistoryClampRadius; ++y ) {
                    for ( int x = -kCloudHistoryClampRadius; x <= kCloudHistoryClampRadius; ++x ) {
                        int2 neighbor = clamp( int2( pixel ) + int2( x, y ), int2( 0 ), int2( frame.mScreen.zw ) - 1 );
                        float3 sample = cloudImages[parameters.mInput].Load( int3( neighbor, 0 ) ).rgb;
                        float3 sampleT = cloudImages[parameters.mInputMetadata].Load( int3( neighbor, 0 ) ).rgb;
                        minimum = min( minimum, sample );
                        maximum = max( maximum, sample );
                        lowT = min( lowT, sampleT );
                        highT = max( highT, sampleT );
                    }
                }
                result = lerp( current.rgb, clamp( history.rgb, minimum, maximum ), frame.mMarch.z );
                resultTransmission = lerp( transmission.rgb, clamp( historyTransmission.rgb, lowT, highT ), frame.mMarch.z );
            }
        }
    }
    // History at a moving edge must not reintroduce the HDR sun behind a ray
    // that the current frame has already resolved as fully opaque.
    if ( all( transmission.rgb == 0 ) ) {
        resultTransmission = 0;
    }
    cloudOutputs[parameters.mOutput][pixel] = float4( result, current.a );
    cloudOutputs[parameters.mMetadata][pixel] = float4( resultTransmission, transmission.a );
}
