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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Animation_Helpers.slang"

struct DrawParams {
    Pointer mGeometryDescPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryPtr;
    Pointer mCameraPtr;

    Pointer mMaterialsPtr;

    Pointer mClusterPtr;
    Pointer mLightsPtr;

    float2 mGridSize1;
    float2 mGridSize2;

    uint mEnableSsao;

    uint mBrdfColorTargetID;
    uint mPrefilterCubeRtID;
    uint mIrradianceCubeRtID;

    uint mSamplerCubeID;
    uint mSamplerBasicID;

    uint mActiveLightCount;

    uint mPrefilteredCubeMipLevels;
    float mScaleIblAmbient;

    uint mIsSkyboxActive;

    uint mDirectionalShadowsInfoBufferID;
    uint mDirectionalShadowCastersCount;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

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

    uint MaterialIndex : TEXCOORD6;

    float4 PositionH : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    // Prepare vertex data
    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;
    VertexDescription vertex = GetVertexDescription(instanceIndex, input.VertexID, parameters.mGeometryPtr, parameters.mGeometryDescPtr);

    // Apply skinning
    MeshGeometryInfo *geometryBuffer = (MeshGeometryInfo *)parameters.mGeometryPtr;
    MeshGeometryInfo meshInfo = geometryBuffer[instanceIndex];

    // Compute world position
    SkinningResult skinningResult = ApplySkinning(vertex, meshInfo, parameters.mSkinningPtr);

    float4 worldPos = skinningResult.mPosition;
    float3 worldNormal = skinningResult.mNormal;

    CameraInfo *camera = (CameraInfo *)parameters.mCameraPtr;

    o.TexCoord0 = vertex.uv0;
    o.TexCoord1 = vertex.uv1;
    o.Color = vertex.color;

    o.FragmentWorldPos = worldPos.xyz;
    o.FragmentViewPos = mul(camera->mViewMatrix, worldPos).xyz;
    o.Normal = worldNormal;

    o.MaterialIndex = instanceIndex;

    o.PositionH =
        mul(camera->mProjection,
            mul(camera->mViewMatrix, worldPos));

    return o;
}