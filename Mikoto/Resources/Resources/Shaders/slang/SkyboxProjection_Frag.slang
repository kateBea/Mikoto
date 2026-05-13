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

struct FSInput {
    float3 Pos : TEXCOORD0;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

struct DrawParams {
    float4x4 Mvp;
    uint BasicSamplerID;
    uint EquirectangularID;
};

MKT_DECLARE_SAMPLERS Samplers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) Textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) Parameters;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    SamplerState sampler = Samplers[Parameters.BasicSamplerID];
    Texture2D texture = Textures[Parameters.EquirectangularID];

    float3 dir = normalize(input.Pos);

    float2 uv = SampleSphericalMap(dir);
    float3 color = texture.Sample(sampler, uv).rgb;

    o.Color = float4(color, 1.0);
    return o;
}