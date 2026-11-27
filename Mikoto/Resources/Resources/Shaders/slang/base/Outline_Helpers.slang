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

// Encode valid silhouette seeds and decode their pixel coordinates. The reserved invalid value
// lets jump flooding distinguish empty pixels from genuine boundary seeds.

#pragma once

// Ben Golus's RG16_SNORM encoding reserves y == -1 exclusively for invalid seeds.
// Valid positions may have a negative x or y, so testing either coordinate < 0 is incorrect.
// Source: https://gist.github.com/bgolus/a18c1a3fc9af2d73cc19169a809eb195
static const float kOutlineSnormLimit = 32766.0f / 32767.0f;
static const float2 kOutlineEncodeOffset = float2( 1.0f, kOutlineSnormLimit );
static const float2 kOutlineEncodeScale = float2( 2.0f, 1.0f + kOutlineSnormLimit );
static const float2 kInvalidOutlineSeed = float2( -1.0f );

bool IsOutlineSeedValid( float2 encoded ) {
    return encoded.y != -1.0f;
}

// Store the valid boundary-pixel coordinates in the seed representation. Invalid pixels use a
// separate reserved encoding.
float2 EncodeOutlineSeed( float2 pixelPosition, float2 dimensions ) {
    // SV_Position and decoded coordinates both use pixel centers at (x + 0.5, y + 0.5).
    // Clamp subpixel offsets at screen boundaries before writing to the normalized format.
    return clamp( pixelPosition, float2( 0.0f ), dimensions ) / dimensions *
        kOutlineEncodeScale - kOutlineEncodeOffset;
}

// Recover the source boundary coordinate for distance comparison in jump flooding and outline
// composition.
float2 DecodeOutlineSeed( float2 encoded, float2 dimensions ) {
    return ( encoded + kOutlineEncodeOffset ) * dimensions / kOutlineEncodeScale;
}
