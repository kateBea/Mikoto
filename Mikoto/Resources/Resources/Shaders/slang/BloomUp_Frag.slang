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

// Reconstruct bloom from the smaller level and combine it with the current level. The final
// composition can add the reconstructed glow to the original scene.

#pragma once

#include "base/Base.slang"
#include "base/Bloom_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct FSInput {
    float2 mUV : TEXCOORD0;
};

struct FSOutput {
    float4 mColor : SV_Target0;
};

struct DrawParams {
    uint mSamplerID;
    uint mLowerImageID;
    uint mDetailImageID;

    float mScatter;
    float mRadius;
};

MKT_DECLARE_SAMPLERS samplers;

MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput output;

    SamplerState sampler = samplers[parameters.mSamplerID];
    Texture2D srcTexture = textures[parameters.mLowerImageID];
    Texture2D detailTexture = textures[parameters.mDetailImageID];

    float3 color = BloomUpsample(
        srcTexture, detailTexture, sampler, input.mUV,
        parameters.mScatter, parameters.mRadius );

    output.mColor = float4( color, 1.0 );

    return output;
}
