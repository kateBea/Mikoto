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

#include "base/Base.slang"

struct VSInput {
    float3 mPosition : POSITION;
    float2 mUv0 : TEXCOORD0;
};

struct VSOutput {
    float4 mPosition : SV_POSITION;
    float2 mUv : TEXCOORD;
};

struct MyData {
    float4x4 mModel;
    float4x4 mView;
    float4x4 mProjection;

    int mTextureIndex;
    int mSamplerIndex;
};

DECL_BINDING( 2, 0 ) ConstantBuffer<MyData> Matrices;

[shader( "vertex" )]
VSOutput main( VSInput input ) {
    VSOutput o;
    o.mPosition = Matrices.mProjection * Matrices.mView * Matrices.mModel * float4( input.mPosition, 1.0 );
    o.mUv = input.mUv0;
    return o;
}