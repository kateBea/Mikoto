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

// Cache local-light illumination in cloud space, including extinction and geometry visibility.
// This separates expensive light transport from per-pixel camera-ray integration.

#include "base/CloudLighting_Helpers.slang"
#include "base/TerrainSampling_Helpers.slang"
#include "Clouds_Common.slang"

[shader( "compute" )]
[numthreads( kVolumeGroupSide, kVolumeGroupSide, kVolumeGroupSide )]
void main( uint3 thread : SV_DispatchThreadID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    uint3 size = frame.mResolution.xyz / 4;
    if ( any( thread >= size ) ) {
        return;
    }
    CloudRecord cloud = LoadCloud( frame, parameters.mVolume );
    float3 local = ( float3( thread ) + .5 ) / float3( size ) - .5;
    float3 position = mul( cloud.mLocalToWorld, float4( local, 1 ) ).xyz;
    float3 energy = 0;
    if ( CloudDensity( frame, cloud, position, CloudField( frame, cloud, position ) ) > 0 ) {
        float3 view = normalize( position - frame.mCamera.xyz );
        for ( uint i = 0; i < frame.mLightingControl.x; ++i ) {
            CloudLocalLight light = LoadBufferValue<CloudLocalLight>( frame.mLocalLights, i * sizeof( CloudLocalLight ) );
            float3 delta = light.mPositionRange.xyz - position;
            float distance = length( delta );
            if ( distance >= light.mPositionRange.w || distance < 1e-5 ) {
                continue;
            }
            float3 direction = delta / distance;
            float window = saturate( 1 - pow( distance / light.mPositionRange.w, 4.0 ) );
            float attenuation = window * window / max( distance * distance, max( light.mParameters.y * light.mParameters.y, .01 ) );
            if ( light.mControl.x == 2 ) {
                attenuation *= smoothstep( light.mDirectionOuter.w, max( light.mParameters.x, light.mDirectionOuter.w + 1e-5 ), dot( -direction, light.mDirectionOuter.xyz ) );
            }
            if ( attenuation <= 0 ) {
                continue;
            }
            float3 transmission = 1;
            if ( ( cloud.mFlags.w & 2u ) != 0 ) {
                float visible = light.mControl.y != 0 ? CloudMeshVisibility( frame, light.mControl.z, position ) : 1;
                if ( light.mControl.y != 0 ) {
                    visible *= TerrainSunVisibility( frame.mTerrainVertices, frame.mTerrainRecords, frame.mTerrainBounds, position, direction, distance );
                }
                if ( visible <= 0 ) {
                    continue;
                }
                transmission = visible * exp( -CloudLocalOpticalDepth( frame, position, direction, distance ) );
            }
            energy += light.mColorIntensity.rgb * light.mColorIntensity.w * attenuation * transmission *
                      CloudPhase( dot( view, direction ), cloud.mScatteringPhase.w );
        }
    }
    cloudVolumeOutputs[parameters.mOutput][thread] = float4( min( energy, 60000.0 ), 0 );
}
