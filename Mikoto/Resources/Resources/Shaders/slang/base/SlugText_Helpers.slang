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

struct SlugTextOutput {
    float4 mPosition : SV_Position;
    float2 mTexcoord : TEXCOORD0;
    nointerpolation float4 mColor : TEXCOORD1;
    nointerpolation float4 mBanding : TEXCOORD2;
    nointerpolation int4 mGlyph : TEXCOORD3;
    nointerpolation uint2 mTextures : TEXCOORD4;
    nointerpolation float mOutline : TEXCOORD7;
    nointerpolation float4 mOutlineColor : TEXCOORD8;
};
