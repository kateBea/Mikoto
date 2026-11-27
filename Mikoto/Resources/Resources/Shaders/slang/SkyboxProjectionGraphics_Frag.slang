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

// Project the source environment into a cubemap face through the graphics path. The face
// direction selects the source environment sample.

#pragma once

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct DrawParams {
    uint mSamplerID;
    uint mTextureID;
    uint mDimension;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
float4 main( float2 unusedUV : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    // Framebuffer coordinates give top-left UVs regardless of the backend's
    // clip-space Y convention. The input is already a cube face, not a panorama.
    float2 uv = position.xy / float( parameters.mDimension );
    return float4( textures[parameters.mTextureID].SampleLevel( samplers[parameters.mSamplerID], uv, 0.0 ).rgb, 1.0 );
}
