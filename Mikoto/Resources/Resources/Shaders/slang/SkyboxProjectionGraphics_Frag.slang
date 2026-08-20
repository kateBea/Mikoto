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
#include "base/BoxModel.slang"
#include "base/Memory_Helpers.slang"
#include "base/Lighting_Helpers.slang"

struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    float2 UV : TEXCOORD0;
};

struct DrawParams {
    float4x4 Mvp;
    uint mBasicSamplerID;
    uint mEquirectangularID;
};

MKT_DECLARE_SAMPLERS Samplers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) Textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) Parameters;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    SamplerState sampler = Samplers[Parameters.mBasicSamplerID];
    Texture2D texture = Textures[Parameters.mEquirectangularID];

    float2 uv = input.UV;
    float3 color = texture.Sample(sampler, uv).rgb;

    o.Color = float4(color, 1.0);
    return o;
}