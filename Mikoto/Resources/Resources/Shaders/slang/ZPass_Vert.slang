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

struct VSInput {
    uint VertexID : SV_VertexID;
    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    CameraInfo mCameraBda : TEXCOORD0;
    float4 Position : SV_Position;
};

struct DrawParams {
    Pointer mGeometryPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryDescPtr;

    Pointer mCameraBda;
};

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    // Prepare vertex data
    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;
    VertexDescription vertex = GetVertexDescription(instanceIndex, input.VertexID, parameters.mGeometryPtr, parameters.mGeometryDescPtr);

    // Apply skinning
    MeshGeometryInfo *geometryBuffer = (MeshGeometryInfo *)parameters.mGeometryPtr;
    MeshGeometryInfo meshInfo = geometryBuffer[instanceIndex];

    SkinningResult skinningResult = ApplySkinning(vertex, meshInfo, parameters.mSkinningPtr);
    
    // Compute world position
    float4 worldPos = mul(meshInfo.Transform, skinningResult.mPosition);
    float3 worldNormal = normalize(mul((float3x3)meshInfo.Transform, skinningResult.mNormal));

    CameraInfo *camera = (CameraInfo*)parameters.mCameraBda;

    // Final position
    o.Position =
        mul(camera->mProjection,
            mul(camera->mViewMatrix, worldPos));

    return o;
}