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

#include "base/Memory_Helpers.slang"

struct DrawParams {
    uint Outline;
    uint IDs;
    uint Hover;
    uint ShowOutline;
};

MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_TEXTURE_SRV( Texture2D<uint> ) ids;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
float4 main( float2 unusedUV : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    int3 pixel = int3(int2(position.xy),0);
    float4 outline = parameters.ShowOutline != 0 ? textures[parameters.Outline].Load(pixel) : float4(0);
    if ( parameters.Hover != 0 && ids[parameters.IDs].Load(pixel) == parameters.Hover ) {
        float alpha = 0.12;
        outline.rgb = float3(1,0.95,0.25)*alpha + outline.rgb*(1-alpha);
        outline.a = alpha + outline.a*(1-alpha);
    }
    if ( outline.a <= 0.0f ) {
        discard;
    }

    // The RHI uses source-alpha blending. Recover straight color after the
    // premultiplied MSAA resolve so coverage is applied exactly once.
    return float4( outline.rgb / outline.a, outline.a );
}
