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

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT( TextDrawParams, parameters );

[shader( "vertex" )]
TextVertexOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    TextVertexOutput o;
    uint vertex = kTextIndices[vertexID];

    FontRenderParams p = buffers[parameters.TextParametersID]
        .Read<FontRenderParams>( 0, parameters.GlyphOffset + instanceID );

    o.TexCoord = p.TextureCoords[vertex];
    o.Color = p.Color;
    o.TextureIndex = p.TextureIndex;
    o.DistanceRange = p.DistanceRange;
    o.ImageType = p.ImageType;
    o.OutlineWidth = p.OutlineWidth;
    o.OutlineColor = p.OutlineColor;

    float3 local = float3( kTextVertices[vertex] * p.Size.xy + p.Position.xy, p.Position.z );

    o.PositionH = mul( p.Projection, mul( p.View, mul( p.Model, float4( local, 1 ) ) ) );

    return o;
}
