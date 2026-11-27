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

// Bake cloud detail noise into a 3D texture. The generated channels are sampled repeatedly by
// density evaluation instead of recomputing lattice noise per ray step.

#include "base/Alligator_Helpers.slang"
#include "Clouds_Common.slang"

// Nubis3: low/high curled wisps in RG, low/high billows in BA.
// Noise is generated once, independently of weather, camera and frame jitter.
[shader( "compute" )]
[numthreads( kVolumeGroupSide, kVolumeGroupSide, kVolumeGroupSide )]
void main( uint3 voxel : SV_DispatchThreadID ) {
    if ( any( voxel >= 128 ) ) {
        return;
    }
    float3 uv = ( float3( voxel ) + .5 ) / 128;
    float3 curl = CloudDetailCurl( uv * 4, 4 ) * .45;
    float low = CloudAlligator( uv * 8, 8 );
    float high = CloudAlligator( uv * 16, 16 );
    float lowWisp = 1 - CloudAlligator( uv * 8 + curl, 8 );
    float highWisp = 1 - CloudAlligator( uv * 16 + curl * 2, 16 );
    cloudVolumeOutputs[parameters.mOutput][voxel] = float4( lowWisp, highWisp, low, high );
}
