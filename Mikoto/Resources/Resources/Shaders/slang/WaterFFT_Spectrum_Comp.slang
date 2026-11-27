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

// Evolve the ocean frequency spectrum at the current time. The complex frequency data feeds the
// inverse FFT that produces spatial displacement.

#include "base/Memory_Helpers.slang"
#include "base/WaterFFT_Helpers.slang"

struct WaterFftSpectrumParameters {
    BufferReference<float2> mInitialSpectrum;
    RWBufferReference<float4> mOutput;
    uint mInitialOffset;
    uint mOutputOffset;
    float mPatchLength;
    float mTime;
    float mChoppiness;
};

MKT_DECLARE_PUSH_CONSTANT( WaterFftSpectrumParameters, parameters );

[shader("compute")]
[numthreads(kWaterFftTile, kWaterFftTile, 1)]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint2 coordinate = thread.xy;
    uint index = WaterFftIndex( coordinate );
    uint mirror = WaterFftIndex( (kWaterFftResolution - coordinate) & kWaterFftMask );

    if ( all( coordinate == 0 ) || any( coordinate == kWaterFftResolution / 2 ) ) {
        parameters.mOutput[parameters.mOutputOffset + index] = float4( 0.0 );
        return;
    }

    int2 signedCoordinate = int2( coordinate );
    signedCoordinate -= select( coordinate > kWaterFftResolution / 2, int2( kWaterFftResolution ), int2( 0 ) );
    float2 waveNumber = float2( signedCoordinate ) * (kWaterFftTau / parameters.mPatchLength);
    float waveNumberLength = length( waveNumber );
    const float gravity = 9.81;

    // Tessendorf's deep-water dispersion, quantized so a wrapped clock stays continuous.
    // https://3dmodelizm.ru/ext/graphics.ucsd.edu/courses/rendering/2005/jdewall/tessendorf.pdf
    const float loopSeconds = 600.0;
    const float angularStep = kWaterFftTau / loopSeconds;
    float omega = round( sqrt( gravity * waveNumberLength ) / angularStep ) * angularStep;
    float sine;
    float cosine;
    sincos( omega * parameters.mTime, sine, cosine );

    // With the positive-exponent inverse transform, h0(k) advances in the +k direction.
    // Combine the initial wave with its conjugate mirror at opposite time phase. This Hermitian
    // pairing produces a real spatial height field after the inverse transform.
    float2 forward = parameters.mInitialSpectrum[parameters.mInitialOffset + index];
    float2 backward = parameters.mInitialSpectrum[parameters.mInitialOffset + mirror];
    float2 height = MultiplyWaterComplex( forward, float2( cosine, -sine ) ) +
        MultiplyWaterComplex( backward * float2( 1, -1 ), float2( cosine, sine ) );

    // Pack the two choppy displacement spectra into one complex transform. After the
    // inverse FFT its real component is displacement X and its imaginary component is Z.
    float2 direction = waveNumber / waveNumberLength;
    float2 horizontal = float2( -height.y, height.x ) * parameters.mChoppiness;
    float2 shiftX = direction.x * horizontal;
    float2 shiftZ = direction.y * horizontal;
    parameters.mOutput[parameters.mOutputOffset + index] = float4( height, shiftX + float2( -shiftZ.y, shiftZ.x ) );
}
