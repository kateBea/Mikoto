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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct DrawParams {
    uint mSceneImage;
    uint mMaskImage;
    uint mSampler;
    float mStrength;
    float3 mColor;
    float mPadding;
};

MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

float3 ApplyVignette( float3 sceneColor, float3 vignetteColor, float mask, float strength ) {
    return lerp( sceneColor, vignetteColor, saturate( mask * strength ) );
}

[shader( "fragment" )]
float4 main( FSInput input ) : SV_Target0 {
    SamplerState sampler = samplers[parameters.mSampler];

    float4 sceneColor = textures[parameters.mSceneImage].SampleLevel( sampler, input.UV, 0 );
    float mask = textures[parameters.mMaskImage].SampleLevel( sampler, input.UV, 0 ).r;

    return float4( ApplyVignette( sceneColor.rgb, parameters.mColor, mask, parameters.mStrength ), sceneColor.a );
}
