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

// Reference types
// https://docs.shader-slang.org/en/latest/coming-from-glsl.html#option-2-glsl-style-layout-syntax

// Command validate slang offline (Example)
// slangc Skybox_Vert.slang -profile glsl_450 -target spirv -entry main -stage vertex

#pragma once

#define MKT_DEFAULT_REGISTER_SPACE 0

#define MKT_STRUCTURED_SRV_BINDING 0
#define MKT_STRUCTURED_UAV_BINDING 1

#define MKT_SAMPLER_BINDING 2

#define MKT_TEXTURE_SRV_BINDING 3
#define MKT_TEXTURE_UAV_BINDING 4

#define MKT_ACCELERATION_STRUCTURE_BINDING 5

#if !defined(MKT_SUPPORT_D3D11)
    #define MKT_DECLARE_PUSH_CONSTANT(Type) [[vk::push_constant]] Type
    #define DECL_BINDING(BINDING, SET) [[vk::binding(BINDING, SET)]]
#else
    #define MKT_DECLARE_PUSH_CONSTANT(Type) ConstantBuffer<Type>
    #define DECL_BINDING(BINDING, SET) [[vk::binding(BINDING, 0)]]
#endif

// TODO: Operators. Follow GLSL convention for unified interface
//https://shader-slang.org/slang/user-guide/convenience-features#:~:text=Slang%20currently%20supports%20overloading%20the,%7C%7C%20operators%20is%20not%20supported.
// I think operator helpers must go in global namespace so they are visible

// Matrix * Vector
// ============================================================
// GLSL-style operator helpers (column-vector convention)
// ============================================================

// Matrix * Vector
float4 operator*(float4x4 m, float4 v) {
    return mul(m, v);
}

// Matrix * Matrix
float4x4 operator*(float4x4 a, float4x4 b) {
    return mul(a, b);
}

// 3x3
float3 operator*(float3x3 m, float3 v) {
    return mul(m, v);
}

float3x3 operator*(float3x3 a, float3x3 b) {
    return mul(a, b);
}

// From: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics
// Common vertex input semantics (HLSL / Slang):
//
// POSITION[n]      : float4  → Vertex position in object space
// POSITIONT        : float4  → Transformed vertex position (post-projection)
//
// NORMAL[n]        : float3/4 → Vertex normal
// BINORMAL[n]      : float3/4 → Bitangent (binormal)
// TANGENT[n]       : float3/4 → Tangent vector
//
// COLOR[n]         : float4  → Vertex color (diffuse / specular)
//
// TEXCOORD[n]      : float1–4 → Texture coordinates (UV sets)
//
// BLENDINDICES[n]  : uint    → Bone indices for skinning
// BLENDWEIGHT[n]   : float   → Bone weights for skinning
//
// PSIZE[n]         : float   → Point size (used for point primitives)
//
// Notes:
// - [n] denotes multiple sets (e.g. TEXCOORD0, TEXCOORD1, BLENDINDICES0…)
// - For skinning, each BLENDINDICES/BLENDWEIGHT pair typically represents 4 influences
// - In Vulkan, semantics are ignored at runtime; explicit location bindings must match vertex input layout
// - Types may vary (e.g. float3 vs float4), but must match pipeline input definitions


// D3D11 Stuff

static const uint kArrayMaxSize = 40;

#define X10(n) X(n) X(n+1) X(n+2) X(n+3) X(n+4) X(n+5) X(n+6) X(n+7) X(n+8) X(n+9)
#define MKT_TEXTURE_LIST \
    X10(0)  \
    X10(10) \
    X10(20) \
    X10(30)

namespace mikoto {
    float4 SampleTexture(
        Texture2D textures[kArrayMaxSize],
        int texIndex,
        SamplerState samplers[kArrayMaxSize],
        int samplerIndex,
        float2 uv)
    {
        switch(texIndex) {
            #define X(n) case n: return textures[n].Sample(samplers[samplerIndex], uv);
                MKT_TEXTURE_LIST
            #undef X

            default: return float4(0, 0, 0, 1); // Color de error
        }
    }
}