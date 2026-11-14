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

#include "base/Animation_Helpers.slang"
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct VSInput {
    uint VertexID : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    nointerpolation uint MaterialIndex : TEXCOORD0;
    nointerpolation uint FaceFlags : TEXCOORD4;
    float2 Uv0 : TEXCOORD1;
    float2 Uv1 : TEXCOORD2;
    float VertexAlpha : TEXCOORD3;
    float4 Position : SV_Position;
};

struct DrawParams {
    Pointer mGeometryPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryDescPtr;

    Pointer mCameraBda;
    Pointer mMaterialsPtr;
    uint mBasicSamplerID;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "vertex" )]
VSOutput main( VSInput input ) {
    VSOutput o;

    // Highlight filtering preserves the original packed instance IDs.
    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;

    // Apply skinning
    MeshGeometryInfo meshInfo = LoadMeshGeometry( parameters.mGeometryPtr, instanceIndex );
    GeometryAllocation allocation = GetGeometryAllocation( meshInfo, parameters.mGeometryDescPtr );
    uint vertexIndex = PullVertexIndex( allocation, input.VertexID );
    VertexDescription vertex = PullDepthVertex( meshInfo, allocation, vertexIndex );

    // Compute world position
    float4 worldPos = ApplySkinningPosition( vertex, meshInfo, parameters.mSkinningPtr );

    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCameraBda;

    // Final position
    o.Position = CameraClipPosition( worldPos, camera[0] );

    o.MaterialIndex = meshInfo.MaterialIndex;
    o.FaceFlags = meshInfo.FaceFlags;
    o.Uv0 = vertex.uv0;
    o.Uv1 = vertex.uv1;
    o.VertexAlpha = vertex.color.a;

    return o;
}
