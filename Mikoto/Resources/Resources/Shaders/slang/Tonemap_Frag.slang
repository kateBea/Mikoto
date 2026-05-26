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
};

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput output;

    Texture2D texture = textures[parameters.TextureSamplerID];
    SamplerState sampler = samplers[parameters.BasicSamplerID];

    float4 color = texture.Sample(sampler, input.UV);

    switch (parameters.ToneMapType) {
    case c_ToneMapLinear:
        output.Color = Tonemap_Linear(color, parameters.Exposure, parameters.Gamma);
        break;
    case c_ToneMapReinhard:
        output.Color = float4(Tonemap_Reinhard(color.xyz), color.a);
        break;
    case c_ToneMapUncharted2:
        output.Color = Tonemap_Uncharted2(color, parameters.Exposure, parameters.Gamma);
        break;
    case c_ToneMapACES:
        output.Color = Tonemap_Aces(color, parameters.Exposure, parameters.Gamma);
        break;
    case c_ToneMapKhronosNeutral:
        output.Color = Tonemap_KhronosNeutral(color, parameters.Exposure, parameters.Gamma);
        break;
    default:
        output.Color = Tonemap_Aces(color, parameters.Exposure, parameters.Gamma);
        break;
    }

    return output;
}