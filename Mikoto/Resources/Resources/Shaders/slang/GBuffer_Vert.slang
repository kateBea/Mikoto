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
    float3 ViewPos : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 TexCoord0 : TEXCOORD3;
    float2 TexCoord1 : TEXCOORD4;
    float4 Color : TEXCOORD5;

    nointerpolation uint MaterialIndex : TEXTCOORD6;
    nointerpolation uint FaceFlags : TEXCOORD7;

    float4 Position : SV_Position;
};

struct DrawParams {
    Pointer mGeometryPtr;
    Pointer mMaterialsPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryDescPtr;
    Pointer mCameraBda;

    uint mBasicSamplerID;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

[shader( "vertex" )]
VSOutput main( VSInput input ) {
    VSOutput o;

    // Prepare vertex data
    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;

    // Apply skinning
    MeshGeometryInfo meshInfo = LoadMeshGeometry( parameters.mGeometryPtr, instanceIndex );
    GeometryAllocation allocation = GetGeometryAllocation( meshInfo, parameters.mGeometryDescPtr );
    uint vertexIndex = PullVertexIndex( allocation, input.VertexID );
    VertexDescription vertex = PullShadingVertex( meshInfo, allocation, vertexIndex );

    // Compute world position
    SkinningResult skinningResult = ApplySkinning( vertex, meshInfo, parameters.mSkinningPtr );

    float4 worldPos = skinningResult.mPosition;
    float3 worldNormal = skinningResult.mNormal;

    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCameraBda;

    o.TexCoord0 = vertex.uv0;
    o.TexCoord1 = vertex.uv1;
    o.Color = vertex.color;

    o.WorldPos = worldPos.xyz;
    o.ViewPos = mul( camera[0].mViewMatrix, worldPos ).xyz;
    o.Normal = worldNormal;

    o.MaterialIndex = meshInfo.MaterialIndex;
    o.FaceFlags = meshInfo.FaceFlags;

    o.Position = CameraClipPosition( worldPos, camera[0] );

    return o;
}
