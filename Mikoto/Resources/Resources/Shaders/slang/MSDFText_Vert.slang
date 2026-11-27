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

// Expand uploaded glyph records into quads in world or screen space. Atlas coordinates and text
// styling data are passed to the MSDF fragment stage.

#pragma once

#include "base/Base.slang"
#include "base/FontRender_Helpers.slang"
#include "base/Memory_Helpers.slang"

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT( TextDrawParams, parameters );

[shader( "vertex" )]
TextVertexOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    TextVertexOutput o;
    uint vertex = kTextIndices[vertexID];

    FontRenderParams p = buffers[parameters.mTextParametersID]
        .Read<FontRenderParams>( 0, parameters.mGlyphOffset + instanceID );

    o.mTexCoord = p.mTextureCoords[vertex];
    o.mColor = p.mColor;
    o.mTextureIndex = p.mTextureIndex;
    o.mDistanceRange = p.mDistanceRange;
    o.mImageType = p.mImageType;
    o.mOutlineWidth = p.mOutlineWidth;
    o.mOutlineColor = p.mOutlineColor;

    float3 local = float3( kTextVertices[vertex] * p.mSize.xy + p.mPosition.xy, p.mPosition.z );

    o.mPositionH = mul( p.mProjection, mul( p.mView, mul( p.mModel, float4( local, 1 ) ) ) );

    return o;
}
