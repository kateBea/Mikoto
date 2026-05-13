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

using namespace mikoto;

struct VSInput {
    uint VertexID : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VertexDescription {
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
    float2 uv    : ATTRIBUTE3;
};

struct DrawParams {
    uint VertexID;
    uint IndexID;
    uint OffsetsID;

    uint SamplerID;

    uint TextureID;

    uint IndexOffsetID;
    uint VertexOffsetID;

    uint TransformID;
};

struct GeometryTransformation {
    float4x4 Model;
    float4x4 View;
    float4x4 Projection;
};

MKT_DECLARE_STRUCTURED_SRV Buffers;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) Parameters;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    ByteAddressBuffer Vertices = Buffers[Parameters.VertexID];
    ByteAddressBuffer Indices = Buffers[Parameters.IndexID];
    ByteAddressBuffer Offsets = Buffers[Parameters.OffsetsID];

    uint indexOffset = Offsets.Read<uint>(Parameters.IndexOffsetID, 0);
    uint vertexOffset = Offsets.Read<uint>(Parameters.VertexOffsetID, 0);

    uint index = Indices.Read<uint>(indexOffset, input.VertexID);
    VertexDescription vertex = Vertices.Read<VertexDescription>(vertexOffset, index);

    ByteAddressBuffer cBuff = Buffers[Parameters.TransformID];
    GeometryTransformation tg = cBuff.Read<GeometryTransformation>(0, 0);

    o.position = tg.Projection * tg.View * tg.Model * float4(vertex.position, 1.0);
    o.uv = vertex.uv0;

    return o;
}