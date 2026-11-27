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

// Shared multisample subsurface composition declarations. The shader preserves sample-specific
// coverage while recombining scattered and unscattered surface lighting.

#pragma once
#include "Memory_Helpers.slang"

// Geometry application of the diffuse-only compositor, before MSAA resolve.
// https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/#3-reconstructing-the-final-image
struct SubsurfaceCompositeParameters {
    Pointer mGeometry;
    Pointer mSkinning;
    Pointer mAllocations;
    Pointer mCamera;
    Pointer mMaterials;
    uint mOriginal;
    uint mFiltered;
    uint mSpecular;
    uint mAmbient;
    uint mMetadata;
    uint mSampler;
};
MKT_DECLARE_PUSH_CONSTANT( SubsurfaceCompositeParameters, parameters );

struct SubsurfaceCompositeVertex {
    float4 mPosition : SV_Position;
    float2 mUv0 : TEXCOORD0;
    float2 mUv1 : TEXCOORD1;
    float mVertexAlpha : TEXCOORD2;
    nointerpolation uint mMaterialIndex : TEXCOORD3;
    nointerpolation uint mFaceFlags : TEXCOORD4;
    nointerpolation uint mDrawIdentity : TEXCOORD5;
};
