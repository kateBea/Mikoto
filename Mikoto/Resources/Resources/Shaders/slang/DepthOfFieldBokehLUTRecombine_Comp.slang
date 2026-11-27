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

// Build normalized aperture-distance data for depth-of-field recombination. The radius accounts
// for the configured polygonal aperture shape.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float2 disk = ( float2( thread.xy ) + 0.5 ) / float2( parameters.mTargetWidth, parameters.mTargetHeight ) * 2.0 - 1.0;
    float radius = DoFApertureRadius( atan2( disk.y, disk.x ), parameters );
    // Distance in aperture space, for subpixel intersection tests on full-resolution samples.
    dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( length( disk ) / max( radius, 1e-4 ), radius, 0, 1 );
}
