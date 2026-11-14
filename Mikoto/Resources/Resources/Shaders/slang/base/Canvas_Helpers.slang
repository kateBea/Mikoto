#pragma once
#include "Base.slang"
#include "Memory_Helpers.slang"
struct CanvasQuad {
    float4 positions[4];
    float4 color;
    float4 uv;
    float2 size;
    float rounding;
    float stroke;
    uint textureID;
    uint kind;
    float distanceRange;
    float padding;
};
struct CanvasParameters {
    Pointer quads;
    uint samplerID;
    uint offset;
};
MKT_DECLARE_PUSH_CONSTANT( CanvasParameters, parameters );
struct CanvasVaryings {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 local : TEXCOORD1;
    nointerpolation uint index : TEXCOORD2;
};
