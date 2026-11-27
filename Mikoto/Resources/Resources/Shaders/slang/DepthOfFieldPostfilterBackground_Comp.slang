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

// Apply a 3x3 median filter to the gathered bokeh layer. This suppresses isolated gather
// outliers before the foreground/background layers are recombined.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    // Collect the complete 3x3 neighborhood before taking the componentwise median. This
    // removes isolated gather outliers while preserving the layer coverage channel.
    float4 values[kDoFMedianSampleCount];
    uint index = 0;
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            values[index++] = DoFLoad( parameters.mInputs[0], int2( thread.xy ) + int2( x, y ) );
        }
    }
    dofOutputs[parameters.mOutputs[0]][thread.xy] = DoFMedian9( values );
}
