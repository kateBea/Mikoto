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

// Pull and deform scene geometry, then export world-space surface attributes. This guide pass
// shares geometry evaluation with forward shading so depth and normals line up.

#pragma once

#include "base/Animation_Helpers.slang"
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct VSInput {
    uint mVertexID : SV_VertexID;
    uint mInstanceIndex : SV_InstanceID;
    uint mBaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    float3 mViewPos : TEXCOORD0;
    float3 mWorldPos : TEXCOORD1;
    float3 mNormal : TEXCOORD2;
    float2 mTexCoord0 : TEXCOORD3;
    float2 mTexCoord1 : TEXCOORD4;
    float4 mColor : TEXCOORD5;

    nointerpolation uint mMaterialIndex : TEXTCOORD6;
    nointerpolation uint mFaceFlags : TEXCOORD7;

    float4 mPosition : SV_Position;
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
    uint instanceIndex = input.mBaseInstanceIndex + input.mInstanceIndex;

    // Apply skinning
    MeshGeometryInfo meshInfo = LoadMeshGeometry( parameters.mGeometryPtr, instanceIndex );
    GeometryAllocation allocation = GetGeometryAllocation( meshInfo, parameters.mGeometryDescPtr );
    uint vertexIndex = PullVertexIndex( allocation, input.mVertexID );
    VertexDescription vertex = PullShadingVertex( meshInfo, allocation, vertexIndex );

    // Compute world position
    SkinningResult skinningResult = ApplySkinning( vertex, meshInfo, parameters.mSkinningPtr );

    float4 worldPos = skinningResult.mPosition;
    float3 worldNormal = skinningResult.mNormal;

    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCameraBda;

    o.mTexCoord0 = vertex.mUv0;
    o.mTexCoord1 = vertex.mUv1;
    o.mColor = vertex.mColor;

    o.mWorldPos = worldPos.xyz;
    o.mViewPos = mul( camera[0].mViewMatrix, worldPos ).xyz;
    o.mNormal = worldNormal;

    o.mMaterialIndex = meshInfo.mMaterialIndex;
    o.mFaceFlags = meshInfo.mFaceFlags;

    o.mPosition = CameraClipPosition( worldPos, camera[0] );

    return o;
}
