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
#include "base/Memory_Helpers.slang"
#include "base/FontRender_Helpers.slang"
#include "base/SlugReference.slang"
#include "base/SlugHornText_Helpers.slang"
#include "base/SlugStroke.slang"
#include "base/SlugHorn.slang"
#include "base/SlugHornStroke.slang"

// Both typed views use the existing bindless sampled-image binding.
MKT_DECLARE_TEXTURE_SRV( Texture2D ) curveTextures;
MKT_DECLARE_TEXTURE_SRV( Texture2D<uint4> ) bandTextures;
MKT_DECLARE_PUSH_CONSTANT( TextDrawParams, parameters );

[shader( "fragment" )]
float4 main( SlugHornTextOutput input ) : SV_Target0 {
    float coverage = SlugHornRender( curveTextures[NonUniformResourceIndex( input.textures.x )],
                                 bandTextures[NonUniformResourceIndex( input.textures.y )], input.texcoord,
                                 input.banding, input.glyph.xy, input.glyph.zw );

    if ( input.outline > 0 ) {
        float outer = max( coverage, SlugHornStrokeCoverage(
            curveTextures[NonUniformResourceIndex( input.stroke.x )],
            input.stroke.y, input.stroke.z, input.texcoord, input.outline ) );
        return CompositeTextOutline( coverage, outer, input.color, input.outlineColor );
    }
    // Mikoto's blend state expects straight alpha. The reference entry point
    // premultiplies RGB; retain the official coverage calculation, adapt output.
    return float4( input.color.rgb, input.color.a * coverage );
}
