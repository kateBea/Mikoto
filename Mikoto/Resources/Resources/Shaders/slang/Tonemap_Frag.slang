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

// Map HDR scene radiance into the display range with the selected tone mapper. Exposure, gamma
// and optional bloom composition are applied according to the pass settings.

#pragma once

#include "base/Base.slang"
#include "base/Bloom_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

struct FSOutput {
    float4 mColor : SV_Target0;
};

struct FSInput {
    float2 mUV : TEXCOORD0;
};

struct TonemapParameters {
    float mExposure;
    float mGamma;

    int mToneMapType;
};

struct DrawParams {
    uint mTextureSamplerID;
    uint mBasicSamplerID;

    float mExposure;
    float mGamma;

    int mToneMapType;
    uint mBloomImageID;
    uint mBloomSamplerID;
    float mBloomIntensity;
    uint mBloomEnabled;
    uint mVisualizeDepthOfField;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput output;

    Texture2D texture = textures[parameters.mTextureSamplerID];
    SamplerState sampler = samplers[parameters.mBasicSamplerID];

    float4 color = texture.Sample( sampler, input.mUV );
    if ( parameters.mVisualizeDepthOfField != 0 ) {
        output.mColor = color;
        return output;
    }

    if ( parameters.mBloomEnabled != 0 ) {
        float3 bloom =
                textures[parameters.mBloomImageID].SampleLevel( samplers[parameters.mBloomSamplerID], input.mUV, 0 ).rgb;
        color = BloomComposite( color, bloom, parameters.mBloomIntensity );
    }

    output.mColor = ApplyToneMapping( color, parameters.mToneMapType, parameters.mExposure, parameters.mGamma );

    return output;
}
