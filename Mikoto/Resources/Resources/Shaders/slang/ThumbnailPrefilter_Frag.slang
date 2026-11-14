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

#include "base/Thumbnail_Helpers.slang"

#include "base/PreviewCubeDirection.slang"
#include "base/CubemapPrefilter_Helpers.slang"

[shader("fragment")]
float4 main(float2 uv : TEXCOORD0) : SV_Target0 {
    float3 direction = PreviewCubeDirection(uv, preview.mDrawIndex);
    float roughness = float(preview.mColor) / float(preview.mPrefilterMipCount - 1);
    return float4(PrefilterEnvMap(previewCubes[preview.mPrefilter],
        previewSamplers[preview.mSampler], direction, roughness, 1024), 1);
}
