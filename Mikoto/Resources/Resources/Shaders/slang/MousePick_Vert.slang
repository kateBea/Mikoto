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

// Project selectable geometry using the same instance and deformation data as scene rendering.
// Carry the object identifier to the picking fragment stage.

#pragma once

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Animation_Helpers.slang"

struct VSInput {
    uint mVertexID : SV_VertexID;
    uint mInstanceIndex : SV_InstanceID;
    uint mBaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    nointerpolation uint mObjectID : ATTRIBUTE0;
    nointerpolation uint mMaterialIndex : TEXCOORD0;
    nointerpolation uint mFaceFlags : TEXCOORD4;
    float2 mUv0 : TEXCOORD1;
    float2 mUv1 : TEXCOORD2;
    float mVertexAlpha : TEXCOORD3;
    float4 mPosition : SV_Position;
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

    // Prepare vertex data
    uint instanceIndex = input.mBaseInstanceIndex + input.mInstanceIndex;

    // Apply skinning
    MeshGeometryInfo meshInfo = LoadMeshGeometry( parameters.mGeometryPtr, instanceIndex );
    GeometryAllocation allocation = GetGeometryAllocation( meshInfo, parameters.mGeometryDescPtr );
    uint vertexIndex = PullVertexIndex( allocation, input.mVertexID );
    VertexDescription vertex = PullDepthVertex( meshInfo, allocation, vertexIndex );

    // Compute world position
    float4 worldPos = ApplySkinningPosition( vertex, meshInfo, parameters.mSkinningPtr );

    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCameraBda;

    // Final position
    o.mPosition = CameraClipPosition( worldPos, camera[0] );

    o.mObjectID = meshInfo.mObjectID;
    o.mMaterialIndex = meshInfo.mMaterialIndex;
    o.mFaceFlags = meshInfo.mFaceFlags;
    o.mUv0 = vertex.mUv0;
    o.mUv1 = vertex.mUv1;
    o.mVertexAlpha = vertex.mColor.a;

    return o;
}
