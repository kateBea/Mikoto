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
#include "base/Lighting_Helpers.slang"

struct FSInput {
    float3 ViewPos : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
    float3 Normal : TEXCOORD2;
    float2 UV : TEXCOORD3;
    float3 Color : TEXCOORD4;

    int AlbedoIndex : TEXCOORD5;
    int NormalIndex : TEXCOORD6;
    float4 Albedo : TEXCOORD7;

    float4 ScreenPos : SV_Position;
};

struct FSOutput {
    float4 Position : SV_Target0;
    float4 Normal : SV_Target1;
    float4 Color : SV_Target2;
};

[[vk::binding(Slot_0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(Slot_0, UNBOUNDED_IV_SAMPLERS_SET_INDEX)]]
Sampler2D g_BindlessTextures[];

[shader("fragment")]
FSOutput main(FSInput input)
{
    FSOutput o;

    // NORMAL
    float3 N = (input.NormalIndex != INVALID_TEXTURE_INDEX)
                   ? GetNormalFromMap(g_BindlessTextures[input.NormalIndex],
                           input.UV,
                           input.Normal,
                           input.WorldPos)
                   : normalize(input.Normal);

    o.Normal = float4(normalize(N) * 0.5 + 0.5, 1.0);

    // POSITION (view-space + depth)
    o.Position = normalize(float4(input.ViewPos, LinearDepth(input.ScreenPos.z, u_Camera.Planes)));

    // ALBEDO
    float4 albedo =
        (input.AlbedoIndex != INVALID_TEXTURE_INDEX)
                        ? pow(g_BindlessTextures[input.AlbedoIndex].Sample(input.UV), float4(2.2))
                        : input.Albedo;

    o.Color = float4(albedo.xyz, 1.0);

    return o;
}
