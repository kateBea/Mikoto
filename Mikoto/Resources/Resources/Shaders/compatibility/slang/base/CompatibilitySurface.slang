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

#pragma once

#include "../../../slang/base/SurfaceCoverage_Helpers.slang"

struct DrawParameters {
    float4x4 World;
    float4x4 NormalMatrix;
    float4 BaseColor;
    float4 Emission;
    float4 Surface; // metallic, roughness, normal map scale, alpha cutoff
    uint4 Object; // entity ID, highlight, alpha mode, transparent pass
    float4 UVTransform;
    uint4 UVSets; // base color, metallic-roughness, normal, occlusion
    uint4 Material; // emissive UV set, face flags, shading model, sRGB formats
};

[[vk::binding( 1, 0 )]] ConstantBuffer<DrawParameters> draw : register( b1 );

static const uint kAlphaMask = 1;
static const uint kAlphaBlend = 2;
static const uint kDoubleSided = 1u;
static const uint kMirrored = 2u;
static const uint kBaseColorSRGB = 1u;
static const uint kEmissiveSRGB = 2u;

[[vk::binding( 2, 0 )]] SamplerState materialSampler : register( s2 );
[[vk::binding( 4, 0 )]] Texture2D baseColor : register( t4 );

struct SurfaceInput {
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 UV : TEXCOORD2;
    float4 Color : COLOR0;
    float2 UV1 : TEXCOORD3;
};

// Use the same authored winding rule as MaterialFrontFacing in Material_Helpers.slang.
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#double-sided
bool SurfaceFrontFacing( bool rasterFrontFace ) {
    bool front = rasterFrontFace != ( ( draw.Material.y & kMirrored ) != 0u );
    if ( !front && ( draw.Material.y & kDoubleSided ) == 0u ) {
        discard;
    }

    return front;
}

float2 SurfaceUV( SurfaceInput input, uint set ) {
    float2 uv = set == 0u ? input.UV : input.UV1;
    return uv * draw.UVTransform.xy + draw.UVTransform.zw;
}

float SurfaceAlpha( float alpha ) {
    if ( draw.Object.z == kAlphaMask ) {
        clip( alpha - draw.Surface.w );
    }
    else if ( draw.Object.z == kAlphaBlend && alpha <= 0.0 ) {
        discard;
    }

    return draw.Object.z == kAlphaBlend ? saturate( alpha ) : 1.0;
}
