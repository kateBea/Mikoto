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

#pragma once
#include "Memory_Helpers.slang"
#include "WaterFFT_Helpers.slang"

// Matches WaterWaveGPU/WaterSurfaceGPU in WaterSimulationModule.cc.
struct WaterWave {
    float4 directionAmplitudeFrequency;
    float4 phaseHorizontal;
};
struct WaterSurface {
    float4x4 world;
    float4 colorRoughness;
    float4 attenuation;
    float4 optics;
    uint4 geometry;
    uint4 simulation;
    float4 debugPreviewColor;
    float4 fft; // reciprocal patch length
};
struct WaterParameters {
    Pointer vertices;
    Pointer indices;
    Pointer camera;
    Pointer surfaces;
    Pointer waves;
    Pointer lights;
    uint surface;
    uint lightCount;
    uint environment;
    uint irradiance;
    uint brdf;
    uint sampler;
    float environmentScale;
    int toneMap;
    uint sceneColor;
    uint sceneDepth;
    uint screenSampler;
    Pointer fftDisplacement;
    Pointer fftGradients;
};

struct WaterVaryings {
    float4 position : SV_Position;
    float2 grid : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float2 restPosition : TEXCOORD2;
};

static const uint kWaterSimulationFft = 2; // WaterSimulationType::eFastFourierTransform.

// Periodic bilinear lookup in the FFT output. Coordinates use the same rest plane
// in the vertex and pixel shaders, including when the surface moves horizontally.
struct WaterFftSample {
    uint2 lower;
    uint2 upper;
    float2 blend;
};

WaterFftSample GetWaterFftSample( float2 rest, float inversePatchLength ) {
    float2 samplePosition = frac( rest * inversePatchLength ) * kWaterFftResolution;
    WaterFftSample sample;
    sample.lower = uint2( samplePosition ) & kWaterFftMask;
    sample.upper = (sample.lower + 1) & kWaterFftMask;
    sample.blend = frac( samplePosition );
    return sample;
}

float4 SampleWaterFft( Pointer field, uint offset, WaterFftSample sample, uint stride ) {
    BufferReference<float4> values = (BufferReference<float4>)field;
    float4 a = values[offset + WaterFftIndex( sample.lower ) * stride];
    float4 b = values[offset + WaterFftIndex( uint2( sample.upper.x, sample.lower.y ) ) * stride];
    float4 c = values[offset + WaterFftIndex( uint2( sample.lower.x, sample.upper.y ) ) * stride];
    float4 d = values[offset + WaterFftIndex( sample.upper ) * stride];
    return lerp( lerp( a, b, sample.blend.x ), lerp( c, d, sample.blend.x ), sample.blend.y );
}

// GPU Gems 1, chapter 1: sum directional waves at the REST position. Sampling
// subsequent waves at an already displaced position would change the model.
// Local XZ is the water plane; speed is world units/second before entity scale.
void EvaluateWater( float3 rest, WaterSurface surface, Pointer waveAddress,
                    out float3 position, out float3 tangentX, out float3 tangentZ ) {
    position = rest;
    tangentX = float3( 1, 0, 0 );
    tangentZ = float3( 0, 0, 1 );
    BufferReference<WaterWave> waves = (BufferReference<WaterWave>)waveAddress;
    for ( uint i = 0; i < surface.simulation.y; ++i ) {
        WaterWave wave = waves[surface.simulation.x + i];
        float2 direction = wave.directionAmplitudeFrequency.xy;
        float amplitude = wave.directionAmplitudeFrequency.z;
        float frequency = wave.directionAmplitudeFrequency.w; // 2*pi / wavelength
        float phase = frequency * dot( direction, rest.xz ) + wave.phaseHorizontal.x;
        float sine, cosine;
        sincos( phase, sine, cosine );
        position.y += amplitude * sine;
        float slope = amplitude * frequency * cosine;
        tangentX.y += direction.x * slope;
        tangentZ.y += direction.y * slope;

        if ( surface.simulation.z == 1 ) { // Gerstner: horizontal crest compression
            float horizontal = wave.phaseHorizontal.y;
            position.xz += horizontal * direction * cosine;
            float compression = horizontal * frequency * sine;
            tangentX.xz -= compression * direction.x * direction;
            tangentZ.xz -= compression * direction.y * direction;
        }
    }
}
