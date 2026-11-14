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
#include "base/Bloom_Helpers.slang"

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    PreviewScene scene = ( ( BufferReference<PreviewScene> )preview.mScene )[0];
    if ( scene.mBloom.y == 0.0 ) {
        return float4( 0 );
    }
    float3 color;
    if ( preview.mMode == 4 ) {
        color = BloomDownsample( previewTextures[preview.mColor], previewSamplers[preview.mClampSampler], uv,
                                 scene.mBloom.x, preview.mDrawIndex == 0 );
    } else {
        color = BloomUpsample( previewTextures[preview.mColor], previewTextures[preview.mBloom],
                               previewSamplers[preview.mClampSampler], uv, scene.mBloom.z, scene.mBloom.w );
    }
    return float4( color, 1 );
}
