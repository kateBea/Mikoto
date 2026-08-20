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

struct FSInput {
    float3 ViewPos : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 Uv0 : TEXCOORD3;
    float2 Uv1 : TEXCOORD4;
    float4 Color : TEXCOORD5;

    uint MaterialIndex : TEXTCOORD6;

    float4 ScreenPos : SV_Position;
};

struct FSOutput {
    float4 Position : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Color : SV_Target2;
    float4 Emissive : SV_Target3;
};

struct DrawParams {
    uint mGeometryBufferID;
    uint mMaterialsBufferID;
    uint mSkinningBufferID;

    uint mBasicSamplerID;

    uint mIndicesBufferID;
    uint mVerticesBufferID;

    uint mCameraDataID;
};

MKT_DECLARE_STRUCTURED_SRV buffers;

MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

[shader("fragment")]
FSOutput main(FSInput input)
{
    FSOutput o;

    ByteAddressBuffer materialsBuffer = buffers[parameters.mMaterialsBufferID];
    MaterialParameters materialInfo = materialsBuffer.Read<MaterialParameters>(0, input.MaterialIndex);

    CameraInfo camera = buffers[parameters.mCameraDataID].Read<CameraInfo>(0, 0);

    // NORMAL
    float2 normalUvs = materialInfo.NormalTextureSet == 0 ? input.Uv0 : input.Uv1;
    float3 N = (materialInfo.NormalIndex != INVALID_TEXTURE_INDEX)
                   ? GetNormalFromMap(textures[materialInfo.NormalIndex],
                            samplers[parameters.mBasicSamplerID],
                           normalUvs,
                           input.Normal,
                           input.WorldPos)
                   : normalize(input.Normal);

    o.Normal = float4(normalize(N) * 0.5 + 0.5, 1.0);

    // POSITION (view-space + depth)
    o.Position = normalize(float4(input.ViewPos, LinearDepth(input.ScreenPos.z, camera.Planes.xy)));

    // ALBEDO
    if (materialInfo.Workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS) {
        float2 baseColorUvs = materialInfo.BaseColorTextureSet == 0 ? input.Uv0 : input.Uv1;

        float4 albedo =
            (materialInfo.AlbedoIndex != INVALID_TEXTURE_INDEX)
                ? pow(textures[materialInfo.AlbedoIndex].Sample(samplers[parameters.mBasicSamplerID], baseColorUvs), float4(2.2))
                : materialInfo.BaseColorFactor;

        o.Color = float4(albedo.xyz, 1.0);
    }

    if (materialInfo.Workflow == PBR_WORKFLOW_SPECULAR_GLOSSINESS) {
        float2 diffuseUvs = materialInfo.BaseColorTextureSet == 0 ? input.Uv0 : input.Uv1;

        float4 diffuse =
            (materialInfo.DiffuseIndex != INVALID_TEXTURE_INDEX)
                ? pow(textures[materialInfo.DiffuseIndex].Sample(samplers[parameters.mBasicSamplerID], diffuseUvs), float4(2.2))
                : materialInfo.DiffuseFactor;

        o.Color = float4(diffuse.xyz, 1.0);
    }

    // Emisive
    o.Emissive = float4(0, 0, 0, 0);
    if (materialInfo.IsBloomy == MKT_SHADER_TRUE) {
        float3 emissiveContrib = materialInfo.EmissiveFactor.xyz * materialInfo.EmissiveStrength * materialInfo.BaseColorFactor.xyz;

        if (materialInfo.EmissiveIndex != INVALID_TEXTURE_INDEX) {
            Texture2D emissiveMap = textures[materialInfo.EmissiveIndex];
            float2 emissiveUvSet = materialInfo.EmissiveTextureSet == 0 ? input.Uv0 : input.Uv1;

            float3 emissive = emissiveMap.Sample(samplers[parameters.mBasicSamplerID], emissiveUvSet).rgb;

            emissiveContrib *= emissive;
        }

        o.Emissive = float4(emissiveContrib, 1.0f);
    }

    return o;
}
