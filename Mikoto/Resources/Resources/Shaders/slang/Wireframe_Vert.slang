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

// Project pulled scene geometry for the wireframe debug view. Instance and animation transforms
// follow the geometry conventions of the shaded passes.

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
    nointerpolation uint mFaceFlags : TEXCOORD0;
    float4 mPosition : SV_Position;
};

struct DrawParams {
    Pointer mGeometryPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryDescPtr;

    Pointer mCameraBda;

    float4 mLineColor;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
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
    o.mPosition = mul( camera[0].mProjection, mul( camera[0].mViewMatrix, worldPos ) );
    o.mFaceFlags = meshInfo.mFaceFlags;

    return o;
}