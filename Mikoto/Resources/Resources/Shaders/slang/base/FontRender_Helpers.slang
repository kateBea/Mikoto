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

#pragma once

struct FontRenderParams {
    float4x4 Projection;
    float4x4 View;
    float4x4 Model;

    float4 OutlineColor;
    float  OutlineWidth;

    float4 Position;
    float4 Size;
    float4 Color;

    float2 TextureCoords[4];

    uint   TextureIndex;
};

float Median(float3 v) {
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

float ScreenPxRange3D(Sampler2D msdf, float2 uv) {
    float pxRange = 6.0;

    float textureWidth;
    float textureHeight;
    float levelCount;
    msdf.GetDimensions(0, textureWidth, textureHeight, levelCount);

    float2 unitRange = float2(pxRange) / float2(textureWidth, textureHeight);
    float2 screenTexSize = float2(1.0) / fwidth(uv);

    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

float ScreenPxRange(Sampler2D msdf, float2 uv) {
    float textureWidth;
    float textureHeight;
    float levelCount;
    msdf.GetDimensions(0, textureWidth, textureHeight, levelCount);

    float2 unitRange = float2(2.0) / float2(textureWidth, textureHeight);
    float2 screenTexSize = float2(1.0) / fwidth(uv);

    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

float ScreenPxRange3D(Texture2D msdf, SamplerState sampler, float2 uv) {
    float pxRange = 6.0;

    float textureWidth;
    float textureHeight;
    float levelCount;
    msdf.GetDimensions(0, textureWidth, textureHeight, levelCount);

    float2 unitRange = float2(pxRange) / float2(textureWidth, textureHeight);
    float2 screenTexSize = float2(1.0) / fwidth(uv);

    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

float ScreenPxRange(Texture2D msdf, SamplerState sampler, float2 uv) {
    float textureWidth;
    float textureHeight;
    float levelCount;

    // Atlas textures only have one mip level, so we can ignore the levelCount output
    msdf.GetDimensions(0, textureWidth, textureHeight, levelCount);

    float2 unitRange = float2(2.0) / float2(textureWidth, textureHeight);
    float2 screenTexSize = float2(1.0) / fwidth(uv);

    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

// Vertex description is defined here, I will not upload it from
// CPU as it is very little and not expected to change often
// Estructura simplificada que coincide exactamente con tu CPU
struct TextVertexDescription {
    float3 mPosition;
    uint mTextureIndex;
};

// Datos estáticos del Quad embebidos directamente en el shader
static const TextVertexDescription kTextVertices[4] = {
    { float3(0.0f, 0.0f, 0.0f), 0 },
    { float3(1.0f, 0.0f, 0.0f), 0 }, // Cambia el índice si usas texturas distintas por vértice
    { float3(1.0f, 1.0f, 0.0f), 0 },
    { float3(0.0f, 1.0f, 0.0f), 0 }
};

// Índices para el orden de dibujo del Quad
static const uint kTextIndices[6] = {
    0, 1, 2, // Primer triángulo
    2, 3, 0  // Segundo triángulo
};