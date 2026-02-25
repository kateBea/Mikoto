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

#include "base/Base.slang"

struct FSInput {
    float3 WorldPos : TEXCOORD0;
    float3 CameraWorldPos : TEXCOORD1;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

static const float gridSize = 100.0;
static const float gridMinPixelsBetweenCells = 2.0;
static const float gridCellSize = 0.025;

static const float4 gridColorThin = float4(0.5, 0.5, 0.5, 1.0);
static const float4 gridColorThick = float4(0.3, 0.2, 0.5, 1.0);

float log10_f(float x) {
    return log(x) / log(10.0);
}

float satf(float x) {
    return clamp(x, 0.0, 1.0);
}

float2 satv(float2 v) {
    return clamp(v, float2(0.0), float2(1.0));
}

float max2(float2 v) {
    return max(v.x, v.y);
}

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float2 dvx = float2(ddx(input.WorldPos.x), ddy(input.WorldPos.x));
    float2 dvy = float2(ddx(input.WorldPos.z), ddy(input.WorldPos.z));

    float lx = length(dvx);
    float ly = length(dvy);

    float2 dudv = float2(lx, ly);

    float l = length(dudv);

    float LOD = max(0.0, log10_f(l * gridMinPixelsBetweenCells / gridCellSize) + 1.0);

    float GridCellSizeLod0 = gridCellSize * pow(10.0, floor(LOD));
    float GridCellSizeLod1 = GridCellSizeLod0 * 10.0;
    float GridCellSizeLod2 = GridCellSizeLod1 * 10.0;

    dudv *= 4.0;

    float2 mod_div = fmod(input.WorldPos.xz, GridCellSizeLod0) / dudv;
    float Lod0a = max2(float2(1.0) - abs(satv(mod_div) * 2.0 - float2(1.0)));

    mod_div = fmod(input.WorldPos.xz, GridCellSizeLod1) / dudv;
    float Lod1a = max2(float2(1.0) - abs(satv(mod_div) * 2.0 - float2(1.0)));

    mod_div = fmod(input.WorldPos.xz, GridCellSizeLod2) / dudv;
    float Lod2a = max2(float2(1.0) - abs(satv(mod_div) * 2.0 - float2(1.0)));

    float LOD_fade = frac(LOD);
    float4 Color;

    if (Lod2a > 0.0) {
        Color = gridColorThick;
        Color.a *= Lod2a;
    }
    else if (Lod1a > 0.0) {
        Color = lerp(gridColorThick, gridColorThin, LOD_fade);
        Color.a *= Lod1a;
    }
    else {
        Color = gridColorThin;
        Color.a *= (Lod0a * (1.0 - LOD_fade));
    }

    float dist = length(input.WorldPos.xz - input.CameraWorldPos.xz);
    float OpacityFalloff = (1.0 - satf(dist / gridSize));

    Color.a *= OpacityFalloff;

    o.Color = Color;

    return o;
}