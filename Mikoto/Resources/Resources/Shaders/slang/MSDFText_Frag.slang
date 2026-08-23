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
#include "base/FontRender_Helpers.slang"

struct FSInput {
    float2 TexCoord : TEXCOORD0;
    uint InstanceIndex : TEXCOORD1;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

struct DrawParams {
    uint mTextSamplerID;
    uint mTextParametersID;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float threshold = 0.5;

    FontRenderParams fontParams = buffers[parameters.mTextParametersID]
        .Read<FontRenderParams>(0, input.InstanceIndex);

    Texture2D msdfTexture = textures[fontParams.TextureIndex];
    SamplerState textSampler = samplers[parameters.mTextSamplerID];

    float3 msdf = msdfTexture.Sample(textSampler, input.TexCoord).rgb;
    float sd = Median(msdf);

    float pxRange = ScreenPxRange3D(msdfTexture, textSampler, input.TexCoord);
    float screenPxDist = pxRange * (sd - threshold);

    float opacity = clamp(screenPxDist + threshold, 0.0, 1.0);

    float4 bg = float4(0.0);
    o.Color = lerp(bg, fontParams.Color, opacity);

    return o;
}