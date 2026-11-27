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

// Pull the environment cube geometry and project the requested capture view. The interpolated
// local direction identifies the cubemap sample evaluated by the fragment stage.

#pragma once

#include "base/Base.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct SkyboxRenderConstants {
    float4x4 mMVP;
};

struct VSInput {
    uint mVID : SV_VertexID;
};

struct VSOutput {
    float3 mPos : TEXCOORD0;
    float4 mPositionH : SV_Position;
};

struct DrawParams {
    PushConstantMatrix4x4 mMvp;
    uint mBasicSamplerID;
    uint mEquirectangularID;

    uint mVertexBufferID;
    uint mIndexBufferID;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "vertex" )]
VSOutput main( VSInput input ) {
    VSOutput o;

    ByteAddressBuffer vertexBuffer = buffers[parameters.mVertexBufferID];
    ByteAddressBuffer indexBuffer = buffers[parameters.mIndexBufferID];

    uint index = indexBuffer.Read<uint>( 0, input.mVID );
    VertexDescription vertex = vertexBuffer.Read<VertexDescription>( 0, index );

    o.mPos = vertex.mPosition;

    o.mPositionH = mul( parameters.mMvp.Load(), float4( vertex.mPosition, 1.0 ) );

    return o;
}
