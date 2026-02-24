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

struct VSOutput {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
    float3 color : COLOR0;
};

static const float2 positions[4] = {
    float2(-0.5, -0.5),
    float2(0.5, -0.5),
    float2(-0.5, 0.5),
    float2(0.5, 0.5)
};

static const float2 texCoords[4] ={
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 1.0)
};

static const float3 colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

[shader("vertex")]
VSOutput main(uint vid: SV_VertexID) {
    VSOutput o;
    o.position = float4(positions[vid], 0.0, 1.0);
    o.texCoord = texCoords[vid];
    o.color = colors[vid];

    return o;
}