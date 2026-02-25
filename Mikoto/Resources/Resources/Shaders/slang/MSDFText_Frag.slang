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
#include "base/FontRender_Helpers.slang"

struct FSInput {
    float2 TexCoord : TEXCOORD0;
    uint AtlasIndex : TEXCOORD1;
    float4 Color : TEXCOORD2;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

[[vk::binding(0, TEXTURES_SET_INDEX)]]
Sampler2D g_BindlessTextures[];

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    float threshold = 0.5;

    float3 msdf = g_BindlessTextures[input.AtlasIndex].Sample(input.TexCoord).rgb;
    float sd = Median(msdf);

    float pxRange = ScreenPxRange3D(g_BindlessTextures[input.AtlasIndex], input.TexCoord);
    float screenPxDist = pxRange * (sd - threshold);

    float opacity = clamp(screenPxDist + threshold, 0.0, 1.0);

    float4 bg = float4(0.0);
    o.Color = lerp(bg, input.Color, opacity);

    return o;
}