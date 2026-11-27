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

// Convert the median MSDF distance into pixel coverage using derivatives. Fill and outline
// coverage are combined with glyph color for antialiased text edges.

#pragma once

#include "base/Base.slang"
#include "base/FontRender_Helpers.slang"
#include "base/Memory_Helpers.slang"

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_PUSH_CONSTANT( TextDrawParams, parameters );

[shader( "fragment" )]
float4 main( TextVertexOutput input ) : SV_Target0 {
    Texture2D atlas = textures[NonUniformResourceIndex( input.mTextureIndex )];
    float4 texel = atlas.Sample( samplers[parameters.mTextSamplerID], input.mTexCoord );
    if ( input.mImageType == 1 ) { // GlyphImageType::eColor
        // FreeType supplies premultiplied sRGB. Filter premultiplied texels to
        // avoid dark fringes, then return straight alpha for this pipeline.
        return float4( texel.a > 0 ? texel.rgb / texel.a : float3( 0 ), texel.a * input.mColor.a );
    }
    // MTSDF stores sharp contour distances in RGB and true distance in alpha.
    // Use the former for the letter and the latter for a rounded outside stroke.
    // Offsetting the RGB median produces false spikes away from the contour.
    float2 distance = float2( Median( texel.rgb ), texel.a );

    // Measure the *edge's* screen-space footprint, not an average UV scale.
    // The two differ on rotated or foreshortened world-space text. Derivatives
    // must be evaluated before the sampling loop, in uniform control flow.
    float2 footprint = max( fwidth( distance ), float2( 1e-5 ) );
    float2 dx = ddx( input.mTexCoord );
    float2 dy = ddy( input.mTexCoord );

    // Integrate four half-pixel footprints. Filtering distance first and then
    // taking its median cannot resolve all the corners inside a small pixel.
    // Averaging coverage instead also antialiases the fill when outlines are off.
    float2 coverage = float2( 0 );
    [unroll]
    for ( uint y = 0; y < kLinearFilterSide; ++y ) {
        [unroll]
        for ( uint x = 0; x < kLinearFilterSide; ++x ) {
            float2 uv = input.mTexCoord + ( float( x ) - 0.5 ) * 0.5 * dx
                                      + ( float( y ) - 0.5 ) * 0.5 * dy;
            float4 sample = atlas.SampleLevel( samplers[parameters.mTextSamplerID], uv, 0 );
            float2 d = float2( Median( sample.rgb ), sample.a ) - 0.5;
            d.y += max( input.mOutlineWidth, 0.0 );
            float2 opacity = saturate( 2.0 * d / footprint + 0.5 );
            // Never let the rounded distance field cut into sharp fill corners.
            opacity.y = max( opacity.x, opacity.y );
            coverage += opacity * 0.25;
        }
    }

    if ( input.mOutlineWidth <= 0 ) {
        return float4( input.mColor.rgb, input.mColor.a * coverage.x );
    }
    return CompositeTextOutline( coverage.x, coverage.y, input.mColor, input.mOutlineColor );
}
