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

static const int LIGHT_TYPE_POINT = 1;
static const int LIGHT_TYPE_SPOT = 2;
static const int LIGHT_TYPE_DIRECTIONAL = 0;
static const int LIGHT_TYPE_INACTIVE = -1;

struct LightInfo {
    float4 Position;
    float4 Direction; // w marks the directional light used as the atmospheric sun.
    float4 Diffuse;

    float CutOff;
    float OuterCutOff;

    float Intensity;
    float Radius;

    int ActiveLightType;

    int ShadowIndex;
    uint ShadowSlot;
    float Padding;
};
