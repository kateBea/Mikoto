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

#define MKT_ATMOSPHERE_MULTIPLE_SCATTERING
#include "Atmosphere_Common.slang"

// One workgroup integrates the 64 directions for one LUT texel, then reduces
// their radiance and scattering fractions in shared memory. Based on Mirinae:
// https://github.com/SausageTaste/mirinae/blob/54293d427b1c107f0566a0db59935950546e1528/asset/slang/atmos/multi_scat.slang#L19-L125
// Storage format 2 is rgba16f, matching the frame graph texture.
typedef RWTexture2D<float4, 0, 2> AtmosphereStorageImage;
MKT_DECLARE_TEXTURE_UAV( AtmosphereStorageImage ) outputLut;

groupshared float3 radiance[64];
groupshared float3 fraction[64];

[shader( "compute" )]
[numthreads( 64, 1, 1 )]
void main( uint3 group : SV_GroupID, uint index : SV_GroupIndex ) {
    AtmosphereData a = Atmosphere();
    float2 uv = float2( group.xy ) / 31;
    float3 origin = float3( 0, lerp( a.mRadii.x + AtmosGroundOffset, a.mRadii.y - AtmosGroundOffset, uv.y ), 0 );
    float cosine = uv.x * 2 - 1;
    float3 sun = float3( sqrt( saturate( 1 - cosine * cosine ) ), cosine, 0 );

    // Uniform 8x8 quadrature on the sphere, with 20 integration steps per direction.
    float y = 1 - 2 * ( ( index / 8 ) + 0.5 ) / 8;
    float angle = 2 * AtmosPi * ( ( index % 8 ) + 0.5 ) / 8;
    float radius = sqrt( saturate( 1 - y * y ) );
    float3 direction = float3( radius * cos( angle ), y, radius * sin( angle ) );
    AtmosIntegration sample = AtmosIntegrate( a, origin, direction, sun, 1e6, 20, false, true, true );

    radiance[index] = sample.mRadiance;
    fraction[index] = sample.mScatteringFraction;
    GroupMemoryBarrierWithGroupSync();

    for ( uint stride = 32; stride > 0; stride /= 2 ) {
        if ( index < stride ) {
            radiance[index] += radiance[index + stride];
            fraction[index] += fraction[index + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if ( index == 0 ) {
        // Hillaire Eqs. 5, 7 and 10 sum the isotropic higher orders as a geometric series.
        // https://sebh.github.io/publications/egsr2020.pdf
        float3 averageRadiance = radiance[0] / 64;
        float3 averageFraction = fraction[0] / 64;
        outputLut[parameters.mOutput][group.xy] = float4( averageRadiance / max( float3( 0.001 ), 1 - averageFraction ), 1 );
    }
}
