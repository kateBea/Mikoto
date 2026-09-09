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

using namespace mikoto;

struct FSInput {
    float2 texCoord : ATTRIBUTE0;
    float3 color : ATTRIBUTE1;
};

struct FSOutput {
    float4 color : SV_Target;
};

struct DrawParams {
    int TextureIndex;
    int SamplerIndex;
};

#if defined(MKT_SUPPORT_VULKAN)
MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_TEXTURE_SRV(Texture2D) textures;
#endif

MKT_DECLARE_PUSH_CONSTANT(DrawParams) uParams;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

#if defined(MKT_SUPPORT_VULKAN) || defined(MKT_SUPPORT_D3D12)
    if (uParams.TextureIndex != -1) {
        o.color = textures[uParams.TextureIndex].Sample(samplers[uParams.SamplerIndex], input.texCoord);
    } else {
        o.color = float4(input.color, 1.0);
    }
#endif

    return o;
}