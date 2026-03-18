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

const static int c_ToneMapLinear = 0;
const static int c_ToneMapReinhard = 1;
const static int c_ToneMapUncharted2 = 2;
const static int c_ToneMapACES = 3;
const static int c_ToneMapKhronosNeutral = 4;

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 Uncharted2Tonemap(float3 color) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return ((color * (A * color + C * B) + D * E) /
            (color * (A * color + B) + D * F)) - (E / F);
}

float4 Tonemap_Uncharted2(float4 color, float exposure, float gamma) {
    float3 outcol = Uncharted2Tonemap(color.rgb * exposure);
    outcol = outcol * (1.0f / Uncharted2Tonemap(float3(11.2f)));

    return float4(pow(outcol, float3(1.0f / gamma)), color.a);
}

float3 Tonemap_Reinhard(float3 color) {
    return color / (1.0 + color);
}

float4 Tonemap_Linear(float4 inColor, float exposure, float gamma) {

    float3 color = inColor.xyz * exposure;
    color = saturate(color);

    // gamma correction
    return float4(pow(color, float3(1.0f / gamma)), inColor.a);
}

float4 Tonemap_Aces(float4 color, float exposure, float gamma) {
    float3 outColor = color.xyz * exposure;

    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    outColor = saturate((outColor * (a * outColor + b)) / (outColor * (c * outColor + d) + e));
    return float4(pow(outColor, float3(1.0f / gamma)), color.a);
}

// Input color is non-negative and resides in the Linear Rec. 709 color space.
// Output color is also Linear Rec. 709, but in the [0, 1] range.
// https://github.com/KhronosGroup/ToneMapping
// https://www.khronos.org/news/press/khronos-pbr-neutral-tone-mapper-released-for-true-to-life-color-rendering-of-3d-products
float4 Tonemap_KhronosNeutral(float4 inColor, float exposure, float gamma) {
    float3 color = inColor.xyz * exposure;

    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) {
        return float4(pow(color, float3(1.0f / gamma)), inColor.a);
    }

    const float d = 1. - startCompression;
    float newPeak = 1. - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
    color = lerp(color, newPeak * float3(1, 1, 1), g);

    return float4(pow(color, float3(1.0f / gamma)), inColor.a);
}