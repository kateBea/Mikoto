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

#pragma once

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

using namespace mikoto;

struct VSInput {
    uint mVertexID : SV_VertexID;
    uint mInstanceIndex : SV_InstanceID;
    uint mBaseInstanceIndex : SV_StartInstanceLocation;
};

struct VertexDescription {
    float3 mPosition : POSITION;
    float3 mNormal : NORMAL;
    float4 mColor : COLOR;

    float2 mUv0 : TEXCOORD0;
    float2 mUv1 : TEXCOORD1;

    float4 mJoints0 : BLENDINDICES0;
    float4 mWeights0 : BLENDWEIGHT0;

    float4 mJoints1 : BLENDINDICES1;
    float4 mWeights1 : BLENDWEIGHT1;
};

struct VSOutput {
    float4 mPosition : SV_POSITION;
    float2 mUv : ATTRIBUTE3;
};

struct DrawParams {
    uint mVertexID;
    uint mIndexID;
    uint mOffsetsID;

    uint mSamplerID;

    uint mTextureID;

    uint mIndexOffsetID;
    uint mVertexOffsetID;

    uint mTransformID;
};

struct GeometryTransformation {
    float4x4 mModel;
    float4x4 mView;
    float4x4 mProjection;
};

MKT_DECLARE_STRUCTURED_SRV Buffers;

MKT_DECLARE_PUSH_CONSTANT( DrawParams, Parameters );

[shader( "vertex" )]
VSOutput main( VSInput input ) {
    VSOutput o;

    ByteAddressBuffer Vertices = Buffers[Parameters.mVertexID];
    ByteAddressBuffer Indices = Buffers[Parameters.mIndexID];
    ByteAddressBuffer Offsets = Buffers[Parameters.mOffsetsID];

    uint indexOffset = Offsets.Read<uint>( Parameters.mIndexOffsetID, 0 );
    uint vertexOffset = Offsets.Read<uint>( Parameters.mVertexOffsetID, 0 );

    uint index = Indices.Read<uint>( indexOffset, input.mVertexID );
    VertexDescription vertex = Vertices.Read<VertexDescription>( vertexOffset, index );

    ByteAddressBuffer cBuff = Buffers[Parameters.mTransformID];
    GeometryTransformation tg = cBuff.Read<GeometryTransformation>( 0, 0 );

    o.mPosition = tg.mProjection * tg.mView * tg.mModel * float4( vertex.mPosition, 1.0 );
    o.mUv = vertex.mUv0;

    return o;
}