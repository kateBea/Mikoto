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

[[vk::binding(Slot_0, DYNAMIC_SAMPLERS_SET_INDEX)]] 
Sampler2D u_FinalComposition;

[[vk::push_constant]] 
TonemapParameters u_Constants;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput output;

    float4 color = u_FinalComposition.Sample(input.UV);

    switch (u_Constants.ToneMapType) {
    case c_ToneMapLinear:
        output.Color = Tonemap_Linear(color, u_Constants.Exposure, u_Constants.Gamma);
        break;
    case c_ToneMapReinhard:
        output.Color = float4(Tonemap_Reinhard(color.xyz), color.a);
        break;
    case c_ToneMapUncharted2:
        output.Color = Tonemap_Uncharted2(color, u_Constants.Exposure, u_Constants.Gamma);
        break;
    case c_ToneMapACES:
        output.Color = Tonemap_Aces(color, u_Constants.Exposure, u_Constants.Gamma);
        break;
    case c_ToneMapKhronosNeutral:
        output.Color = Tonemap_KhronosNeutral(color, u_Constants.Exposure, u_Constants.Gamma);
        break;
    default:
        output.Color = Tonemap_Aces(color, u_Constants.Exposure, u_Constants.Gamma);
        break;
    }

    return output;
}