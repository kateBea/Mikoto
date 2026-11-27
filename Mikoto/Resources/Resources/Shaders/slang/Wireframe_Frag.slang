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

// Output the wireframe debug surface color. Rasterizer wireframe mode determines which triangle
// edges generate fragments.

#pragma once

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct FSOutput {
    float4 mColor : SV_Target0;
};

struct FSInput {
    nointerpolation uint mFaceFlags : TEXCOORD0;
    float4 mScreenPos : SV_Position;
};

struct DrawParams {
    Pointer mGeometryPtr;
    Pointer mSkinningPtr;
    Pointer mGeometryDescPtr;

    Pointer mCameraBda;

    float4 mLineColor;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, Parameters );

[shader( "fragment" )]
FSOutput main( FSInput input, bool frontFacing : SV_IsFrontFace ) {
    if ( MaterialBackfaceDiscard( input.mFaceFlags, frontFacing ) ) {
        discard;
    }
    FSOutput o;

    o.mColor = Parameters.mLineColor;

    return o;
}