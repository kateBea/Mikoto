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
    uint mTonemapImageID;

    uint mCameraBufferID;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_STRUCTURED_SRV buffers;

MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

// Credits:
// Created by moonlightoctopus in 2025-05-25
// https://www.shadertoy.com/view/tcKGWw

float4 mainImage(float2 uv) {
    float distortionAmount = 3.0;

    CameraInfo camera = buffers[parameters.mCameraBufferID].Read<CameraInfo>(0, 0);

    SamplerState sampler = samplers[parameters.mSamplerID];
    Texture2D tonemappedImage = textures[parameters.mTonemapImageID];

    float2 resolution = camera.mScreenDimensions.xy;

    float2 pixelSize = 1.0 / resolution;

    float3 col1 = tonemappedImage.Sample(sampler, uv).xyz;
    float3 col2 = tonemappedImage.Sample(sampler, uv - pixelSize * distortionAmount).xyz;
    float3 col3 = tonemappedImage.Sample(sampler, uv - pixelSize * 2.0 * distortionAmount).xyz;

    float3 col = float3(col1.x, col2.y, col3.z);
    return float4(col, 1.0);
}

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    o.Color = mainImage(input.UV);
    return o;
}