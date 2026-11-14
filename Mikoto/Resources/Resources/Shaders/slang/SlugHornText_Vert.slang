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
            buffers[parameters.TextParametersID].Read<FontRenderParams>( 0, parameters.GlyphOffset + instanceID );

    float2 corner = kTextVertices[kTextIndices[vertexID]];
    float2 local = corner * p.Size.xy + p.Position.xy;
    float2 em = lerp( p.GlyphPlaneBounds.xy, p.GlyphPlaneBounds.zw,
                      float2( corner.x, p.Size.z > 0 ? corner.y : 1 - corner.y ) );
    // Extend geometry along with the stroke; screen-space Slug dilation below
    // then provides the extra antialiasing pixel, including under perspective.
    float2 strokeExpansion = ( corner * 2 - 1 ) * p.OutlineWidth;
    local += strokeExpansion * p.Size.w;
    em += strokeExpansion * float2( 1, p.Size.z );
    float4x4 mvp = mul( p.Projection, mul( p.View, p.Model ) );

    float2 dilated;
    float2 expandedEm = SlugDilate( float4( local, corner * 2 - 1 ), float4( em, 0, 0 ),
                                    float4( 1 / p.Size.w, 0, 0, p.Size.z / p.Size.w ), mvp[0], mvp[1], mvp[3],
                                    p.Viewport, dilated );

    SlugHornTextOutput o;

    o.position = mul( mvp, float4( dilated, 0, 1 ) );
    o.texcoord = expandedEm;
    o.color = p.Color;
    o.banding = p.BandTransform;
    o.glyph = int4( p.GlyphBands );
    o.textures = uint2( p.CurveTextureID, p.BandTextureID );
    o.outline = p.OutlineWidth;
    o.outlineColor = p.OutlineColor;
    o.stroke = p.SlugStroke.xyz;
    return o;
}
