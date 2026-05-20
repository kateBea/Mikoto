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
    float4 Position : SV_Position;
};

struct DrawParams {
    uint GeometryID;
    uint SkinningID;

    uint IndexID;
    uint VertexID;

    uint CameraInfoBufferID;
};

MKT_DECLARE_STRUCTURED_SRV Buffers;
MKT_DECLARE_PUSH_CONSTANT(DrawParams) Parameters;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    ByteAddressBuffer GeometryBuffer = Buffers[Parameters.GeometryID];
    ByteAddressBuffer SkinningBuffer = Buffers[Parameters.SkinningID];

    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;
    MeshGeometryInfo meshInfo = GeometryBuffer.Read<MeshGeometryInfo>(0, instanceIndex);

    ByteAddressBuffer Vertices = Buffers[Parameters.VertexID];
    ByteAddressBuffer Indices = Buffers[Parameters.IndexID];

    uint index = Indices.Read<uint>(meshInfo.IndexOffset, input.VertexID);
    VertexDescription vertex = Vertices.Read<VertexDescription>(meshInfo.VertexOffset, index);

    float4x4 model = meshInfo.Transform;

    float4 inWeight0 = vertex.weights0;
    float4 inJoint0 = vertex.joints0;

    float4 locPos = float4(0, 0, 0, 0);
    float3 normal = float3(0, 0, 0);
    if (meshInfo.SkinningMatricesID != -1) {
        uint animatorIndex = meshInfo.SkinningMatricesID - 1;

        BoneData bonePalette = SkinningBuffer.Read<BoneData>(0, animatorIndex);

        float4x4 skinMat =
            inWeight0.x * bonePalette.BoneMatrices[(uint)inJoint0.x] +
            inWeight0.y * bonePalette.BoneMatrices[(uint)inJoint0.y] +
            inWeight0.z * bonePalette.BoneMatrices[(uint)inJoint0.z] +
            inWeight0.w * bonePalette.BoneMatrices[(uint)inJoint0.w];

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

    CameraInfo camera = Buffers[Parameters.CameraInfoBufferID].Read<CameraInfo>(0, 0);

    o.Position =
        mul(camera.Projection,
            mul(camera.ViewMatrix, worldPos));

    return o;
}