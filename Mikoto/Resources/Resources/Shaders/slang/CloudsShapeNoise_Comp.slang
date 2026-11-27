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

// Bake low-frequency billow structure for procedural cloud shapes. Multiple noise scales
// establish the large lobes before the density shader applies finer erosion.

#include "base/PsrdNoise.slang"
#include "Clouds_Common.slang"

// Derivative-warped fBm is the puffy-sphere construction used by Cumulus.
// https://stegu.github.io/psrdnoise/3d-gallery/puffysphere-threejs.html
// Accumulate folded gradient-noise octaves into a billowy field. The low-frequency lobes
// control the silhouette while higher octaves add smaller bulges.
float CloudBillowFbm( float3 uv, uint octaves ) {
    float3 warp = 0;
    float frequency = 1;
    float amplitude = .4;
    float value = .5;
    for ( uint octave = 0; octave < octaves; ++octave ) {
        float3 gradient;
        value += amplitude * CloudPsrdNoise( uv * 6 * frequency + warp, float3( 6 ), frequency * 2, gradient );
        amplitude *= .5;
        frequency *= 2;
        warp += gradient * amplitude * .6;
    }
    return saturate( value );
}

[shader( "compute" )]
[numthreads( kVolumeGroupSide, kVolumeGroupSide, kVolumeGroupSide )]
void main( uint3 voxel : SV_DispatchThreadID ) {
    if ( any( voxel >= 128 ) ) {
        return;
    }
    float3 uv = ( float3( voxel ) + .5 ) / 128;
    float profile = CloudBillowFbm( uv, 8 );
    float detail = CloudBillowFbm( uv + float3( .25, .5, .75 ), 3 );
    cloudVolumeOutputs[parameters.mOutput][voxel] = float4( profile, detail, profile, detail );
}
