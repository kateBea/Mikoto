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

// Reduce color and signed circle of confusion to half resolution. CoC-aware weights avoid
// mixing near and far layers equally across depth discontinuities.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float4 samples[kBilinearTapCount];
    float minimum = kDoFMaxHalfFloat;
    float maximum = -kDoFMaxHalfFloat;
    for ( uint i = 0; i < kBilinearTapCount; ++i ) {
        samples[i] = DoFLoad( parameters.mInputs[0], int2( thread.xy * 2 + uint2( i & 1, i >> 1 ) ) );
        minimum = min( minimum, samples[i].a );
        maximum = max( maximum, samples[i].a );
    }
    // Choose a representative near/far layer before averaging color. This avoids a foreground
    // sample and its background receiving the same reduction weight.
    float referenceCoC = minimum < -0.25 ? minimum : maximum;
    float3 color = 0.0;
    float sum = 0.0;
    for ( uint i = 0; i < kBilinearTapCount; ++i ) {
        float weight = saturate( 1.0 - abs( samples[i].a - referenceCoC ) / max( abs( referenceCoC ), 0.5 ) );
        color += samples[i].rgb * weight;
        sum += weight;
    }
    dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( color / max( sum, 1e-5 ), referenceCoC );
}
