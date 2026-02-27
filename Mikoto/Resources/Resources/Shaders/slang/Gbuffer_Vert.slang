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

struct VSInput {
    float3 Position : ATTRIBUTE0;
    float3 Normal : ATTRIBUTE1;
    float3 Color : ATTRIBUTE2;
    float2 TexCoord : ATTRIBUTE3;

    uint InstanceIndex : SV_InstanceID;
    uint BaseInstanceIndex : SV_StartInstanceLocation;
};

struct VSOutput {
    float3 FragmentViewPos : TEXCOORD0;
    float3 FragmentWorldPos : TEXCOORD1;
    float3 VertexNormal : TEXCOORD2;
    float2 UV : TEXCOORD3;
    float3 VertexColor : TEXCOORD4;

    int AlbedoIndex : TEXCOORD5;
    int NormalIndex : TEXCOORD6;
    float4 Albedo : TEXCOORD7;

    float4 PositionH : SV_Position;
};

[[vk::binding(0, DYNAMIC_BUFFERS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(1, DYNAMIC_RESOURCE_SET_INDEX)]]
StructuredBuffer<MeshInfo, ScalarDataLayout> Meshes;

[shader("vertex")]
VSOutput main(VSInput input) {
    VSOutput o;

    MeshInfo mp = Meshes[input.BaseInstanceIndex + input.InstanceIndex];
    float4x4 model = mp.Transform;

    float4 worldPos = mul(model, float4(input.Position, 1.0));
    float4 viewPos = mul(u_Camera.ViewMatrix, mul(model, float4(input.Position, 1.0)));

    o.FragmentWorldPos = worldPos.xyz;
    o.FragmentViewPos = viewPos.xyz;

    // Normal transform
    float3x3 normalMat = float3x3((float3x3)transpose(mp.InverseModelView));

    o.VertexNormal = normalize(mul(normalMat, input.Normal));

    // Interpolated per-vertex data
    o.VertexColor = input.Color;
    o.UV = input.TexCoord;

    // Material
    o.AlbedoIndex = mp.AlbedoIndex;
    o.NormalIndex = mp.NormalIndex;
    o.Albedo = mp.Albedo;

    // Final clip position
    o.PositionH =
        mul(u_Camera.Projection,
        mul(u_Camera.ViewMatrix,
        mul(model, float4(input.Position, 1.0))));

    return o;
}