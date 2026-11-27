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

// Perform a complete one-dimensional FFT over each row or column in shared memory. Butterfly
// stages synchronize within the workgroup; transposed output prepares the following axis
// transform.

#include "base/Memory_Helpers.slang"
#include "base/WaterFFT_Helpers.slang"

struct WaterFftTransformParameters {
    BufferReference<float4> mInput;
    RWBufferReference<float4> mOutput;
    uint mSampleOffset;
    uint mColumns;
};

MKT_DECLARE_PUSH_CONSTANT( WaterFftTransformParameters, parameters );

groupshared float4 samples[kWaterFftResolution];

// One workgroup per 1D transform, with radix-2 butterflies in shared memory.
// https://www.bealto.com/gpu-fft.html
[shader("compute")]
[numthreads(kWaterFftResolution / 2, 1, 1)]
void main( uint3 group : SV_GroupID, uint lane : SV_GroupIndex ) {
    uint line = group.x;
    uint second = lane + kWaterFftResolution / 2;
    uint row = parameters.mSampleOffset + line * kWaterFftResolution;
    // Load the input in bit-reversed order so the iterative butterflies can run in place. Each
    // lane brings in two complex-packed field values.
    samples[reversebits( lane ) >> (32 - kWaterFftLogResolution)] = parameters.mInput[row + lane];
    samples[reversebits( second ) >> (32 - kWaterFftLogResolution)] = parameters.mInput[row + second];
    GroupMemoryBarrierWithGroupSync();

    // Grow the butterfly width from two samples to the entire transform. Barriers separate
    // reads and writes of each shared-memory stage.
    for ( uint halfWidth = 1; halfWidth < kWaterFftResolution; halfWidth <<= 1 ) {
        uint pair = lane & (halfWidth - 1);
        uint left = (lane / halfWidth) * 2 * halfWidth + pair;
        uint right = left + halfWidth;

        float sine;
        float cosine;
        float angle = kWaterFftTau * float( pair ) / float( 2 * halfWidth );
        sincos( angle, sine, cosine );

        float4 a = samples[left];
        float4 b = samples[right];
        float2 twiddle = float2( cosine, sine );
        b = float4( MultiplyWaterComplex( b.xy, twiddle ), MultiplyWaterComplex( b.zw, twiddle ) );
        samples[left] = a + b;
        samples[right] = a - b;

        // Pairs are disjoint within each stage. All lanes finish before the next stage.
        GroupMemoryBarrierWithGroupSync();
    }

    // Transpose on output so both axes read contiguous data. The second pass restores
    // X/Z order and writes the real-valued displacement field for the water shaders.
    uint firstOut = parameters.mSampleOffset + WaterFftIndex( uint2( line, lane ) );
    uint secondOut = parameters.mSampleOffset + WaterFftIndex( uint2( line, second ) );
    float4 firstValue = samples[lane];
    float4 secondValue = samples[second];

    // After the final axis transform, unpack the complex-packed fields into the X displacement,
    // height and Z displacement channels expected by water rendering.
    if ( parameters.mColumns != 0 ) {
        firstValue = float4( firstValue.z, firstValue.x, firstValue.w, 0 );
        secondValue = float4( secondValue.z, secondValue.x, secondValue.w, 0 );
    }

    parameters.mOutput[firstOut] = firstValue;
    parameters.mOutput[secondOut] = secondValue;
}
