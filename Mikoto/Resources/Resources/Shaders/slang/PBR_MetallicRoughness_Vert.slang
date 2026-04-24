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
#include "base/Camera_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Animation_Helpers.slang"

struct FinalShadingParameters {
    float4x4 LightViewProjection;

    float4 GridSize;
    float Exposure;
    float Gamma;
    float MaxReflectionLOD;

    uint ActiveLights;

    int EnableSSAO;
    int UseBlurred;
    float SSAOIntensity;

    int PrefilteredCubeMipLevels;
    float ScaleIBLAmbient;
    int Step; // for debug

    int IsSkyboxActive;

    int ToneMapType;
};

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(Slot_0, UAV_SET_INDEX)]] StructuredBuffer<MeshParameters, ScalarDataLayout> Meshes;
[[vk::binding(Slot_1, UAV_SET_INDEX)]] StructuredBuffer<BoneData, ScalarDataLayout> Bones;

// Vertex pullign data
[[vk::binding(Slot_5, UAV_SET_INDEX)]] ByteAddressBuffer[] Vertices;
[[vk::binding(Slot_6, UAV_SET_INDEX)]] ByteAddressBuffer[] Indices;

[[vk::push_constant]]
FinalShadingParameters u_Constants;

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

    float4 FragPosLightSpace : TEXCOORD7;

    float4 PositionH : SV_Position;
};

T LoadData<T>(uint bufferID, uint baseOffset, uint index) {
    return Vertices[bufferID].LoadByteOffset<T>(
        (baseOffset + index) * sizeof(T)
    );
}

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    // Compute instance index
    uint instanceIndex = input.BaseInstanceIndex + input.InstanceIndex;
    MeshParameters meshInfo = Meshes[instanceIndex];
    o.MaterialIndex = instanceIndex;

    // Compute attribute index
    uint index = Indices[0].LoadByteOffset<uint>( (meshInfo.IndexOffset + input.VertexID) * sizeof(uint) );
    VertexData vertex = LoadData<VertexData>( 0, meshInfo.VertexOffset, index );

    float4x4 model = meshInfo.Transform;

    float4 inWeight0 = vertex.Weight;
    float4 inJoint0 = vertex.Joint;

    float4 locPos = float4(0, 0, 0, 0);
    float3 normal = float3(0, 0, 0);
    if (meshInfo.AnimatorID != -1) {
        // Mesh is animated or has armature
        uint animatorIndex = meshInfo.AnimatorID - 1;

        BoneData bonePalette = Bones[animatorIndex];

        float4x4 skinMat =
            inWeight0.x * bonePalette.BoneMatrices[(uint)inJoint0.x] +
            inWeight0.y * bonePalette.BoneMatrices[(uint)inJoint0.y] +
            inWeight0.z * bonePalette.BoneMatrices[(uint)inJoint0.z] +
            inWeight0.w * bonePalette.BoneMatrices[(uint)inJoint0.w];

        locPos = mul(skinMat, float4(vertex.Position, 1.0));
        normal = normalize(mul((float3x3)skinMat, vertex.Normal));
        
    }
    else {
        locPos = float4(vertex.Position, 1.0);
        normal = vertex.Normal;
    }

    float4 worldPos = mul(model, locPos);
    float3 worldNormal = normalize(mul((float3x3)model, normal));

    o.TexCoord0 = vertex.TexCoord0;
    o.TexCoord1 = vertex.TexCoord1;
    o.Color = vertex.Color;

    o.FragmentWorldPos = worldPos.xyz;
    o.FragmentViewPos = mul(u_Camera.ViewMatrix, worldPos).xyz;
    o.Normal = worldNormal;

    o.FragPosLightSpace = mul(u_Constants.LightViewProjection, worldPos);

    o.PositionH =
        mul(u_Camera.Projection,
            mul(u_Camera.ViewMatrix, worldPos));

    return o;
}