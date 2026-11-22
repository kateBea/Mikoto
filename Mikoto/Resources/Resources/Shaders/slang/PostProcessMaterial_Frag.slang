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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "base/ChromaticAberration_Helpers.slang"

// Matches renderer::PostProcessMaterialData. Records compose in stable owner order.
struct PostProcessMaterialData {
    float4 TintColor;
    float Contrast;
    float Saturation;
    uint Stage;
    float FilmGrainAmount;
};

struct DrawParams {
    Pointer mMaterials;
    uint mSceneImage;
    uint mSampler;
    uint mMaterialCount;
    uint mStage;
    float mElapsedSeconds;
    uint mPadding;
    float2 mChromaticOffsetPixels;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_SAMPLERS samplers;

// Lettier's additive, animated film grain; elapsed seconds are not the frame delta.
// https://lettier.github.io/3d-game-shaders-for-beginners/film-grain.html
// Adapted from David Lettier's demonstration (BSD-3-Clause); see THIRD_PARTY.md.
float4 ApplyFilmGrain( float4 color, float2 pixel, float elapsedSeconds, float amount ) {
    float toRadians = 3.14f / 180.0f;
    float intensity = frac( 10000.0f * sin( ( pixel.x + pixel.y * elapsedSeconds ) * toRadians ) );
    color.rgb += amount * intensity;
    return color;
}

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    Texture2D source = textures[parameters.mSceneImage];
    SamplerState sampler = samplers[parameters.mSampler];

    float4 color;
    if ( any( parameters.mChromaticOffsetPixels != 0.0f ) ) {
        uint width, height;
        source.GetDimensions( width, height );
        float2 pixelSize = 1.0f / float2( width, height );
        color = SampleChromaticAberration( source, sampler, uv, pixelSize, parameters.mChromaticOffsetPixels );
    }
    else {
        color = source.SampleLevel( sampler, uv, 0 );
    }

    BufferReference<PostProcessMaterialData> materials = ( BufferReference<PostProcessMaterialData> )parameters.mMaterials;

    // Keep the accumulated color in registers: separate draws over the source would overwrite prior materials.
    for ( uint index = 0; index < parameters.mMaterialCount; ++index ) {
        PostProcessMaterialData material = materials[index];

        if ( material.Stage != parameters.mStage ) { continue; }

        // Each material consumes the previous material's result, matching
        // PostProcessMaterial::Evaluate; identity properties preserve the input.
        float luminance = dot( color.rgb, float3( 0.2126f, 0.7152f, 0.0722f ) );
        float3 saturated = lerp( luminance.xxx, color.rgb, material.Saturation );
        float3 graded = max( ( saturated - 0.5f ) * material.Contrast + 0.5f, 0.0f ) * material.TintColor.rgb;
        color.rgb = lerp( color.rgb, graded, material.TintColor.a );

        if ( material.FilmGrainAmount > 0.0f ) {
            color = ApplyFilmGrain( color, position.xy, parameters.mElapsedSeconds, material.FilmGrainAmount );
        }
    }

    return color;
}
