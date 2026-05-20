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
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

static const uint kBGTypeSkybox = 0;
static const uint kBGTypePrefilter = 1;

struct FSInput {
    float3 Direction : TEXCOORD0;
};

struct FSOutput {
    float4 Color : SV_Target0;
};

struct DrawParams {
    uint CameraBufferID;
    uint BasicSamplerID;

    uint SkyboxCubeRtID;
    uint PrefilterCubeRtID;

    uint BackGroundType; // Prefilter map, Original skybox, clear color

    uint MaxMipLevel;
    float Exposure;
    float Gamma;
};

MKT_DECLARE_PUSH_CONSTANT(DrawParams) Parameters;

MKT_DECLARE_SAMPLERS Samplers;
MKT_DECLARE_TEXTURE_SRV(TextureCube) Textures;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    SamplerState sampler = Samplers[Parameters.BasicSamplerID];
    TextureCube texture;

    if (Parameters.BackGroundType == kBGTypeSkybox) {
        texture = Textures[Parameters.SkyboxCubeRtID];
    } else if (Parameters.BackGroundType == kBGTypePrefilter) {
        texture = Textures[Parameters.PrefilterCubeRtID];
    }

    const float blur = 1;
    float3 color;

    if (Parameters.MaxMipLevel > blur) {
        color = texture.SampleLevel(sampler, input.Direction, blur).rgb;
    }
    else {
        color = texture.Sample(sampler, input.Direction).rgb;
    }

    o.Color = float4(color, 1.0);
    return o;
}