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

// Turn ribbon coordinates into an HDR lightning core and surrounding glow. Segment intensity
// and color come from the uploaded lightning records.

#include "CloudsLightning_Common.slang"

[shader( "fragment" )]
float4 main( LightningVertex input ) : SV_Target0 {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    int2 pixel = clamp( int2( input.mPosition.xy ), int2( 0 ), int2( frame.mScreen.xy ) - 1 );
    float depth = lightningDepth[parameters.mDepth].Load( int3( pixel, 0 ) ).r;
    float2 uv = ( float2( pixel ) + .5 ) / frame.mScreen.xy;
    float4 scene = mul( frame.mInverseViewProjection, float4( uv.x * 2 - 1, 1 - uv.y * 2, depth, 1 ) );
    bool orthographic = camera.mProjection[3][3] != 0;
    float3 forward = -camera.mViewMatrix[2].xyz;
    float3 direction = orthographic ? normalize( forward ) : normalize( input.mWorld - camera.mViewPosition.xyz );
    float distance = orthographic ? dot( input.mWorld - camera.mViewPosition.xyz, direction ) : length( input.mWorld - camera.mViewPosition.xyz );
    float3 origin = input.mWorld - direction * distance;
    if ( abs( scene.w ) > 1e-8 && depth > 0 && depth < 1 ) {
        float sceneDistance = dot( scene.xyz / scene.w - origin, direction );
        if ( distance > sceneDistance + .02 ) {
            discard;
        }
    }
    float3 opticalDepth = CloudLocalOpticalDepth( frame, origin, direction, distance );
    float core = exp( -input.mUv.y * input.mUv.y * 80 );
    float glow = exp( -input.mUv.y * input.mUv.y * 5 ) * .12;
    float3 color = lerp( input.mColorIntensity.rgb, float3( 1 ), .65 * core );
    return float4( min( color * input.mColorIntensity.w * ( core + glow ) * exp( -opticalDepth ), 60000.0 ), 0 );
}
