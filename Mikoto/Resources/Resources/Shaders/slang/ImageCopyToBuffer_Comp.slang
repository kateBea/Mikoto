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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

struct Parameters {
    uint imageWidth;
    uint imageHeight;

    uint inputImageID;
    uint outputBufferID;

    // format info
}

MKT_DECLARE_STRUCTURED_UAV buffers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;

MKT_DECLARE_PUSH_CONSTANT(Parameters) parameters;

[numthreads(8,8,1)]
[shader("compute")]
void main(uint3 tid: SV_DispatchThreadID) {
    // Assumes 4x4 RGBA32F

    uint2 pixel = tid.xy;

    uint width;
    uint height;

    textures[parameters.inputImageID].GetDimensions(width, height);

    if (pixel.x >= width || pixel.y >= height)
        return;

    uint linearIndex = pixel.y * width + pixel.x;
    float4 color = textures[parameters.outputBufferID].Load(int3(pixel, 0));

    buffers[parameters.outputBufferID].Store4(linearIndex * sizeof(float4), asuint(color));
}