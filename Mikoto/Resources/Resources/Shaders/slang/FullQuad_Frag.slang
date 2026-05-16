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

struct DrawParams {
    uint TextureSamplerID;
    uint BasicSamplerID;
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    Texture2D texture = textures[parameters.TextureSamplerID];
    SamplerState sampler = samplers[parameters.BasicSamplerID];

    o.color = texture.Sample(sampler, input.uv);

    return o;
}