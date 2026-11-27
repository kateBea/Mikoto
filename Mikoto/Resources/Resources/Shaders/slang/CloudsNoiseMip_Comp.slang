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

// Reduce a 3D cloud-noise level into its next mip. Coarser noise footprints help keep distant
// cloud detail from aliasing.

#include "Clouds_Common.slang"

// Separate 3D images provide the filtered levels through the existing RHI,
// including backends whose frame-graph UAV interface does not select a mip.
[shader( "compute" )]
[numthreads( kVolumeGroupSide, kVolumeGroupSide, kVolumeGroupSide )]
void main( uint3 voxel : SV_DispatchThreadID ) {
    uint width;
    uint height;
    uint depth;
    cloudVolumeOutputs[parameters.mOutput].GetDimensions( width, height, depth );
    if ( any( voxel >= uint3( width, height, depth ) ) ) {
        return;
    }
    float4 value = 0;
    for ( uint z = 0; z < kLinearFilterSide; ++z ) {
        for ( uint y = 0; y < kLinearFilterSide; ++y ) {
            for ( uint x = 0; x < kLinearFilterSide; ++x ) {
                value += cloudVolumes[parameters.mInput].Load( int4( voxel * 2 + uint3( x, y, z ), 0 ) );
            }
        }
    }
    cloudVolumeOutputs[parameters.mOutput][voxel] = value * ( 1.0 / float( kTrilinearTapCount ) );
}
