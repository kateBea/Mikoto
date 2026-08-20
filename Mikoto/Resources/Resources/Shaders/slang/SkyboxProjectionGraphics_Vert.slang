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

struct VSOutput {
    float2 UV : TEXCOORD0;
    float4 Position : SV_Position;
};

[shader("vertex")]
VSOutput main(uint vertexID: SV_VertexID) {
    VSOutput o;

    float2 uv = float2((vertexID << 1) & 2, (vertexID) & 2);

    o.UV = uv;
    o.Position = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);

#ifdef MKT_SUPPORT_VULKAN
    o.Position.y *= -1.0f;
#endif

    return o;
} 