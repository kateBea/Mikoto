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

// Convert scene depth into a signed circle of confusion and attach it to HDR color. Negative
// radii identify foreground blur; positive radii identify background blur.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( parameters.mWidth, parameters.mHeight ) ) ) { return; }

    float coc = DoFCoC( DoFLoad( parameters.mInputs[1], int2( pixel ) ).x, parameters );
    dofOutputs[parameters.mOutputs[0]][pixel] = float4( coc, 0, 0, 0 );

    if ( any( pixel >= DoFHalfSize( parameters ) ) ) { return; }

    float4 samples[kBilinearTapCount];
    float minimum = parameters.mMaxRadius;
    float maximum = -parameters.mMaxRadius;
    for ( uint i = 0; i < kBilinearTapCount; ++i ) {
        int2 source = int2( pixel * 2 + uint2( i & 1, i >> 1 ) );
        float sampleCoC = DoFCoC( DoFLoad( parameters.mInputs[1], source ).x, parameters );
        samples[i] = float4( DoFClampColor( DoFLoad( parameters.mInputs[0], source ).rgb ), sampleCoC );
        minimum = min( minimum, sampleCoC );
        maximum = max( maximum, sampleCoC );
    }

    // Retain the closest foreground surface at silhouettes. Within one layer,
    // prefer the largest blur footprint so small foreground features survive reduction.
    float referenceCoC = minimum < -0.5 ? minimum : maximum;
    float3 color = 0.0;
    float weightSum = 0.0;
    for ( uint i = 0; i < kBilinearTapCount; ++i ) {
        float weight = saturate( 1.0 - abs( samples[i].a - referenceCoC ) / max( abs( referenceCoC ), 1.0 ) );
        color += samples[i].rgb * weight;
        weightSum += weight;
    }
    // All reduced CoCs remain in HALF-resolution pixels, including subsequent mip levels.
    dofOutputs[parameters.mOutputs[1]][pixel] = float4( color / max( weightSum, 1e-5 ), referenceCoC * 0.5 );
}
