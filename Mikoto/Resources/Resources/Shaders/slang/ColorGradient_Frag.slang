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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct DrawParams {
    uint mSamplerID;
    uint mFinalImageID;

    uint mCameraBufferID;

    float mExposure;
    float mContrast;
    float mSaturation;
    float4 mTint;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_STRUCTURED_SRV buffers;

MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

float3 ApplySaturation(float3 color, float saturation) {
    float luminance = dot(color, float3(0.299, 0.587, 0.114));
    return lerp(float3(luminance, luminance, luminance), color, saturation);
}

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    CameraInfo camera = buffers[parameters.mCameraBufferID].Read<CameraInfo>(0, 0);

    SamplerState sampler = samplers[parameters.mSamplerID];
    Texture2D finalImage = textures[parameters.mFinalImageID];

    float3 color = finalImage.Sample(sampler, input.UV).rgb;

    // Exposure
    color *= parameters.mExposure;

    // Contrast (centered around 0.5)
    color = (color - 0.5) * parameters.mContrast + 0.5;

    // Saturation
    color = ApplySaturation(color, parameters.mSaturation);

    // Tint
    color *= parameters.mTint.xyz;

    o.Color = float4(color, 1.0);
    return o;
}