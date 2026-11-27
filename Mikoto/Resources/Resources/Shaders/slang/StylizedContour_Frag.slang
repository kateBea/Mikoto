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

// Apply contour coverage and material visibility to the expanded shell. The resulting fragment
// supplies the stylized outline color.

#pragma once

#include "base/Base.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/SurfaceShading_Helpers.slang"

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
    float mSsaoIntensity;

    uint mSsaoTextureID;

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

MKT_DECLARE_PUSH_CONSTANT( DrawParams, parameters );

struct FSInput {
    float3 mFragmentViewPos : TEXCOORD0;
    float3 mFragmentWorldPos : TEXCOORD1;
    float3 mNormal : NORMAL;
    float2 mTexCoord0 : TEXCOORD3;
    float2 mTexCoord1 : TEXCOORD4;
    float4 mVertexColor : TEXCOORD5;

    nointerpolation uint mMaterialIndex : TEXCOORD6;
    nointerpolation uint mFaceFlags : TEXCOORD7;
    nointerpolation uint mDrawIdentity : TEXCOORD8;

    float4 mScreenPos : SV_Position;
    bool mIsFrontFace : SV_IsFrontFace;
};

struct FSOutput {
    float4 mColor : SV_Target0;
};

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput output = (FSOutput)0;
    BufferReference<MaterialParameters> materialsBuffer = ( BufferReference<MaterialParameters> )parameters.mMaterialsPtr;
    MaterialParameters material = materialsBuffer[input.mMaterialIndex];

    // The expanded back faces form the contour around the original silhouette.
    if ( MaterialFrontFacing( input.mFaceFlags, input.mIsFrontFace ) ) {
        discard;
    }

    uint shadingModel = uint( material.mSubsurface.z );
    if ( !SurfaceStylized( shadingModel ) || material.mSubsurfaceColorOpacity.a <= 0 || material.mAlphaMask != PBR_ALPHA_OPAQUE ) {
        discard;
    }
    output.mColor = float4( material.mSubsurfaceColorOpacity.rgb, 1 );
    return output;
}
