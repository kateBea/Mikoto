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

struct DrawParams {
    float4x4 MVP;
    float Roughness;
    uint NumSamples;

    uint mCubeID;
    uint mSamplerID;
};

struct VSOutput {
    float3 Pos : TEXCOORD0;
    float4 PositionH : SV_Position;
};

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput o;
    o.Pos = boxPositions[vertexID];
    o.PositionH = mul(parameters.MVP, float4(boxPositions[vertexID], 1.0));
#ifdef MKT_SUPPORT_VULKAN
    o.PositionH.y = -o.PositionH.y;
#endif
    return o;
}