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

#include "base/Memory_Helpers.slang"
#include "base/WaterFFT_Helpers.slang"

struct WaterFftTransformParameters {
    BufferReference<float4> input;
    RWBufferReference<float4> output;
    uint sampleOffset;
    uint columns;
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
    uint row = parameters.sampleOffset + line * kWaterFftResolution;
    samples[reversebits( lane ) >> (32 - kWaterFftLogResolution)] = parameters.input[row + lane];
    samples[reversebits( second ) >> (32 - kWaterFftLogResolution)] = parameters.input[row + second];
    GroupMemoryBarrierWithGroupSync();

    for ( uint halfWidth = 1; halfWidth < kWaterFftResolution; halfWidth <<= 1 ) {
        uint pair = lane & (halfWidth - 1);
        uint left = (lane / halfWidth) * 2 * halfWidth + pair;
        uint right = left + halfWidth;

        float sine, cosine;
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
    uint firstOut = parameters.sampleOffset + WaterFftIndex( uint2( line, lane ) );
    uint secondOut = parameters.sampleOffset + WaterFftIndex( uint2( line, second ) );
    float4 firstValue = samples[lane];
    float4 secondValue = samples[second];

    if ( parameters.columns != 0 ) {
        firstValue = float4( firstValue.z, firstValue.x, firstValue.w, 0 );
        secondValue = float4( secondValue.z, secondValue.x, secondValue.w, 0 );
    }

    parameters.output[firstOut] = firstValue;
    parameters.output[secondOut] = secondValue;
}
