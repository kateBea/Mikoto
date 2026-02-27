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

struct PrefilterConstants {
    float4x4 MVP;
    float Roughness;
    uint NumSamples;
};

struct VSInput {
    float3 Position : ATTRIBUTE0;
};

struct VSOutput {
    float3 Pos : TEXCOORD0;
    float4 PositionH : SV_Position;
};

[[vk::push_constant]]
PrefilterConstants u_Parameters;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;
    o.Pos = input.Position;
    o.PositionH = mul(u_Parameters.MVP, float4(input.Position, 1.0));
    return o;
}