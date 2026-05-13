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

struct FSInput {
    float4 position : SV_POSITION;
    float2 uv    : TEXCOORD;
};

DECL_BINDING(0, 0) SamplerState sampler;
DECL_BINDING(1, 0) Texture2D texture;

DECL_BINDING(0, 1) Texture2D textures[];
DECL_BINDING(1, 1) SamplerState samplers[];

struct MyData {
    float4x4 model;
    float4x4 view;
    float4x4 projection;

    int textureIndex;
    int samplerIndex;
};

DECL_BINDING(2, 0) ConstantBuffer<MyData> Matrices;

[shader("fragment")]
float4 main(FSInput input) : SV_TARGET {
    return float4(textures[Matrices.textureIndex].Sample(samplers[Matrices.samplerIndex], input.uv).xyz * texture.Sample(sampler, input.uv).xyz, 1.0);
}