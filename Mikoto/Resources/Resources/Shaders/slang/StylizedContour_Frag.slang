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
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD3;
    float2 TexCoord1 : TEXCOORD4;
    float4 VertexColor : TEXCOORD5;

    nointerpolation uint MaterialIndex : TEXCOORD6;
    nointerpolation uint FaceFlags : TEXCOORD7;
    nointerpolation uint DrawIdentity : TEXCOORD8;

    float4 ScreenPos : SV_Position;
    bool IsFrontFace : SV_IsFrontFace;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput output = (FSOutput)0;
    BufferReference<MaterialParameters> materialsBuffer = ( BufferReference<MaterialParameters> )parameters.mMaterialsPtr;
    MaterialParameters material = materialsBuffer[input.MaterialIndex];

    // The expanded back faces form the contour around the original silhouette.
    if ( MaterialFrontFacing( input.FaceFlags, input.IsFrontFace ) ) {
        discard;
    }

    uint shadingModel = uint( material.Subsurface.z );
    if ( !SurfaceStylized( shadingModel ) || material.SubsurfaceColorOpacity.a <= 0 || material.AlphaMask != PBR_ALPHA_OPAQUE ) { discard; }
    output.Color = float4( material.SubsurfaceColorOpacity.rgb, 1 );
    return output;
}
