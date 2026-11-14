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

// Project the preview panorama before using SceneRenderer's mip/prefilter shaders.
#include "base/Thumbnail_Helpers.slang"
#include "base/PreviewCubeDirection.slang"

[shader("fragment")]
float4 main(float2 uv : TEXCOORD0) : SV_Target0 {
    return float4(PreviewEnvironment(PreviewCubeDirection(uv, preview.mDrawIndex)), 1);
}
