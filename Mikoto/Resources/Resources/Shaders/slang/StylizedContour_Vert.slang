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

struct DrawParams {
    Pointer mGeometryDescPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryPtr;
    Pointer mCameraPtr;

    Pointer mMaterialsPtr;

    Pointer mClusterPtr;
    Pointer mLightsPtr;

    float2 mGridSize1;
    float mGridSizeZ;
    uint mRayIndirect;

    uint mClusterBufferSize;
    uint mLightsBufferSize;

    uint mEnableSsao;

    uint mBrdfColorTargetID;
    uint mPrefilterCubeRtID;
    uint mIrradianceCubeRtID;

    uint mSamplerCubeID;
    uint mSamplerBasicID;

    uint mActiveLightCount;

    uint mPrefilterMipLevelsAndSkybox;
    float mScaleIblAmbient;

    uint mRayDirect;

    uint mShadowInfoBufferID;
    uint mShadowRecordCount;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

struct VSInput {
    uint VertexID : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD3;
    float2 TexCoord1 : TEXCOORD4;
    float4 Color : TEXCOORD5;

    nointerpolation uint MaterialIndex : TEXCOORD6;
    nointerpolation uint FaceFlags : TEXCOORD7;
    nointerpolation uint DrawIdentity : TEXCOORD8;

    float4 PositionH : SV_Position;
};

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

    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCameraPtr;

    o.TexCoord0 = vertex.uv0;
    o.TexCoord1 = vertex.uv1;
    o.Color = vertex.color;

    o.FragmentWorldPos = worldPos.xyz;
    o.FragmentViewPos = mul( camera[0].mViewMatrix, worldPos ).xyz;
    o.Normal = worldNormal;

    o.DrawIdentity = instanceIndex + 1;
    o.MaterialIndex = meshInfo.MaterialIndex;
    o.FaceFlags = meshInfo.FaceFlags;

    o.PositionH = CameraClipPosition( worldPos, camera[0] );

    MaterialParameters material = ( ( BufferReference<MaterialParameters> )parameters.mMaterialsPtr )[meshInfo.MaterialIndex];
    float4 projectedNormal = mul( camera[0].mProjection, mul( camera[0].mViewMatrix, float4( worldNormal, 0 ) ) );
    // Differentiate the perspective divide, then normalize in pixel space.
    float2 direction = ( projectedNormal.xy * o.PositionH.w - o.PositionH.xy * projectedNormal.w ) * camera[0].mScreenDimensions.xy;
    float lengthSquared = dot( direction, direction );
    if ( lengthSquared > 1e-12 && o.PositionH.w > 0 ) {
        o.PositionH.xy += direction * rsqrt( lengthSquared ) * ( 2 * material.SubsurfaceColorOpacity.a / camera[0].mScreenDimensions.xy ) * o.PositionH.w;
    }
    return o;
}
