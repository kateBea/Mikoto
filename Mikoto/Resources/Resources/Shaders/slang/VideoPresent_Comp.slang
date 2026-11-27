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

// Separate plane views use Mikoto's ordinary bindless textures and sampler states.
// https://docs.vulkan.org/spec/latest/chapters/formats.html#formats-compatible-planes

// YCbCr coefficients/range and inverse transfer function:
// https://www.itu.int/rec/R-REC-BT.709/en
// https://www.itu.int/rec/R-REC-BT.601/en
// https://www.itu.int/rec/R-REC-BT.2020/en

// Convert the uploaded video frame into the presentation texture. Frame layout, transfer-
// function conversion and image orientation are selected by the uploaded video parameters.

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) outputs;

struct Parameters {
    uint mWidth;
    uint mHeight;
    uint mLuma;
    uint mChroma;
    uint mOutput;
    uint mSampler;
    uint mTransfer;
    uint mMatrix;
    uint mFullRange;

    uint3 mPadding;

    float4 mUvRect;
    float2 mChromaOffset;
    float2 mPadding2;
};

MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

static float LinearizeVideo(float value) {
    value = max(value, 0.0);

    if (parameters.mTransfer == 13) {
        return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
    }

    return value < 0.081 ? value / 4.5 : pow((value + 0.099) / 1.099, 1.0 / 0.45);
}

[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main(uint3 pixel : SV_DispatchThreadID) {
    if (pixel.x >= parameters.mWidth || pixel.y >= parameters.mHeight) {
        return;
    }

    const float2 uv = ((float2(pixel.xy) + 0.5) / 
        float2(parameters.mWidth, parameters.mHeight)) * 
        parameters.mUvRect.xy + parameters.mUvRect.zw;

    SamplerState sampleState = samplers[parameters.mSampler];

    float y = textures[parameters.mLuma].SampleLevel(sampleState, uv, 0.0).r;
    float2 cbcr = textures[parameters.mChroma].SampleLevel(sampleState, uv + parameters.mChromaOffset, 0.0).rg;

    y = parameters.mFullRange != 0 ? y : (y * 255.0 - 16.0) / 219.0;
    cbcr = (cbcr * 255.0 - 128.0) / (parameters.mFullRange != 0 ? 255.0 : 224.0);

    const float kr = parameters.mMatrix == 9 ? 0.2627 : (parameters.mMatrix == 5 || parameters.mMatrix == 6 ? 0.299 : 0.2126);
    const float kb = parameters.mMatrix == 9 ? 0.0593 : (parameters.mMatrix == 5 || parameters.mMatrix == 6 ? 0.114 : 0.0722);

    const float r = y + 2.0 * (1.0 - kr) * cbcr.y;
    const float b = y + 2.0 * (1.0 - kb) * cbcr.x;
    const float g = (y - kr * r - kb * b) / (1.0 - kr - kb);
    
    outputs[parameters.mOutput][pixel.xy] = float4(LinearizeVideo(r), LinearizeVideo(g), LinearizeVideo(b), 1.0);
}
