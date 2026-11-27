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

// Expand a material silhouette along its projected normal to form a contour shell. The
// expansion uses viewport scaling so outline width follows screen-space settings.

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
    uint mVertexID : SV_VertexID;
    uint mInstanceIndex : SV_InstanceID;
    uint mBaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    float3 mFragmentViewPos : TEXCOORD0;
    float3 mFragmentWorldPos : TEXCOORD1;
    float3 mNormal : NORMAL;
    float2 mTexCoord0 : TEXCOORD3;
    float2 mTexCoord1 : TEXCOORD4;
    float4 mColor : TEXCOORD5;

    nointerpolation uint mMaterialIndex : TEXCOORD6;
    nointerpolation uint mFaceFlags : TEXCOORD7;
    nointerpolation uint mDrawIdentity : TEXCOORD8;

    float4 mPositionH : SV_Position;
};

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

    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.mCameraPtr;

    o.mTexCoord0 = vertex.mUv0;
    o.mTexCoord1 = vertex.mUv1;
    o.mColor = vertex.mColor;

    o.mFragmentWorldPos = worldPos.xyz;
    o.mFragmentViewPos = mul( camera[0].mViewMatrix, worldPos ).xyz;
    o.mNormal = worldNormal;

    o.mDrawIdentity = instanceIndex + 1;
    o.mMaterialIndex = meshInfo.mMaterialIndex;
    o.mFaceFlags = meshInfo.mFaceFlags;

    o.mPositionH = CameraClipPosition( worldPos, camera[0] );

    MaterialParameters material = ( ( BufferReference<MaterialParameters> )parameters.mMaterialsPtr )[meshInfo.mMaterialIndex];
    float4 projectedNormal = mul( camera[0].mProjection, mul( camera[0].mViewMatrix, float4( worldNormal, 0 ) ) );
    // Differentiate the perspective divide, then normalize in pixel space.
    float2 direction = ( projectedNormal.xy * o.mPositionH.w - o.mPositionH.xy * projectedNormal.w ) * camera[0].mScreenDimensions.xy;
    float lengthSquared = dot( direction, direction );
    if ( lengthSquared > 1e-12 && o.mPositionH.w > 0 ) {
        o.mPositionH.xy += direction * rsqrt( lengthSquared ) * ( 2 * material.mSubsurfaceColorOpacity.a / camera[0].mScreenDimensions.xy ) * o.mPositionH.w;
    }
    return o;
}
