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
#include "base/Memory_Helpers.slang"
#include "base/Material_Helpers.slang"

struct DrawParams {
    float4x4 MVP;
    float Roughness;
    uint NumSamples;

    uint mCubeID;
    uint mSamplerID;

    uint mVertexBufferID;
    uint mIndexBufferID;
};

struct VSOutput {
    float3 Pos : TEXCOORD0;
    float4 PositionH : SV_Position;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID) {
    VSOutput o;

    ByteAddressBuffer vertexBuffer = buffers[parameters.mVertexBufferID];
    ByteAddressBuffer indexBuffer = buffers[parameters.mIndexBufferID];

    uint index = indexBuffer.Read<uint>(0, vertexID);
    VertexDescription vertex = vertexBuffer.Read<VertexDescription>(0, index);

    o.Pos = vertex.position;

    o.PositionH = mul(parameters.MVP, float4(vertex.position, 1.0));

    // Rotate by the Y axis for Vulkan otherwise cube appears upside down
    // Vulkan uses a different coordinate system than OpenGL, Y is flipped
    // https://www.sctheblog.com/blog/vulkan-frame/
    // https://johannesugb.github.io/gpu-programming/setting-up-a-proper-vulkan-projection-matrix/
#if defined(MKT_SUPPORT_VULKAN)
    o.PositionH.y *= -1.0;
#endif
    
    return o;
}