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

// Text upload data, MSDF distance reconstruction and fill/outline composition. Derivative-
// scaled distance converts font-space edge widths into screen-pixel coverage.

#pragma once

struct FontRenderParams {
    float4x4 mProjection;
    float4x4 mView;
    float4x4 mModel;
    float4 mPosition;
    float4 mSize;
    float4 mColor;
    float2 mTextureCoords[kQuadCornerCount];
    uint mTextureIndex;
    float mDistanceRange;
    uint mImageType;
    float mOutlineWidth;
    float4 mGlyphPlaneBounds;
    float4 mBandTransform;
    uint4 mGlyphBands;
    float2 mViewport;
    uint mCurveTextureID;
    uint mBandTextureID;
    float4 mOutlineColor;
    uint4 mSlugStroke;
};

struct TextDrawParams {
    uint mTextSamplerID;
    uint mTextParametersID;
    uint mGlyphOffset;
};

struct TextVertexOutput {
    float4 mPositionH : SV_Position;
    float2 mTexCoord : TEXCOORD0;
    nointerpolation float4 mColor : TEXCOORD1;
    nointerpolation uint mTextureIndex : TEXCOORD2;
    nointerpolation float mDistanceRange : TEXCOORD3;
    nointerpolation uint mImageType : TEXCOORD4;
    nointerpolation float mOutlineWidth : TEXCOORD5;
    nointerpolation float4 mOutlineColor : TEXCOORD6;
};

float Median( float3 v ) {
    return max( min( v.r, v.g ), min( max( v.r, v.g ), v.b ) );
}

// Convert the MSDF atlas range into the current pixel footprint using UV derivatives. This
// keeps edge smoothing approximately one screen pixel across text scale changes.
float ScreenPxRange( Texture2D atlas, float2 uv, float pixelRange ) {
    uint width;
    uint height;
    atlas.GetDimensions( width, height );
    float2 unitRange = pixelRange / float2( width, height );
    float2 screenTexSize = 1.0 / max( fwidth( uv ), float2( 1e-7 ) );
    return max( 0.5 * dot( unitRange, screenTexSize ), 1.0 );
}

static const float2 kTextVertices[kQuadCornerCount] = { float2( 0, 0 ), float2( 1, 0 ), float2( 1, 1 ), float2( 0, 1 ) };
static const uint kTextIndices[kVerticesPerQuad] = { 0, 1, 2, 2, 3, 0 };

// Coverage regions are disjoint: composite their premultiplied contributions,
// then convert once to the straight alpha expected by the text blend state.
// Combine fill and outer-outline coverage without counting the same glyph interior twice. The
// returned color follows the text pipeline blending convention.
float4 CompositeTextOutline( float fill, float outer, float4 color, float4 outline ) {
    float fillAlpha = fill * color.a;
    float borderAlpha = saturate( outer - fill ) * outline.a;
    float alpha = fillAlpha + borderAlpha;
    return float4( alpha > 0 ? ( color.rgb * fillAlpha + outline.rgb * borderAlpha ) / alpha : float3( 0 ), alpha );
}
