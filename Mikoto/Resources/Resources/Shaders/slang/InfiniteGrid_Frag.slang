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

// =========================================================================
// ref: https://github.com/skaarj1989/SupernovaEngine
// http://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/
// =========================================================================

#include "base/Base.slang"
#include "base/Math_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"

struct FSInput {
    float2 TexCoord : TEXCOORD0;

    float3 Near : TEXCOORD1;
    float3 Far : TEXCOORD2;

    float4 FragPos : SV_Position;
};

struct FSOutput {
    float4 Color : SV_Target0;
    float FragDepth : SV_Depth;
};

struct DrawParams {
    uint mCameraDataID;

    float4 mOuterSquareColor;
    float4 mInnerSquareColor;

    float4 mXAxisColor;
    float4 mZAxisColor;

    float mOuterSquareWidth;
    float mInnerSquareWidth;

    float mXAxisWidth;
    float mZAxisWidth;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_PUSH_CONSTANT(DrawParams) parameters;

float4 Grid(float3 fragPos, float scale, float thickness, float3 lineColor) {
    const float2 coords = fragPos.xz * scale;
    const float2 fd = fwidth(coords);
    const float2 lattice = abs(fract(coords - 0.5) - 0.5) / (fd * thickness);
    const float line = min(lattice.x, lattice.y);

    float4 color = float4(lineColor, 1.0 - min(line, 1.0));

    const float axisWidthX = parameters.mXAxisWidth;
    const float axisWidthZ = parameters.mZAxisWidth;

    // X axis
    const float minX = min(fd.x, 1.0);
    if (fragPos.x > -axisWidthX * minX && fragPos.x < axisWidthX * minX) {
        color = float4(parameters.mXAxisColor);
    }

    // Z axis
    const float minZ = min(fd.y, 1.0);
    if (fragPos.z > -axisWidthZ * minZ && fragPos.z < axisWidthZ * minZ) {
        color = float4(parameters.mZAxisColor);
    }
    return color;
}

float4 WorldToClip(float4x4 viewProj, float4 v) {
    return mul(viewProj, v); 
}

float LinearizeDepth(float n, float f, float sampledDepth) {
    const float z = sampledDepth;
    return n * f / (f + z * (n - f));
}

float3 WorldToNDC(float4 v, float4x4 camViewProj) {
    const float4 P = WorldToClip(camViewProj, v);
    return P.xyz / P.w;
}

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    CameraInfo camera = buffers[parameters.mCameraDataID].Read<CameraInfo>(0, 0);

    const float t = -input.Near.y / (input.Far.y - input.Near.y);
    const float3 fragPos = input.Near + t * (input.Far - input.Near);

    const float3 ndc = float3(WorldToNDC(float4(fragPos, 1.0), mul(camera.mProjection, camera.mViewMatrix)));
    if (ndc.z >= 1.0) discard;

    o.FragDepth = ndc.z - EPSILON;

    o.Color = (Grid(fragPos, 0.01, 0.5, parameters.mInnerSquareColor.xyz) + Grid(fragPos, 0.5, 1, parameters.mOuterSquareColor.xyz)) * float(t > 0.0);

    const float linearDepth = LinearizeDepth(camera.mPlanes.x, camera.mPlanes.y, ndc.z) / camera.mPlanes.y;
    o.Color.a *= max(0.0, (0.5 - linearDepth));

    return o;
}