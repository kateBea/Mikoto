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

// Derive surface slopes and compression from ocean displacement. These fields drive water
// normals and foam after the inverse FFT.

#include "base/Memory_Helpers.slang"
#include "base/WaterFFT_Helpers.slang"

struct WaterFftGradientParameters {
    BufferReference<float4> mDisplacement;
    RWBufferReference<float4> mGradients;
    uint mSampleOffset;
    float mInverseDoubleSpacing;
};

MKT_DECLARE_PUSH_CONSTANT( WaterFftGradientParameters, parameters );

float3 LoadWaterDisplacement( uint2 coordinate ) {
    return parameters.mDisplacement[parameters.mSampleOffset + WaterFftIndex( coordinate & kWaterFftMask )].xyz;
}

[shader("compute")]
[numthreads(kWaterFftTile, kWaterFftTile, 1)]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint2 coordinate = thread.xy;
    const uint2 stepX = uint2( 1, 0 );
    const uint2 stepZ = uint2( 0, 1 );

    // Arm's gradient pass samples neighbouring displacement values. Keep both
    // tangents so the water draw can apply its world transform and compute foam.
    // https://arm-software.github.io/opengl-es-sdk-for-android/ocean_f_f_t.html
    // Finite differences of displaced neighbors produce the two water tangents. Preserve both
    // vectors so surface normals and compression can use the same derivatives.
    float3 tangentX = float3( 1, 0, 0 ) + parameters.mInverseDoubleSpacing *
        (LoadWaterDisplacement( coordinate + stepX ) - LoadWaterDisplacement( coordinate - stepX ));
    float3 tangentZ = float3( 0, 0, 1 ) + parameters.mInverseDoubleSpacing *
        (LoadWaterDisplacement( coordinate + stepZ ) - LoadWaterDisplacement( coordinate - stepZ ));

    uint index = (parameters.mSampleOffset + WaterFftIndex( coordinate )) * 2;
    parameters.mGradients[index] = float4( tangentX, 0 );
    parameters.mGradients[index + 1] = float4( tangentZ, 0 );
}
