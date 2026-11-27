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

// Generate the procedural height-noise source used by terrain geometry. Octave settings control
// the terrain frequency content before mesh construction.

#include "base/Noise_Helpers.slang"
#include "base/Memory_Helpers.slang"

// Flat layout matches TerrainNoiseSettings plus padding (48 bytes).
struct TerrainNoiseParameters {
    float mFrequency;
    float mAmplitude;
    float mPersistence;
    float mLacunarity;
    uint mOctaves;
    float mPerlinFrequency;
    float mPerlinAmplitude;
    float mPerlinBlend;
    float2 mOffset;
    uint2 mPadding;
};

struct TerrainNoisePushConstants {
    BufferReference<TerrainNoiseParameters> mSettings;
};

MKT_DECLARE_PUSH_CONSTANT( TerrainNoisePushConstants, pushConstants );

[shader("fragment")]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    TerrainNoiseParameters parameters = pushConstants.mSettings[0];

    FBMSettings settings;
    settings.mFrequency = parameters.mFrequency;
    settings.mAmplitude = parameters.mAmplitude;
    settings.mPersistence = parameters.mPersistence;
    settings.mLacunarity = parameters.mLacunarity;
    settings.mOctaves = parameters.mOctaves;

    float2 position = uv + parameters.mOffset;
    float fractal = FBM( position, settings );
    float perlin = PerlinNoise2D( position * parameters.mPerlinFrequency ) * parameters.mPerlinAmplitude;

    // Blend signed signals before mapping to height. Amplitudes above one can
    // intentionally flatten peaks/valleys as the final map is clamped to [0,1].
    float combined = lerp( fractal, perlin, parameters.mPerlinBlend );
    float height = saturate( combined * 0.5f + 0.5f );
    
    return float4( height, height, height, 1.0f );
}

