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

#include "base/base.slang"

struct FSInput {
    float2 texCoord : TEXCOORD0;
    float3 color : COLOR0;
};

struct FSOutput {
    float4 color : SV_Target;
};

struct HelloTextureUniformBuffer {
    int TextureIndex;
};

[[vk::binding(0, TEXTURES_SET_INDEX)]]
Sampler2D g_BindlessTextures[];

[[vk::push_constant]] 
HelloTextureUniformBuffer u_MetaData;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;
    if (u_MetaData.TextureIndex != INVALID_TEXTURE_INDEX) {
        o.color = g_BindlessTextures[u_MetaData.TextureIndex].Sample(input.texCoord);
    } else {
        o.color = float4(input.color, 1.0);
    }

    return o;
}