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

#include "Fog_Common.slang"

// Apply accumulated in-scattering + transmittance at the actual surface depth.
// https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf

[shader( "fragment" )] float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    float4 color = textures[parameters.Source].SampleLevel( samplers[parameters.Sampler], uv, 0.0 );
    if ( parameters.Stage == 3 ) {
        return color;
    }
    FogFrameData frame = LoadBufferValue<FogFrameData>( parameters.Frame, 0 );
    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.Camera, 0 );
    uint width, height;
    textures[parameters.Depth].GetDimensions( width, height );
    int2 pixel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float depth = textures[parameters.Depth].Load( int3( pixel, 0 ) ).r;
    float3 origin, direction;
    FogCameraRay( camera.mInverseViewProjection, camera.mViewPosition.xyz, camera.mProjection[3][3] != 0.0, uv, origin, direction );
    float distance = frame.Range.y;
    if ( depth < 1.0 ) {
        float4 world = mul( camera.mInverseViewProjection, float4( uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0 ) );
        distance = min( length( world.xyz / world.w - origin ), distance );
    }
    float3 radiance, transmittance;
    ReadIntegratedFog( textures[parameters.Radiance], textures[parameters.Transmittance], samplers[parameters.Sampler],
                       uv, distance, frame, radiance, transmittance );
    return float4( color.rgb * transmittance + radiance, color.a );
}
