//    Copyright 2025 ケイト
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
#include "base/Bloom_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct TonemapParameters {
    float Exposure;
    float Gamma;

    int ToneMapType;
};

struct DrawParams {
    uint TextureSamplerID;
    uint BasicSamplerID;

    float Exposure;
    float Gamma;

    int ToneMapType;
    uint BloomImageID;
    uint BloomSamplerID;
    float BloomIntensity;
    uint BloomEnabled;
    uint VisualizeDepthOfField;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput output;

    Texture2D texture = textures[parameters.TextureSamplerID];
    SamplerState sampler = samplers[parameters.BasicSamplerID];

    float4 color = texture.Sample( sampler, input.UV );
    if ( parameters.VisualizeDepthOfField != 0 ) {
        output.Color = color;
        return output;
    }

    if ( parameters.BloomEnabled != 0 ) {
        float3 bloom =
                textures[parameters.BloomImageID].SampleLevel( samplers[parameters.BloomSamplerID], input.UV, 0 ).rgb;
        color = BloomComposite( color, bloom, parameters.BloomIntensity );
    }

    output.Color = ApplyToneMapping( color, parameters.ToneMapType, parameters.Exposure, parameters.Gamma );

    return output;
}
