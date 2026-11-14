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

#include "Base.slang"

// Matches FractalBrownianMotionParameters in PrepassModule.hh (16 bytes).
struct NoiseParameters {
    uint octaves;
    float persistence;
    float lacunarity;
    float baseFrequency;
};

// Improved 2D gradient noise, following the construction described at:
// https://rtouti.github.io/graphics/perlin-noise-algorithm
// A fixed shuffled permutation keeps exports reproducible. Masking each lookup
// wraps indices instead of duplicating the table; the noise repeats every 256 cells.
static const uint noisePermutation[256] = {
    71, 93, 206, 107, 254, 229, 101, 164, 133, 39, 65, 35, 11, 220, 5, 31,
    48, 224, 186, 33, 239, 207, 198, 202, 9, 230, 29, 140, 75, 10, 19, 153,
    176, 214, 218, 209, 216, 205, 184, 42, 154, 183, 43, 55, 250, 111, 138, 115,
    18, 126, 123, 231, 182, 114, 3, 109, 58, 201, 132, 188, 73, 100, 253, 79,
    45, 62, 149, 80, 150, 161, 2, 81, 179, 152, 180, 40, 17, 46, 151, 145,
    103, 113, 91, 87, 221, 242, 97, 203, 118, 137, 67, 98, 110, 50, 141, 99,
    49, 217, 233, 195, 130, 177, 51, 175, 163, 72, 25, 189, 37, 119, 158, 169,
    147, 155, 222, 60, 24, 74, 127, 135, 238, 187, 143, 181, 0, 162, 16, 159,
    199, 26, 85, 178, 77, 30, 54, 28, 47, 38, 15, 124, 146, 190, 8, 157,
    234, 211, 125, 168, 53, 200, 129, 68, 95, 235, 41, 104, 193, 246, 57, 171,
    204, 66, 245, 174, 105, 102, 213, 36, 128, 122, 6, 191, 69, 142, 52, 156,
    14, 134, 139, 88, 194, 120, 215, 232, 210, 94, 27, 247, 63, 219, 192, 255,
    166, 252, 244, 12, 225, 144, 23, 148, 196, 34, 240, 227, 173, 112, 170, 223,
    117, 82, 165, 251, 90, 249, 131, 212, 21, 86, 96, 56, 83, 70, 185, 172,
    121, 241, 116, 76, 78, 208, 61, 243, 4, 248, 167, 136, 1, 106, 7, 236,
    237, 22, 89, 20, 59, 226, 92, 84, 160, 228, 13, 44, 197, 64, 32, 108
};

uint NoiseCornerHash( int2 corner ) {
    uint x = uint( corner.x ) & 255u;
    uint y = uint( corner.y ) & 255u;
    return noisePermutation[(noisePermutation[x] + y) & 255u];
}

float NoiseGradientDot( int2 corner, float2 displacement ) {
    uint direction = NoiseCornerHash( corner ) & 3u;
    float2 gradient = float2(
        (direction == 0u || direction == 3u) ? 1.0f : -1.0f,
        direction < 2u ? 1.0f : -1.0f );
    return dot( gradient, displacement );
}

float2 NoiseFade( float2 t ) {
    // Quintic interpolation has zero first and second derivatives at cell edges.
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float PerlinNoise2D( float2 position ) {
    // Keep integer lattice conversion bounded, including large octave frequencies.
    position = fmod( position, 256.0f );
    int2 cell = int2( floor( position ) );
    float2 offset = frac( position );
    float2 blend = NoiseFade( offset );

    float lowerLeft = NoiseGradientDot( cell, offset );
    float lowerRight = NoiseGradientDot( cell + int2( 1, 0 ), offset - float2( 1, 0 ) );
    float upperLeft = NoiseGradientDot( cell + int2( 0, 1 ), offset - float2( 0, 1 ) );
    float upperRight = NoiseGradientDot( cell + int2( 1, 1 ), offset - float2( 1, 1 ) );

    float lower = lerp( lowerLeft, lowerRight, blend.x );
    float upper = lerp( upperLeft, upperRight, blend.x );
    return lerp( lower, upper, blend.y );
}

struct FBMSettings {
    float mFrequency;
    float mAmplitude;
    float mPersistence;
    float mLacunarity;
    uint mOctaves;
};

float FBM( float2 position, FBMSettings settings ) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = settings.mFrequency;
    float normalization = 0.0f;
    for ( uint i = 0; i < settings.mOctaves; ++i ) {
        value += PerlinNoise2D( position * frequency ) * amplitude;
        normalization += amplitude;
        frequency *= settings.mLacunarity;
        amplitude *= settings.mPersistence;
    }
    // Apply overall amplitude after normalization; otherwise it would cancel out.
    return normalization > 0.0f ? (value / normalization) * settings.mAmplitude : 0.0f;
}

