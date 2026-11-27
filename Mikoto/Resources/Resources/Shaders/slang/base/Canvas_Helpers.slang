// Canvas quad upload layout and interpolants. Each record supplies its clip positions, UV
// rectangle, tint and shape parameters for the canvas stages.

#pragma once
#include "Base.slang"
#include "Memory_Helpers.slang"
struct CanvasQuad {
    float4 mPositions[kQuadCornerCount];
    float4 mColor;
    float4 mUv;
    float2 mSize;
    float mRounding;
    float mStroke;
    uint mTextureID;
    uint mKind;
    float mDistanceRange;
    float mPadding;
};
struct CanvasParameters {
    Pointer mQuads;
    uint mSamplerID;
    uint mOffset;
};
MKT_DECLARE_PUSH_CONSTANT( CanvasParameters, parameters );
struct CanvasVaryings {
    float4 mPosition : SV_Position;
    float2 mUv : TEXCOORD0;
    float2 mLocal : TEXCOORD1;
    nointerpolation uint mIndex : TEXCOORD2;
};
