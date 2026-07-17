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
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"

struct VSOutput {
    float2 TexCoord : TEXCOORD0;

    float3 Near : TEXCOORD1;
    float3 Far : TEXCOORD2;

    float4 PositionH : SV_Position;
};

struct DrawParams {
    uint mCameraDataID;

    float4 OuterSquareColor;
    float4 InnerSquareColor;

    float4 XAxisColor;
    float4 ZAxisColor;

    float OuterSquareWidth;
    float InnerSquareWidth;

    float XAxisWidth;
    float ZAxisWidth;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

float3 NDCToWorld(float4 v) {
    CameraInfo camera = buffers[parameters.mCameraDataID].Read<CameraInfo>(0, 0);
    const float4 P = mul(camera.InverseViewProjection, v);
    return P.xyz / P.w;
}

[shader("vertex")]
VSOutput main(uint vertexIndex: SV_VertexID) {
    VSOutput o;

    // https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/
    // vkCmdDraw(cb, 3, 1, 0, 0);
    o.TexCoord = float2(float((vertexIndex << 1) & 2), float(vertexIndex & 2));
    o.PositionH = float4(o.TexCoord * 2.0f - 1.0f, 0.0f, 1.0f);

    const float2 P = o.TexCoord * 2.0 - 1.0;
    o.Near = NDCToWorld(float4(P, 0.0, 1.0));
    o.Far = NDCToWorld(float4(P, 1.0, 1.0));

    return o;
}