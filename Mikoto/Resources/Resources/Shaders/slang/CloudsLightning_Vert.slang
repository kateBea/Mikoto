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

// Expand a lightning segment into a camera-facing ribbon. Interpolants preserve position across
// the ribbon for the emissive core and glow profile.

#include "CloudsLightning_Common.slang"

[shader( "vertex" )]
LightningVertex main( uint vertex : SV_VertexID, uint instance : SV_InstanceID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
    CloudBoltSegment segment = LoadBufferValue<CloudBoltSegment>( frame.mBolts, instance * sizeof( CloudBoltSegment ) );
    static const float2 corners[kVerticesPerQuad] = { float2( 0, -1 ), float2( 1, -1 ), float2( 1, 1 ), float2( 0, -1 ), float2( 1, 1 ), float2( 0, 1 ) };
    float2 corner = corners[vertex];
    float3 tangent = normalize( segment.mEndIntensity.xyz - segment.mStartWidth.xyz );
    float3 midpoint = ( segment.mStartWidth.xyz + segment.mEndIntensity.xyz ) * .5;
    float3 view = camera.mProjection[3][3] != 0 ? float3( camera.mViewMatrix[2].xyz ) : camera.mViewPosition.xyz - midpoint;
    float3 right = cross( tangent, view );
    if ( dot( right, right ) < 1e-10 ) {
        right = cross( tangent, abs( tangent.y ) > .99 ? float3( 1, 0, 0 ) : float3( 0, 1, 0 ) );
    }
    right = normalize( right );
    LightningVertex output;
    output.mWorld = lerp( segment.mStartWidth.xyz, segment.mEndIntensity.xyz, corner.x ) + right * corner.y * segment.mStartWidth.w * 3;
    output.mPosition = mul( camera.mProjection, mul( camera.mViewMatrix, float4( output.mWorld, 1 ) ) );
    output.mUv = float2( corner.x, corner.y );
    output.mColorIntensity = float4( segment.mColor.rgb, segment.mEndIntensity.w );
    return output;
}
