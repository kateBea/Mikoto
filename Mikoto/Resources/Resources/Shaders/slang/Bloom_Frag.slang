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
struct FSOutput {
    float4 Color : SV_Target0;
};

struct FSInput {
    float2 UV : TEXCOORD0;
};

// Bloom
[[vk::binding(Slot_0, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_Bloom;
[[vk::binding(Slot_1, STATIC_SAMPLERS_SET_INDEX)]] Sampler2D u_SceneColor;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float3 scene = u_SceneColor.Sample(input.UV).rgb;
    float3 bloom = u_Bloom.Sample(input.UV).rgb;

    float bloomIntensity = 2.0f;

    float3 color = scene + bloom * bloomIntensity;

    o.Color = float4(color, 1.0f);
    return o;
}