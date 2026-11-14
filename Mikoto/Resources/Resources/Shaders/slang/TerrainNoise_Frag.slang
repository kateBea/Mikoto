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

#include "base/Noise_Helpers.slang"
#include "base/Memory_Helpers.slang"

// Flat layout matches TerrainNoiseSettings plus padding (48 bytes).
struct TerrainNoiseParameters {
    float frequency;
    float amplitude;
    float persistence;
    float lacunarity;
    uint octaves;
    float perlinFrequency;
    float perlinAmplitude;
    float perlinBlend;
    float2 offset;
    uint2 padding;
};

struct TerrainNoisePushConstants {
    BufferReference<TerrainNoiseParameters> settings;
};

MKT_DECLARE_PUSH_CONSTANT( TerrainNoisePushConstants, pushConstants );

[shader("fragment")]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    TerrainNoiseParameters parameters = pushConstants.settings[0];

    FBMSettings settings;
    settings.mFrequency = parameters.frequency;
    settings.mAmplitude = parameters.amplitude;
    settings.mPersistence = parameters.persistence;
    settings.mLacunarity = parameters.lacunarity;
    settings.mOctaves = parameters.octaves;

    float2 position = uv + parameters.offset;
    float fractal = FBM( position, settings );
    float perlin = PerlinNoise2D( position * parameters.perlinFrequency ) * parameters.perlinAmplitude;

    // Blend signed signals before mapping to height. Amplitudes above one can
    // intentionally flatten peaks/valleys as the final map is clamped to [0,1].
    float combined = lerp( fractal, perlin, parameters.perlinBlend );
    float height = saturate( combined * 0.5f + 0.5f );
    
    return float4( height, height, height, 1.0f );
}

