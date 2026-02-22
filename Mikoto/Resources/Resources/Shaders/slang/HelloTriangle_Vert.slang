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

static float2 positions[3] = float2[](
    float2(-0.6, -0.5),
    float2( 0.6, -0.5),
    float2( 0.0,  0.6)
);

// per-vertex colors
static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);

struct VSOutput {
    float4 sv_position : SV_Position;
    float3 color       : COLOR0;
};

[shader("vertex")]
VSOutput vertMain(uint vid : SV_VertexID) {
    VSOutput result;
    // look up the position and color by index
    result.sv_position = float4(positions[vid], 0.0, 1.0);
    result.color = colors[vid];
    return result;
}