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

// Shared canopy-shadow push constants and terrain-grid reconstruction. Shadow vertex and
// fragment stages address the same grass description and light record.

#pragma once
#include "Memory_Helpers.slang"
#include "Camera_Helpers.slang"
#include "Grass_Shared.slang"
#include "ShadowMap_Helpers.slang"
#include "TerrainSurface_Helpers.slang"

// Canopy tuning and independent stable hash streams; these are geometry/coverage controls, not
// per-frame randomness.
static const float kGrassCanopyHeightScale = 1.2;
static const float kGrassCanopyNoiseScale = 3;
static const float kGrassCanopyMinimumNoiseScale = .1;
static const uint kGrassProxyPaletteSalt = 17;
static const uint kGrassProxyCoverageSalt = 43;
static const uint kGrassProxyDepthSalt = 53;
static const float kGrassProxyVerticalCoverage = .25;

struct GrassShadowParameters {
    uint64_t mDescriptions;
    uint64_t mVertices;
    uint64_t mRecords;
    uint64_t mCamera;
    uint mDescription;
    uint mRecord;
    uint mFace;
    uint mPadding;
    float3 mToLight;
    uint mPadding1;
};
MKT_DECLARE_PUSH_CONSTANT( GrassShadowParameters, parameters );

struct GrassShadowVaryings {
    float3 mBase : TEXCOORD0;
    float3 mCanopy : TEXCOORD1;
    float3 mLocal : TEXCOORD2;
    float3 mGridHeight : TEXCOORD3;
    float4 mPosition : SV_Position;
};

