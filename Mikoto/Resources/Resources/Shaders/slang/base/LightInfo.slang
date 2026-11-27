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

// CPU-mirrored direct-light records. Color, direction, shape and range fields are shared by
// clustered surface lighting and participating-media lighting.

#pragma once

static const int LIGHT_TYPE_POINT = 1;
static const int LIGHT_TYPE_SPOT = 2;
static const int LIGHT_TYPE_DIRECTIONAL = 0;
static const int LIGHT_TYPE_AREA = 3;
static const int LIGHT_TYPE_INACTIVE = -1;

// Values preserve renderer::AreaLightShape's serialized rectangle/mesh identities.
enum class RasterAreaShape : uint {
    eRectangle = 0,
    eMesh = 1,
    eSphere = 2,
    eTube = 3
};

struct LightInfo {
    float4 mPosition;
    float4 mDirection;// w marks the directional light used as the atmospheric sun.
    float4 mDiffuse;

    float mCutOff;
    float mOuterCutOff;

    float mIntensity;
    float mRadius;

    int mActiveLightType;

    int mShadowIndex;
    uint mShadowSlot;
    float mPadding;

    float4 mAreaAxisX;     // Unit right, half width or tube half length.
    float4 mAreaAxisY;     // Unit up, half height. Padding stores the sphere/tube source radius.
    uint4 mAreaEmission;   // Texture, sampler, manual sRGB decode, mip count. Invalid texture = flat colour.
    float4 mAreaEmissionUv;// Source material's UV scale and offset.
};
