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
#include "base/FontRender_Helpers.slang"

// NOTE (Migration from GLSL): https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/a2-01-spirv-target-specific.html

/*
    Note that SV_DrawIndex, SV_FragInvocationCount, SV_FragSize, SV_PointSize, SV_PointCoord and SV_VulkanSamplePosition are
    Slang-specific semantics that are not defined in HLSL. Also Note that SV_InstanceID/SV_VertexID counts all
    instances/vertices in a draw call, unlike how InstanceIndex/VertexIndex is relative to BaseInstance/BaseVertex.
    See Using SV_InstanceID/SV_VertexID with SPIR-V target

    Using SV_InstanceID and SV_VertexID with SPIR-V target
    When using SV_InstanceID and SV_VertexID with SPIR-V target, it is 
    equivalent to the difference between the index and base builtins. This matches 
    the behavior of D3D where SV_InstanceID and SV_VertexID starts from zero for each 
    draw call, while in SPIR-V, InstanceIndex and VertexIndex includes the base instance.

    If you need direct access to InstanceIndex and VertexIndex values, use SV_VulkanInstanceID 
    and SV_VulkanVertexID semantic names. These are supported for all targets except HLSL.
     Alternatively you can use parameters with SV_InstanceID(or SV_VertexID) and 
     SV_StartInstanceLocation(or SV_StartVertexLocation) semantics:
*/

// At the moment it is valid to access glyph data via instance ID
// because we submit one draw call that has all the instances IDs

struct VSInput {
    uint VertexID : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
};

struct VSOutput {
    float2 TexCoord : TEXCOORD0;
    uint AtlasIndex : TEXCOORD1;
    float4 Color : TEXCOORD2;

    float4 PositionH : SV_Position;
};

struct DrawParams {
    uint TextSamplerID;
    uint TextParametersID;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT(DrawParams) Parameters;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    uint vertexIndex = kTextIndices[input.VertexID];
    TextVertexDescription vertex = kTextVertices[vertexIndex];

    ByteAddressBuffer textBuffer = buffers[Parameters.TextParametersID];
    FontRenderParams p = textBuffer.Read<FontRenderParams>(0, input.InstanceIndex);

    o.TexCoord = p.TextureCoords[vertex.mTextureIndex];
    o.AtlasIndex = p.TextureIndex;
    o.Color = p.Color;

    float3 pos = vertex.mPosition * p.Size.xyz + p.Position.xyz;

    o.PositionH =
        mul(p.Projection,
        mul(p.View,
        mul(p.Model, float4(pos, 1.0))));

    return o;
}