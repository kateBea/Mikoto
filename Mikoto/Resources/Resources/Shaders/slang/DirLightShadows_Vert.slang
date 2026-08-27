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
#include "base/Lighting_Helpers.slang"
#include "base/Animation_Helpers.slang"

struct VSInput {
    uint VertexID : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    float4 Position : SV_Position;
};

struct DrawParams {
    Pointer mGeometryInfoBuffer;
    Pointer mSkinningInfoBuffer;

    Pointer mGeometryAllocBuffer;

    Pointer mDirShadowsBuffer;
    uint mShadowCasterID;
};

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    // Prepare vertex data
    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;

    MeshGeometryInfo *geometryBuffer = (MeshGeometryInfo *)parameters.mGeometryInfoBuffer;
    GeometryAllocation *geometryAllocBuffer = (GeometryAllocation *)parameters.mGeometryAllocBuffer;

    MeshGeometryInfo meshInfo = geometryBuffer[instanceIndex];
    GeometryAllocation geometryAlloc = geometryAllocBuffer[meshInfo.GeometryIndex];

    uint *indexBuffer = (uint *)geometryAlloc.mIndexBufferBda;
    VertexDescription *vertexBuffer = (VertexDescription *)geometryAlloc.mVertexBufferBda;

    uint index = indexBuffer[input.VertexID];
    VertexDescription vertex = vertexBuffer[index];

    // Apply skinning
    float4x4 model = meshInfo.Transform;
    float4 inWeight0 = vertex.weights0;
    float4 inJoint0 = vertex.joints0;

    float4 locPos = float4(0, 0, 0, 0);
    float3 normal = float3(0, 0, 0);
    if (meshInfo.SkinningMatricesID != -1) {
        BoneData *skinningBuffer = (BoneData *)parameters.mSkinningInfoBuffer;

        uint animatorIndex = meshInfo.SkinningMatricesID - 1;
        uint boneBase = animatorIndex * MAX_BONES;

        float4x4 inJoinx = skinningBuffer[boneBase].mBoneMatrices[(uint)inJoint0.x];
        float4x4 inJoiny = skinningBuffer[boneBase].mBoneMatrices[(uint)inJoint0.y];
        float4x4 inJoinz = skinningBuffer[boneBase].mBoneMatrices[(uint)inJoint0.z];
        float4x4 inJoinw = skinningBuffer[boneBase].mBoneMatrices[(uint)inJoint0.w];

        float4x4 skinMat =
            inWeight0.x * inJoinx +
            inWeight0.y * inJoiny +
            inWeight0.z * inJoinz +
            inWeight0.w * inJoinw;

        locPos = mul(skinMat, float4(vertex.position, 1.0));

        normal = normalize(
            mul((float3x3)skinMat, vertex.normal));
    }
    else {
        locPos = float4(vertex.position, 1.0);
        normal = vertex.normal;
    }

    float4 worldPos = mul(model, locPos);
    float3 worldNormal = normalize(mul((float3x3)model, normal));

    DirectionalShadowMapInfo* shadowMapInfo = (DirectionalShadowMapInfo *)parameters.mDirShadowsBuffer;

    o.Position =
        mul(shadowMapInfo->mLightProjection,
            mul(shadowMapInfo->mLightView, worldPos));

    return o;
}