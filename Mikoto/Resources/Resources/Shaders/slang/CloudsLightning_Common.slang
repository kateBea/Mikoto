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

// CPU-mirrored lightning segment data and draw parameters. Shared records let the vertex stage
// expand bolts while the fragment stage evaluates their bright cores and glow.

#pragma once
#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/CloudLighting_Helpers.slang"

struct CloudBoltSegment {
    float4 mStartWidth;
    float4 mEndIntensity;
    float4 mColor;
};
struct LightningParameters {
    Pointer mFrame;
    Pointer mCamera;
    uint mDepth;
    uint mPadding;
};
struct LightningVertex {
    float4 mPosition : SV_Position;
    float3 mWorld : TEXCOORD0;
    float2 mUv : TEXCOORD1;
    nointerpolation float4 mColorIntensity : TEXCOORD2;
};
MKT_DECLARE_PUSH_CONSTANT( LightningParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
lightningDepth;
