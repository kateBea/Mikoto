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

DECL_BINDING(0,0) Texture2D<float4> inputImage;
DECL_BINDING(0,1) RWTexture2D<float4> outputImage;

[numthreads(8,8,1)]
[shader("compute")]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint2 pixel = tid.xy;

    float4 color = inputImage.Load(int3(pixel,0));

    outputImage[pixel] = color;
}