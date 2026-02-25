//    Copyright 2025 ケイト
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

struct SkyBoxParameters {
    float Exposure;
    float Gamma;
    float MaxReflectionLOD;
    float MaxMipLevel;
    int IsSkyboxActive;
};

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

float3 GetNormalFromMap(Sampler2D normalMap, float2 uv, float3 N, float3 worldPos) {
    float3 tangentNormal = normalMap.Sample(uv).xyz * 2.0 - 1.0;

    float3 Q1 = ddx(worldPos);
    float3 Q2 = ddy(worldPos);
    float2 st1 = ddx(uv);
    float2 st2 = ddy(uv);

    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, normalize(N));

    return normalize(mul(TBN, tangentNormal));
}

float LinearDepth(float depth, float2 planes) {
    float z = depth * 2.0 - 1.0;

    return (2.0 * planes.x * planes.y) /
           (planes.y + planes.x - z * (planes.y - planes.x));
}