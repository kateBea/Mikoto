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

#version 450

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "ShaderBase.glsl"
#include "ClusteredShading.glsl"

#define INVALID_TEXTURE_INDEX -1

layout(location = 1) in vec3 in_FragmentWorldPos;
layout(location = 2) in vec3 in_Normals;
layout(location = 3) in vec2 in_TexCoord;
layout(location = 4) in vec3 in_Color;

layout(location = 5) flat in int in_AlbedoIndex;
layout(location = 6) flat in int in_NormalIndex;
layout(location = 7) flat in vec4 in_Albedo;
layout(location = 8) flat in vec4 in_Factors;

layout(location = 9) in vec3 in_FragmentViewPos;

layout(set = TEXTURES_SETINDEX, binding = 0) uniform sampler2D g_BindlessTextures[];

layout(scalar, set = PERPASS_SETINDEX, binding = 2) uniform GBufferCamUBO {
    float NearPlane;
    float FarPlane;
} u_Parameters;

layout(location = 0) out vec4 out_Position;
layout(location = 1) out vec4 out_Normal;
layout(location = 2) out vec4 out_Color;

vec3 GetNormalFromMap(sampler2D normalMap) {
    vec3 tangentNormal = texture(normalMap, in_TexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(in_FragmentWorldPos);
    vec3 Q2  = dFdy(in_FragmentWorldPos);
    vec2 st1 = dFdx(in_TexCoord);
    vec2 st2 = dFdy(in_TexCoord);

    vec3 N   = normalize(in_Normals);
    vec3 T  = normalize( Q1 * st2.t - Q2 * st1.t);
    vec3 B  = -normalize(cross( N, T ));
    mat3 TBN = mat3( T, B, N );

    return normalize(TBN * tangentNormal);
}

float LinearDepth(float depth) {
    float z = depth * 2.0f - 1.0f;

    return (2.0f * u_Parameters.NearPlane * u_Parameters.FarPlane) /
        (u_Parameters.FarPlane + u_Parameters.NearPlane - z * (u_Parameters.FarPlane - u_Parameters.NearPlane));
}

void main() {

    vec3 albedo     = in_AlbedoIndex != INVALID_TEXTURE_INDEX ?
    pow(texture(g_BindlessTextures[in_AlbedoIndex], in_TexCoord).rgb, vec3(2.2)) :
    in_Albedo.xyz;

    vec3 N = in_NormalIndex != INVALID_TEXTURE_INDEX ?
        GetNormalFromMap(g_BindlessTextures[in_NormalIndex]) :
        normalize(in_Normals);

    out_Position = vec4(in_FragmentViewPos, LinearDepth(gl_FragCoord.z));
    out_Normal = vec4(normalize(N) * 0.5 + 0.5, 1.0);
    out_Color = vec4(albedo , 1.0);
}