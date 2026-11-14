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

// Keep in sync with TextDrawParameters (384 bytes, matrices use column-major layout).

#pragma once

struct FontRenderParams {
    float4x4 Projection;
    float4x4 View;
    float4x4 Model;
    float4 Position;
    float4 Size;
    float4 Color;
    float2 TextureCoords[4];
    uint TextureIndex;
    float DistanceRange;
    uint ImageType;
    float OutlineWidth;
    float4 GlyphPlaneBounds;
    float4 BandTransform;
    uint4 GlyphBands;
    float2 Viewport;
    uint CurveTextureID;
    uint BandTextureID;
    float4 OutlineColor;
    uint4 SlugStroke;
};

struct TextDrawParams {
    uint TextSamplerID;
    uint TextParametersID;
    uint GlyphOffset;
};

struct TextVertexOutput {
    float4 PositionH : SV_Position;
    float2 TexCoord : TEXCOORD0;
    nointerpolation float4 Color : TEXCOORD1;
    nointerpolation uint TextureIndex : TEXCOORD2;
    nointerpolation float DistanceRange : TEXCOORD3;
    nointerpolation uint ImageType : TEXCOORD4;
    nointerpolation float OutlineWidth : TEXCOORD5;
    nointerpolation float4 OutlineColor : TEXCOORD6;
};

float Median( float3 v ) {
    return max( min( v.r, v.g ), min( max( v.r, v.g ), v.b ) );
}

float ScreenPxRange( Texture2D atlas, float2 uv, float pixelRange ) {
    uint width, height;
    atlas.GetDimensions( width, height );
    float2 unitRange = pixelRange / float2( width, height );
    float2 screenTexSize = 1.0 / max( fwidth( uv ), float2( 1e-7 ) );
    return max( 0.5 * dot( unitRange, screenTexSize ), 1.0 );
}

static const float2 kTextVertices[4] = { float2( 0, 0 ), float2( 1, 0 ), float2( 1, 1 ), float2( 0, 1 ) };
static const uint kTextIndices[6] = { 0, 1, 2, 2, 3, 0 };

// Coverage regions are disjoint: composite their premultiplied contributions,
// then convert once to the straight alpha expected by the text blend state.
float4 CompositeTextOutline( float fill, float outer, float4 color, float4 outline ) {
    float fillAlpha = fill * color.a;
    float borderAlpha = saturate( outer - fill ) * outline.a;
    float alpha = fillAlpha + borderAlpha;
    return float4( alpha > 0 ? ( color.rgb * fillAlpha + outline.rgb * borderAlpha ) / alpha : float3( 0 ), alpha );
}
