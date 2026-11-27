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

// Estimate light returning from other directions through the atmosphere. This LUT supplies the
// multiple-scattering contribution used by later sky integrations.

#define MKT_ATMOSPHERE_MULTIPLE_SCATTERING
#include "Atmosphere_Common.slang"

// One workgroup integrates the 64 directions for one LUT texel, then reduces
// their radiance and scattering fractions in shared memory. Based on Mirinae:
// https://github.com/SausageTaste/mirinae/blob/54293d427b1c107f0566a0db59935950546e1528/asset/slang/atmos/multi_scat.slang#L19-L125
// Storage format 2 is rgba16f, matching the frame graph texture.
typedef RWTexture2D<float4, 0, 2> AtmosphereStorageImage;
MKT_DECLARE_TEXTURE_UAV( AtmosphereStorageImage ) outputLut;

// One workgroup integrates an 8x8 direction quadrature for one texel of the 32x32 multiple-scattering LUT.
static const uint kAtmosMultipleLutSize = 32;
static const uint kAtmosMultipleQuadratureSide = 8;
static const uint kAtmosMultipleDirectionCount = 64;
static const uint kAtmosMultipleIntegrationSteps = 20;

groupshared float3 radiance[kAtmosMultipleDirectionCount];
groupshared float3 fraction[kAtmosMultipleDirectionCount];

[shader( "compute" )]
[numthreads( kAtmosMultipleDirectionCount, 1, 1 )]
void main( uint3 group : SV_GroupID, uint index : SV_GroupIndex ) {
    AtmosphereData a = Atmosphere();
    float2 uv = float2( group.xy ) / float( kAtmosMultipleLutSize - 1 );
    float3 origin = float3( 0, lerp( a.mRadii.x + AtmosGroundOffset, a.mRadii.y - AtmosGroundOffset, uv.y ), 0 );
    float cosine = uv.x * 2 - 1;
    float3 sun = float3( sqrt( saturate( 1 - cosine * cosine ) ), cosine, 0 );

    // Uniform 8x8 quadrature on the sphere, with 20 integration steps per direction.
    float y = 1 - 2 * ( ( index / kAtmosMultipleQuadratureSide ) + 0.5 ) / float( kAtmosMultipleQuadratureSide );
    float angle = 2 * AtmosPi * ( ( index % kAtmosMultipleQuadratureSide ) + 0.5 ) / float( kAtmosMultipleQuadratureSide );
    float radius = sqrt( saturate( 1 - y * y ) );
    float3 direction = float3( radius * cos( angle ), y, radius * sin( angle ) );
    AtmosIntegration sample = AtmosIntegrate( a, origin, direction, sun, 1e6, kAtmosMultipleIntegrationSteps, false, true, true );

    radiance[index] = sample.mRadiance;
    fraction[index] = sample.mScatteringFraction;
    GroupMemoryBarrierWithGroupSync();

    for ( uint stride = kAtmosMultipleDirectionCount / 2; stride > 0; stride /= 2 ) {
        if ( index < stride ) {
            radiance[index] += radiance[index + stride];
            fraction[index] += fraction[index + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if ( index == 0 ) {
        // Hillaire Eqs. 5, 7 and 10 sum the isotropic higher orders as a geometric series.
        // https://sebh.github.io/publications/egsr2020.pdf
        float3 averageRadiance = radiance[0] / float( kAtmosMultipleDirectionCount );
        float3 averageFraction = fraction[0] / float( kAtmosMultipleDirectionCount );
        outputLut[parameters.mOutput][group.xy] = float4( averageRadiance / max( float3( 0.001 ), 1 - averageFraction ), 1 );
    }
}
