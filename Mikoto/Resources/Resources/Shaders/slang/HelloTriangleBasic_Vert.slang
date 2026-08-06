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

struct VSInput {
    float3 position   : POSITION;
    float3 normal     : NORMAL;
    float4 color      : COLOR;

    float2 uv0        : TEXCOORD0;
    float2 uv1        : TEXCOORD1;

    float4 joints0    : BLENDINDICES0;
    float4 weights0   : BLENDWEIGHT0;

    float4 joints1    : BLENDINDICES1;
    float4 weights1   : BLENDWEIGHT1;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 uv    : TEXCOORD;
};

struct MyData {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

DECL_BINDING(2, 0) ConstantBuffer<MyData> parameters;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    o.position =
        parameters.projection *
        parameters.view *
        parameters.model *
        float4(input.position, 1.0);

    o.uv = input.uv0;

    return o;
}