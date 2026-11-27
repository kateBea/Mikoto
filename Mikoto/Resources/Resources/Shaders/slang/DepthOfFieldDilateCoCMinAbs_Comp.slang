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

// Refine the nearest-to-focus bounds inside the dilated blur footprint. The result supplements
// the outer bounds used for gather mip selection.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    int2 tile = int2( thread.xy );
    float4 bounds = DoFLoad( parameters.mInputs[1], tile );
    float kernel = max( -bounds.x, bounds.w );
    int radius = int( ceil( kernel / float( kDoFTileSize ) ) ) + 1;
    float foregroundMax = -kDoFMaxHalfFloat;
    float backgroundMin = kDoFMaxHalfFloat;
    for ( int y = -radius; y <= radius; ++y ) {
        for ( int x = -radius; x <= radius; ++x ) {
            int2 source = tile + int2( x, y );
            if ( any( source < 0 ) || any( source >= int2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { continue; }
            float separation = length( max( abs( float2( x, y ) ) - 1.0, 0.0 ) * 8.0 );
            if ( separation > kernel + 1.0 ) { continue; }
            float4 candidate = DoFLoad( parameters.mInputs[0], source );
            if ( candidate.x < 0.0 ) { foregroundMax = max( foregroundMax, candidate.y ); }
            if ( candidate.w > 0.0 ) { backgroundMin = min( backgroundMin, candidate.z ); }
        }
    }
    bounds.y = foregroundMax == -kDoFMaxHalfFloat ? 0.0 : foregroundMax;
    bounds.z = backgroundMin == kDoFMaxHalfFloat ? 0.0 : backgroundMin;
    dofOutputs[parameters.mOutputs[0]][thread.xy] = bounds;
}
