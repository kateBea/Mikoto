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

// Cache sun optical depth through each cloud volume. The render march reuses this low-frequency
// lighting field and can add more detailed local samples.

#include "base/CloudLighting_Helpers.slang"
#include "base/TerrainSampling_Helpers.slang"
#include "Clouds_Common.slang"

// Separate sun/sky ray cache, as used by Cumulus and Frostnova. Mikoto retains
// RGB sun optical depth to support chromatic media and surface cloud shadows.
[shader( "compute" )]
[numthreads( kVolumeGroupSide, kVolumeGroupSide, kVolumeGroupSide )]
void main( uint3 voxel : SV_DispatchThreadID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    uint3 size = frame.mResolution.xyz / 2;
    if ( any( voxel >= size ) ) {
        return;
    }
    if ( frame.mFiltering.y == 0 && ( voxel.z & 3u ) != ( frame.mControl.y & 3u ) ) {
        return;
    }
    CloudRecord cloud = LoadCloud( frame, parameters.mVolume );
    float3 position = mul( cloud.mLocalToWorld, float4( ( float3( voxel ) + .5 ) / size - .5, 1 ) ).xyz;
    float3 sunDepth = CloudLocalOpticalDepth( frame, position, frame.mSunDirection.xyz, frame.mMarch.y );
    // Upward ambient includes every cloud, even one that does not cast surface shadows.
    float skyDepth = 0;
    for ( uint index = 0; index < frame.mControl.x; ++index ) {
        CloudRecord occluder = LoadCloud( frame, index );
        float2 interval;
        if ( !CloudInterval( occluder, position, float3( 0, 1, 0 ), interval ) ) {
            continue;
        }
        float begin = max( interval.x, 0 );
        float end = max( min( interval.y, frame.mMarch.y ), begin );
        float spacing = ( end - begin ) / max( frame.mResolution.w, 1u );
        for ( uint i = 0; i < frame.mResolution.w; ++i ) {
            float3 sample = position + float3( 0, begin + ( i + .5 ) * spacing, 0 );
            skyDepth += CloudDensity( frame, occluder, sample, CloudField( frame, occluder, sample ) ) *
                        dot( occluder.mExtinction.rgb, float3( 1.0 / 3.0 ) ) * spacing;
        }
    }
    if ( ( cloud.mFlags.w & 2u ) != 0 ) {
        sunDepth += ( 1 - TerrainSunVisibility( frame.mTerrainVertices, frame.mTerrainRecords, frame.mTerrainBounds, position, frame.mSunDirection.xyz ) ) * 80;
    }
    cloudVolumeOutputs[parameters.mOutput][voxel] = float4( min( sunDepth, 80.0 ), min( skyDepth, 80.0 ) );
}
