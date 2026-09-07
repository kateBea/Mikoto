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

struct FSOutput {
    float4 color : SV_Target0;
};

struct FSInput {
    float2 uv : TEXCOORD0;
};

DECL_BINDING(0, 0) SamplerState sampler;
DECL_BINDING(0, 1) Texture2D[] finalImage;

struct DrawParams {
    uint mSamplerIndex;
    uint mTextureIndex;
};

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

#if defined(MKT_SUPPORT_VULKAN)
    o.color = finalImage[0].Sample(sampler, input.uv);
#elif defined(MKT_SUPPORT_D3D12)
    SamplerState sampler = SamplerDescriptorHeap[parameters.mSamplerIndex];
    Texture2D finalImage = ResourceDescriptorHeap[parameters.mTextureIndex];

    o.color = finalImage.Sample(sampler, input.uv);
#endif

    return o;
}