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

// Expand uploaded analytic-text glyph bounds into renderable quads. Curve/band addresses and
// text styling are forwarded for per-pixel coverage evaluation.

#pragma once

#include "base/Base.slang"
#include "base/FontRender_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/SlugReference.slang"
#include "base/SlugHornText_Helpers.slang"

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT( TextDrawParams, parameters );

[shader( "vertex" )]
SlugHornTextOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    FontRenderParams p =
            buffers[parameters.mTextParametersID].Read<FontRenderParams>( 0, parameters.mGlyphOffset + instanceID );

    float2 corner = kTextVertices[kTextIndices[vertexID]];
    float2 local = corner * p.mSize.xy + p.mPosition.xy;
    float2 em = lerp( p.mGlyphPlaneBounds.xy, p.mGlyphPlaneBounds.zw,
                      float2( corner.x, p.mSize.z > 0 ? corner.y : 1 - corner.y ) );
    // Extend geometry along with the stroke; screen-space Slug dilation below
    // then provides the extra antialiasing pixel, including under perspective.
    float2 strokeExpansion = ( corner * 2 - 1 ) * p.mOutlineWidth;
    local += strokeExpansion * p.mSize.w;
    em += strokeExpansion * float2( 1, p.mSize.z );
    float4x4 mvp = mul( p.mProjection, mul( p.mView, p.mModel ) );

    float2 dilated;
    float2 expandedEm = SlugDilate( float4( local, corner * 2 - 1 ), float4( em, 0, 0 ),
                                    float4( 1 / p.mSize.w, 0, 0, p.mSize.z / p.mSize.w ), mvp[0], mvp[1], mvp[3],
                                    p.mViewport, dilated );

    SlugHornTextOutput o;

    o.mPosition = mul( mvp, float4( dilated, 0, 1 ) );
    o.mTexcoord = expandedEm;
    o.mColor = p.mColor;
    o.mBanding = p.mBandTransform;
    o.mGlyph = int4( p.mGlyphBands );
    o.mTextures = uint2( p.mCurveTextureID, p.mBandTextureID );
    o.mOutline = p.mOutlineWidth;
    o.mOutlineColor = p.mOutlineColor;
    o.mStroke = p.mSlugStroke.xyz;
    return o;
}
