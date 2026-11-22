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


#include "base/Canvas_Helpers.slang"

// Slang 2026.17.1 can misattribute a macro immediately following an include.
// Keep these ordinary declarations first, including with unpatched SDK builds.
static const uint kCanvasSolidQuad = 0;
static const uint kCanvasGlyphQuad = 2;
static const uint kCanvasColorGlyphQuad = 3;

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

[shader( "fragment" )]
float4 main( CanvasVaryings input ) : SV_Target0 {
    CanvasQuad quad = ( ( BufferReference<CanvasQuad> )parameters.quads )[input.index];

    float4 color = quad.color;

    if ( quad.kind != kCanvasSolidQuad ) {
        float4 texel = textures[quad.textureID].Sample( samplers[parameters.samplerID], input.uv );
        if ( quad.kind == kCanvasGlyphQuad ) {
            uint width, height;
            textures[quad.textureID].GetDimensions( width, height );
            float distance = max( min( texel.r, texel.g ), min( max( texel.r, texel.g ), texel.b ) );
            float2 unitRange = quad.distanceRange / float2( width, height );
            float range = max( 0.5 * dot( unitRange, 1 / max( fwidth( input.uv ), float2( 1e-8 ) ) ), 1 );
            // A positive stroke expands the glyph in screen pixels for an outline underlay.
            color.a *= saturate( range * ( distance - 0.5 ) + 0.5 + quad.stroke );
        } else if ( quad.kind == kCanvasColorGlyphQuad ) {
            color = float4( texel.a > 0 ? texel.rgb / texel.a : float3( 0 ), color.a * texel.a );
        } else {
            color *= texel;
        }
    }

    if ( quad.kind != kCanvasGlyphQuad && quad.kind != kCanvasColorGlyphQuad ) {
        float radius = clamp( quad.rounding, 0, min( quad.size.x, quad.size.y ) * 0.5 );
        float2 q = abs( ( input.local - 0.5 ) * quad.size ) - quad.size * 0.5 + radius;
        float distance = length( max( q, float2( 0 ) ) ) + min( max( q.x, q.y ), 0 ) - radius;
        float edge = max( fwidth( distance ), 0.001 );
        color.a *= 1 - smoothstep( -edge, 0, distance );
        if ( quad.stroke > 0 ) {
            color.a *= smoothstep( -quad.stroke - edge, -quad.stroke, distance );
        }
    }
    return color;
}
