//    Copyright 2025 ケイト
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

#include "Material_Helpers.slang"

// ============================================================================
// Constants
// ============================================================================
static const uint MAX_BONES = 256;
static const uint MAX_BONE_INFLUENCE = 4;

// ============================================================================
// Structs
// ============================================================================
struct BoneData {
    float4x4 mBoneMatrices[MAX_BONES];
};

struct SkinningResult {
    float4 mPosition;
    float3 mNormal;
};

SkinningResult ApplySkinning(VertexDescription vertex, MeshGeometryInfo meshInfo, Pointer skinningBufferPtr) {
    BoneData *skinningBuffer = (BoneData *)skinningBufferPtr;

    float4x4 model = meshInfo.Transform;
    float4 inWeight0 = vertex.weights0;
    float4 inJoint0 = vertex.joints0;

    float4 locPos = float4(0, 0, 0, 0);
    float3 normal = float3(0, 0, 0);
    if (meshInfo.SkinningMatricesID != -1) {
        uint animatorIndex = meshInfo.SkinningMatricesID - 1;

        float4x4 inJoinx = skinningBuffer[animatorIndex].mBoneMatrices[(uint)inJoint0.x];
        float4x4 inJoiny = skinningBuffer[animatorIndex].mBoneMatrices[(uint)inJoint0.y];
        float4x4 inJoinz = skinningBuffer[animatorIndex].mBoneMatrices[(uint)inJoint0.z];
        float4x4 inJoinw = skinningBuffer[animatorIndex].mBoneMatrices[(uint)inJoint0.w];

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

    SkinningResult result;
    result.mPosition = worldPos;
    result.mNormal = worldNormal;

    return result;
}